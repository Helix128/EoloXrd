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


// Manejo del RTC DS3231 con lecturas y escrituras I2C verificables.
class RTCManager
{
private:
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
    uint32_t _pendingAdjustAtMs = 0;
    uint32_t _adjustRevision = 0;

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
        bool initialPowerLost = false;
        rtcReady = directRtc.begin(initialPowerLost, initial, timeValid);
        powerLost = initialPowerLost;
        ok = rtcReady;

        // Una hora válida se conserva incluso con OSF: la advertencia sigue
        // activa hasta un ajuste explícito, pero no se reemplaza por la fecha
        // de compilación. Una fecha inválida sí necesita recuperación.
        if (rtcReady && timeValid && isValid(initial))
        {
            updateCache(initial);
        }
        else if (rtcReady)
        {
            DateTime recovered = fallbackTime();
            LOG_F("RTC con fecha inválida al arrancar; recuperando chip físico con %s\n",
                  recovered.timestamp().c_str());
            if (!adjustHardware(recovered)) {
                LOG_LN("No se pudo recuperar el RTC; se reintentará la inicialización");
                ok = false;
                return false;
            }
            powerLost = true;
        }
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
        uint32_t pendingAdjustAtMs = 0;
        uint32_t pendingRevision = 0;
        if (_requestMutex &&
            xSemaphoreTake(_requestMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            hasPendingAdjust = _adjustPending;
            pendingAdjust = _pendingAdjust;
            pendingAdjustAtMs = _pendingAdjustAtMs;
            pendingRevision = _adjustRevision;
            xSemaphoreGive(_requestMutex);
        }
        if (hasPendingAdjust) {
            // Mantener vigente la hora solicitada aunque un fallo requiera
            // varios intentos, sin bloquear a quien encola un ajuste nuevo.
            DateTime target(pendingAdjust.unixtime() + (millis() - pendingAdjustAtMs) / 1000UL);
            if (adjustHardware(target)) {
                if (_requestMutex && xSemaphoreTake(_requestMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                    if (_adjustRevision == pendingRevision)
                        _adjustPending = false;
                    xSemaphoreGive(_requestMutex);
                }
            } else {
                LOG_LN("No se pudo aplicar el ajuste RTC encolado; se reintentará");
            }
        }

        DateTime current;
        float temperatureC = -1.0f;
        bool temperatureValid = false;
        const DirectDS3231::TimeReadResult readResult = directRtc.readTimeResult(current);
        if (readResult == DirectDS3231::TimeReadResult::TransportError) {
            ok = false;
            return false;
        }
        temperatureValid = directRtc.readTemperature(temperatureC);
        if (temperatureValid)
            updateTemperatureCache(temperatureC);

        // Un calendario inválido no identifica su causa. Se usa la fecha de
        // compilación como referencia aproximada y se deja activa la advertencia.
        if (readResult == DirectDS3231::TimeReadResult::Invalid || !isValid(current))
        {
            DateTime recovered = fallbackTime();
            LOG_F("RTC con fecha inválida; recuperando chip físico con %s\n",
                  recovered.timestamp().c_str());
            if (!adjustHardware(recovered)) {
                LOG_LN("No se pudo recuperar el RTC; se reintentará la inicialización");
                ok = false;
                return false;
            }
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
        _pendingAdjustAtMs = millis();
        ++_adjustRevision;
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
        if (!directRtc.adjust(time))
            return false;
        delay(5);
        if (!directRtc.readTime(written))
            return false;
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
