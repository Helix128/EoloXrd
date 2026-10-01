#ifndef RTC_H
#define RTC_H

#include <Arduino.h>
#include "Wire.h"
#include <RTClib.h>
#include <sys/time.h>
#include "../../Variants/Legacy.h"
#include "I2CBus.h"
#include "DirectDS3231.h"
#include <Eolo/Core/Time/RtcTimeParser.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>


// Manejo del RTC (DS3231 via RTClib)
class RTCManager
{
private:
    RTC_DS3231 rtc;
    DirectDS3231 directRtc;
    SemaphoreHandle_t _cacheMutex = nullptr;
    SemaphoreHandle_t _requestMutex = nullptr;
    uint32_t _cachedUnix = 0;
    uint32_t _cachedAtMs = 0;
    bool _cacheValid = false;
    float _cachedTemperatureC = -1.0f;
    uint32_t _temperatureCachedAtMs = 0;
    bool _temperatureCacheValid = false;
    bool _adjustPending = false;
    DateTime _pendingAdjust;

    void updateCache(const DateTime &value) {
        if (_cacheMutex)
            xSemaphoreTake(_cacheMutex, pdMS_TO_TICKS(5));
        _cachedUnix = value.unixtime();
        _cachedAtMs = millis();
        _cacheValid = isValid(value);
        if (_cacheMutex)
            xSemaphoreGive(_cacheMutex);
    }

    void updateTemperatureCache(float value) {
        if (_cacheMutex)
            xSemaphoreTake(_cacheMutex, pdMS_TO_TICKS(5));
        _cachedTemperatureC = value;
        _temperatureCachedAtMs = millis();
        _temperatureCacheValid = isfinite(value) && value >= -55.0f && value <= 125.0f;
        if (_cacheMutex)
            xSemaphoreGive(_cacheMutex);
    }

public:
    enum class BackupBatteryStatus : uint8_t
    {
        Unknown,
        Good,
        Check
    };

    static constexpr uint32_t MaxNtpAdjustDiffSeconds = 120;
    static constexpr const char *DefaultTimeServerUrl = EoloConfig::rtcTimeServerUrl;
    static constexpr size_t TimeServerResponseBufferSize = 4096;

    std::atomic_bool ok{false};
    std::atomic_bool powerLost{false};

    static DateTime compileTime()
    {
        return DateTime(__DATE__, __TIME__);
    }

    static DateTime fallbackTime()
    {
        uint32_t compileUnix = compileTime().unixtime();
        return DateTime(compileUnix + (millis() / 1000UL));
    }

    RTCManager()
        : _cacheMutex(xSemaphoreCreateMutex()),
          _requestMutex(xSemaphoreCreateMutex()) {}

    bool begin()
    {
        LOG_LN("Iniciando RTC...");
        ok = true;

        bool rtcReady = false;
        bool timeValid = false;
        DateTime initial;
        I2CBus &bus = I2CBus::getInstance();
#if EOLO_I2C_DIRECT_DRIVERS
        bool initialPowerLost = false;
        rtcReady = directRtc.begin(initialPowerLost, initial, timeValid);
        powerLost = initialPowerLost;
#else
        // RTClib/Adafruit BusIO vuelve a ejecutar Wire.begin() y realiza su
        // propio sondeo. Primero registramos el ACK/NACK mediante I2CBus;
        // así un DS3231 ausente no genera warnings repetidos ni queda fuera
        // de las estadísticas del worker.
        if (bus.probe(0x68) == I2CBus::Result::Ok) {
            I2CBus::Guard guard;
            if (guard.acquired()) {
                rtcReady = rtc.begin(&Wire);
                if (rtcReady)
                    powerLost = rtc.lostPower();
            }
        }
        bus.applyProfile();
#endif
        ok = rtcReady;

        // Si el DS3231 responde con hora válida, inicializamos la caché.
        // Si responde pero con fecha inválida/OSF, recuperamos el chip físico
        // escribiendo el fallbackTime para que oscile en fecha moderna.
        if (rtcReady && timeValid && isValid(initial))
        {
            updateCache(initial);
        }
        else if (rtcReady)
        {
            DateTime recovered = fallbackTime();
            LOG_F("RTC perdió la memoria/hora al arrancar; recuperando chip físico con %s\n",
                  recovered.timestamp().c_str());
            adjustHardware(recovered);
            powerLost = true;
        }
#if !EOLO_I2C_DIRECT_DRIVERS
        if (rtcReady)
            poll();
#endif

    
        return ok.load();
    }

    bool isPresent() const { return ok.load(); }
    bool hasValidTime() { return isValid(now()); }

    // Lectura cacheada: nunca inicia una transacción I2C desde el publicador.
    bool getTemperature(float &temperatureC) const
    {
        if (_cacheMutex && xSemaphoreTake(_cacheMutex, pdMS_TO_TICKS(5)) != pdTRUE)
            return false;
        bool valid = _temperatureCacheValid &&
                     (uint32_t)(millis() - _temperatureCachedAtMs) <= 5000UL;
        temperatureC = valid ? _cachedTemperatureC : -1.0f;
        if (_cacheMutex)
            xSemaphoreGive(_cacheMutex);
        return valid;
    }

    // Devuelve la fecha/hora actual en formato "YYYY-MM-DD HH:MM:SS"
    String getTimeString()
    {
        DateTime now = this->now();
        if (!isValid(now))
            return String("0000-00-00 00:00:00");
        char buf[20];
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
                 now.year(), now.month(), now.day(),
                 now.hour(), now.minute(), now.second());
        return String(buf);
    }

    String getTimeString(DateTime time)
    {
        if (!isValid(time))
            return String("0000-00-00 00:00:00");

        char buf[20];
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
                 time.year(), time.month(), time.day(),
                 time.hour(), time.minute(), time.second());
        return String(buf);
    }

    // Opcional: devuelve el objeto DateTime para uso avanzado
    DateTime now()
    {
        if (_cacheMutex &&
            xSemaphoreTake(_cacheMutex, pdMS_TO_TICKS(5)) != pdTRUE)
            return fallbackTime();
        if (!_cacheValid) {
            if (_cacheMutex)
                xSemaphoreGive(_cacheMutex);
            return fallbackTime();
        }
        uint32_t unixTime = _cachedUnix + ((millis() - _cachedAtMs) / 1000UL);
        bool valid = _cacheValid;
        if (_cacheMutex)
            xSemaphoreGive(_cacheMutex);
        return valid ? DateTime(unixTime) : fallbackTime();
    }

    // Única lectura periódica del RTC físico; el resto del firmware usa now().
    bool poll()
    {
        if (!ok.load())
            return false;

        bool hasPendingAdjust = false;
        DateTime pendingAdjust;
        if (_requestMutex &&
            xSemaphoreTake(_requestMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            hasPendingAdjust = _adjustPending;
            pendingAdjust = _pendingAdjust;
            _adjustPending = false;
            xSemaphoreGive(_requestMutex);
        }
        if (hasPendingAdjust && !adjustHardware(pendingAdjust))
            LOG_LN("No se pudo aplicar el ajuste RTC encolado");

        DateTime current;
        float temperatureC = -1.0f;
        bool temperatureValid = false;
#if EOLO_I2C_DIRECT_DRIVERS
        if (!directRtc.readTime(current)) {
            ok = false;
            return false;
        }
        temperatureValid = directRtc.readTemperature(temperatureC);
#else
        I2CBus::Guard guard;
        if (!guard.acquired())
            return false;
        current = rtc.now();
        temperatureC = rtc.getTemperature();
        temperatureValid = isfinite(temperatureC) && temperatureC >= -55.0f && temperatureC <= 125.0f;
#endif
        if (temperatureValid)
            updateTemperatureCache(temperatureC);

        // Hora inválida (por ejemplo 2000-01-01 tras OSF o pérdida de batería).
        // Si el chip físico perdió la hora, se auto-recupera escribiendo inmediatamente
        // la fecha corregida (fallbackTime = compileTime + uptime) en los registros del chip
        // para que empiece a oscilar en una fecha moderna, y se mantiene el aviso powerLost
        // para que los diagnósticos y pantallas indiquen que la pila debe ser revisada.
        if (!isValid(current))
        {
            DateTime recovered = fallbackTime();
            LOG_F("RTC perdió la memoria/hora (OSF detectado); recuperando chip físico con %s\n",
                  recovered.timestamp().c_str());
            adjustHardware(recovered);
            powerLost = true;
            return true;
        }

        ok = true;
        updateCache(current);
        return true;
    }

    bool adjust(const DateTime &time)
    {
        if (!ok.load() || !isValid(time))
            return false;

        // Todo ajuste se encola, incluso si el solicitante es una tarea de
        // comunicaciones en core 0. Así el worker I2C sigue siendo el único
        // contexto que toca el DS3231 y la llamada nunca espera una
        // transacción física.
        if (!_requestMutex ||
            xSemaphoreTake(_requestMutex, 0) != pdTRUE)
            return false;
        _pendingAdjust = time;
        _adjustPending = true;
        xSemaphoreGive(_requestMutex);
        return true;
    }

private:
    bool adjustHardware(const DateTime &time)
    {
        if (!ok.load())
            return false;
        if (!isValid(time))
            return false;

        DateTime written;
#if EOLO_I2C_DIRECT_DRIVERS
        if (!directRtc.adjust(time))
            return false;
        delay(5);
        if (!directRtc.readTime(written))
            return false;
#else
        I2CBus::Guard guard;
        if (!guard.acquired())
            return false;
        rtc.adjust(time);
        delay(5);
        written = rtc.now();
#endif
        uint32_t targetUnix = time.unixtime();
        uint32_t writtenUnix = written.unixtime();
        uint32_t diff = targetUnix > writtenUnix ? targetUnix - writtenUnix : writtenUnix - targetUnix;
        bool verified = diff <= 2;

        if (verified)
        {
            powerLost = false;
            updateCache(written);
        }
        else
        {
            LOG_F("Fallo verificacion ajuste RTC: objetivo=%s leido=%s diff=%lu s\n",
                  time.timestamp().c_str(),
                  written.timestamp().c_str(),
                  (unsigned long)diff);
        }

        return verified;
    }

public:

    bool lostPower() const
    {
        return powerLost;
    }

    BackupBatteryStatus getBackupBatteryStatus()
    {
        DateTime current = now();
        if (!isValid(current))
            return BackupBatteryStatus::Unknown;
        if (powerLost || !ok.load())
            return BackupBatteryStatus::Check;

        return BackupBatteryStatus::Good;
    }

    const char *getBackupBatteryStatusText()
    {
        switch (getBackupBatteryStatus())
        {
        case BackupBatteryStatus::Good:
            return "OK";
        case BackupBatteryStatus::Check:
            return "REVISAR";
        default:
            return "N/D";
        }
    }

    bool isValid(const DateTime &time) const
    {
        return time.year() >= 2024 && time.year() <= 2099 &&
               time.month() >= 1 && time.month() <= 12 &&
               time.day() >= 1 && time.day() <= 31;
    }

    // Base segura para una edición manual cuando el RTC responde pero aún
    // conserva su fecha de fábrica/OSF. Evita que la UI intente guardar 2000.
    DateTime timeForManualAdjustment()
    {
        DateTime current = now();
        return isValid(current) ? current : DateTime(__DATE__, __TIME__);
    }

    static bool parseDateTimeString(const char *text, DateTime &time)
    {
        RtcDateTime parsed;
        if (!RtcTimeParser::parseDateTime(text, parsed))
            return false;
        time = DateTime(parsed.unixTime);
        return true;
    }

    static bool fromUnixWithOffset(uint32_t unixUtc, int32_t offsetSeconds, DateTime &time)
    {
        RtcDateTime parsed;
        if (!RtcTimeParser::fromUnixWithOffset(unixUtc, offsetSeconds, parsed))
            return false;
        time = DateTime(parsed.unixTime);
        return true;
    }

    bool applyTimeServerResponse(const char *response, const char *source = DefaultTimeServerUrl)
    {
        if (!ok)
        {
            LOG_LN("RTC no disponible; se omite aplicar hora HTTP");
            return false;
        }

        DateTime localTime;
        int32_t offsetSeconds = 0;
        if (!parseTimeServerResponse(response, localTime, offsetSeconds))
        {
            return false;
        }

        return applyNetworkTime(localTime, offsetSeconds, source);
    }

    static bool parseTimeServerResponse(const char *json, DateTime &localTime, int32_t &offsetSeconds)
    {
        RtcDateTime parsed;
        if (!RtcTimeParser::parseTimeServerResponse(json, parsed, offsetSeconds))
            return false;
        localTime = DateTime(parsed.unixTime);
        return true;
    }

private:
    bool applyNetworkTime(const DateTime &localTime, int32_t offsetSeconds, const char *source)
    {
        DateTime current = now();
        bool invalidRtc = !isValid(current);
        uint32_t currentUnix = current.unixtime();
        uint32_t targetUnix = localTime.unixtime();
        uint32_t diff = currentUnix > targetUnix ? currentUnix - targetUnix : targetUnix - currentUnix;

        if (lostPower() || invalidRtc || diff > MaxNtpAdjustDiffSeconds)
        {
            if (!adjust(localTime))
            {
                LOG_F("No se pudo guardar hora HTTP en RTC fisico: %s\n",
                      localTime.timestamp().c_str());
                return false;
            }

            LOG_F("RTC ajustado desde HTTP %s: %s (UTC%+ld)\n",
                  source,
                  localTime.timestamp().c_str(),
                  (long)(offsetSeconds / 3600));
            return true;
        }

        LOG_F("RTC ya esta sincronizado con HTTP; diferencia %lu s\n", (unsigned long)diff);
        return true;
    }
};
#endif
