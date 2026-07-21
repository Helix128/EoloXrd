#ifndef RTC_H
#define RTC_H

#include <Arduino.h>
#include "Wire.h"
#include <RTClib.h>
#include <sys/time.h>
#include "../Config/Legacy.h"
#include "I2CBus.h"
#include <Eolo/Core/Time/RtcTimeParser.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>

#if BAREBONES == false

// Manejo del RTC (DS3231 via RTClib)
class RTCManager
{
private:
    RTC_DS3231 rtc;
    SemaphoreHandle_t _cacheMutex = nullptr;
    SemaphoreHandle_t _requestMutex = nullptr;
    uint32_t _cachedUnix = 0;
    uint32_t _cachedAtMs = 0;
    bool _cacheValid = false;
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

public:
    enum class BackupBatteryStatus : uint8_t
    {
        Unknown,
        Good,
        Check
    };

    static constexpr uint32_t MaxNtpAdjustDiffSeconds = 120;
    static constexpr const char *DefaultTimeServerUrl = RTC_TIME_SERVER_URL;
    static constexpr size_t TimeServerResponseBufferSize = 4096;

    std::atomic_bool ok{false};
    std::atomic_bool powerLost{false};

    RTCManager()
        : _cacheMutex(xSemaphoreCreateMutex()),
          _requestMutex(xSemaphoreCreateMutex()) {}

    bool begin()
    {
        LOG_LN("Iniciando RTC...");
        ok = true;

        bool rtcReady = false;
        I2CBus &bus = I2CBus::getInstance();
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
        ok = rtcReady;

        if (ok.load())
            poll();

    
#if CHECK_SENSORS
        testRTC();
#endif
        return ok.load();
    }

    void testRTC()
    {
        if (!ok.load())
        {
            LOG_LN("RTC no está inicializado correctamente");
        }
        LOG_LN("Probando RTC...");
        DateTime now = this->now();
        LOG_OUT("Fecha y hora actual: ");
        LOG_OUT(now.year(), DEC);
        LOG_OUT('/');
        LOG_OUT(now.month(), DEC);
        LOG_OUT('/');
        LOG_OUT(now.day(), DEC);
        LOG_OUT(' ');
        LOG_OUT(now.hour(), DEC);
        LOG_OUT(':');
        LOG_OUT(now.minute(), DEC);
        LOG_OUT(':');
        LOG_OUT(now.second(), DEC);
        LOG_OUT_LN();
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
            return DateTime();
        if (!_cacheValid) {
            if (_cacheMutex)
                xSemaphoreGive(_cacheMutex);
            return DateTime();
        }
        uint32_t unixTime = _cachedUnix + ((millis() - _cachedAtMs) / 1000UL);
        bool valid = _cacheValid;
        if (_cacheMutex)
            xSemaphoreGive(_cacheMutex);
        return valid ? DateTime(unixTime) : DateTime();
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

        I2CBus::Guard guard;
        if (!guard.acquired()) {
            ok = false;
            return false;
        }
        DateTime current = rtc.now();
        if (!isValid(current)) {
            ok = false;
            return false;
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

        I2CBus::Guard guard;
        if (!guard.acquired())
            return false;
        rtc.adjust(time);
        delay(5);

        DateTime written = rtc.now();
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
#else
// Manejo del RTC interno del ESP32
class RTCManager
{
public:
    enum class BackupBatteryStatus : uint8_t
    {
        Unknown,
        Good,
        Check
    };

    static constexpr uint32_t MaxNtpAdjustDiffSeconds = 120;
    static constexpr const char *DefaultTimeServerUrl = RTC_TIME_SERVER_URL;
    static constexpr size_t TimeServerResponseBufferSize = 4096;

    bool ok = true;
    bool powerLost = false;

    bool begin()
    {
        // El RTC interno no requiere inicialización especial; siempre retorna true.
        ok = true;
#if CHECK_SENSORS
        testRTC();
#endif
        return ok;
    }

    void testRTC()
    {
        if (!ok)
        {
            LOG_LN("RTC no está inicializado correctamente");
        }
        LOG_LN("Probando RTC...");
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        LOG_OUT("Fecha y hora actual: ");
        LOG_F("%04d/%02d/%02d %02d:%02d:%02d\n",
                      timeinfo.tm_year + 1900,
                      timeinfo.tm_mon + 1,
                      timeinfo.tm_mday,
                      timeinfo.tm_hour,
                      timeinfo.tm_min,
                      timeinfo.tm_sec);
    }

    // Devuelve la fecha/hora actual en formato "YYYY-MM-DD HH:MM:SS"
    String getTimeString()
    {
        if (!ok)
            return String("0000-00-00 00:00:00 RTCERROR");

        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        char buf[20];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                 timeinfo.tm_year + 1900,
                 timeinfo.tm_mon + 1,
                 timeinfo.tm_mday,
                 timeinfo.tm_hour,
                 timeinfo.tm_min,
                 timeinfo.tm_sec);
        return String(buf);
    }

    // Opcional: devuelve el objeto tm para uso avanzado
    struct DateTime now()
    {
        struct tm timeinfo;
        time_t now;
        time(&now);
        localtime_r(&now, &timeinfo);

        DateTime dt = DateTime(timeinfo.tm_sec + timeinfo.tm_min * 60 + timeinfo.tm_hour * 3600 +
                               timeinfo.tm_mday * 86400 + (timeinfo.tm_mon + timeinfo.tm_year * 365) * 86400);
        return dt;
    }

    bool adjust(const DateTime &time)
    {
        if (!isValid(time))
            return false;

        struct timeval tv;
        tv.tv_sec = time.unixtime();
        tv.tv_usec = 0;
        bool adjusted = settimeofday(&tv, nullptr) == 0;
        if (adjusted)
            powerLost = false;
        return adjusted;
    }

    bool lostPower() const
    {
        return powerLost;
    }

    BackupBatteryStatus getBackupBatteryStatus()
    {
        return BackupBatteryStatus::Unknown;
    }

    const char *getBackupBatteryStatusText()
    {
        return "N/D";
    }

    bool isValid(const DateTime &time) const
    {
        return time.year() >= 2024 && time.year() <= 2099 &&
               time.month() >= 1 && time.month() <= 12 &&
               time.day() >= 1 && time.day() <= 31;
    }

    String getTimeString(DateTime time)
    {
        if (!ok)
            return String("0000-00-00 00:00:00 RTCERROR");

        char buf[20];
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
                 time.year(), time.month(), time.day(),
                 time.hour(), time.minute(), time.second());
        return String(buf);
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
        DateTime localTime;
        int32_t offsetSeconds = 0;
        if (!parseTimeServerResponse(response, localTime, offsetSeconds))
        {
            return false;
        }

        (void)localTime;
        (void)offsetSeconds;
        (void)source;
        return true;
    }

    static bool parseTimeServerResponse(const char *json, DateTime &localTime, int32_t &offsetSeconds)
    {
        RtcDateTime parsed;
        if (!RtcTimeParser::parseTimeServerResponse(json, parsed, offsetSeconds))
            return false;
        localTime = DateTime(parsed.unixTime);
        return true;
    }

};
#endif
#endif
