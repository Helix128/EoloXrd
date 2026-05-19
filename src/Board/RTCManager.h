#ifndef RTC_H
#define RTC_H

#include <Arduino.h>
#include "Wire.h"
#include <RTClib.h>
#include "../Config.h"
#include "ESPJob.h"

#if BAREBONES == false

// Manejo del RTC (DS3231 via RTClib)
class RTCManager
{
private:
    RTC_DS3231 rtc;

public:
    enum class BackupBatteryStatus : uint8_t
    {
        Unknown,
        Good,
        Check
    };

    static constexpr uint32_t MaxNtpAdjustDiffSeconds = 120;
    static constexpr const char *DefaultTimeServerUrl = "http://time.cmasccp.cl/";
    static constexpr size_t TimeServerResponseBufferSize = 4096;

    bool ok = false;
    bool powerLost = false;

    bool begin()
    {
        LOG_LN("Iniciando RTC...");
        ok = true;

        const int TIMEOUT_MS = 5000;
        int startTime = millis();
    
        if (!rtc.begin())
        {
            ok = false;
        }

        powerLost = ok && rtc.lostPower();

    
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
        DateTime now = rtc.now();
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
        if (!ok)
            return String("0000-00-00 00:00:00");

        DateTime now = rtc.now();
        char buf[20];
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
                 now.year(), now.month(), now.day(),
                 now.hour(), now.minute(), now.second());
        return String(buf);
    }

    String getTimeString(DateTime time)
    {
        if (!ok)
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
        if (!ok)
            return DateTime();
        return rtc.now();
    }

    bool adjust(const DateTime &time)
    {
        if (!ok)
            return false;
        if (!isValid(time))
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

    bool lostPower() const
    {
        return powerLost;
    }

    BackupBatteryStatus getBackupBatteryStatus()
    {
        if (!ok)
            return BackupBatteryStatus::Unknown;

        DateTime current = rtc.now();
        if (powerLost || !isValid(current))
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
        int64_t utcUnix = 0;
        if (!extractJsonInt(json, "unix", utcUnix))
        {
            LOG_LN("Respuesta de tiempo sin campo unix");
            return false;
        }

        int64_t offset = 0;
        if (!extractJsonInt(json, "utc_offset", offset))
        {
            LOG_LN("Respuesta de tiempo sin campo utc_offset");
            return false;
        }

        if (offset < -12 * 3600 || offset > 14 * 3600)
        {
            LOG_F("utc_offset invalido: %lld\n", (long long)offset);
            return false;
        }

        int64_t localUnix = utcUnix + offset;
        if (localUnix < 0 || localUnix > UINT32_MAX)
        {
            LOG_F("unix local fuera de rango: %lld\n", (long long)localUnix);
            return false;
        }

        DateTime parsed = DateTime((uint32_t)localUnix);
        if (parsed.year() < 2024 || parsed.year() > 2099)
        {
            LOG_F("Anio de servidor fuera de rango: %u\n", parsed.year());
            return false;
        }

        localTime = parsed;
        offsetSeconds = (int32_t)offset;
        return true;
    }

private:
    static bool extractJsonInt(const char *json, const char *key, int64_t &value)
    {
        if (json == nullptr || key == nullptr)
            return false;

        char pattern[40];
        int written = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
        if (written < 0 || written >= (int)sizeof(pattern))
            return false;

        const char *pos = strstr(json, pattern);
        if (pos == nullptr)
            return false;

        pos = strchr(pos + written, ':');
        if (pos == nullptr)
            return false;
        pos++;

        while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n')
            pos++;

        bool negative = false;
        if (*pos == '-')
        {
            negative = true;
            pos++;
        }

        if (*pos < '0' || *pos > '9')
            return false;

        int64_t result = 0;
        while (*pos >= '0' && *pos <= '9')
        {
            result = result * 10 + (*pos - '0');
            pos++;
        }

        value = negative ? -result : result;
        return true;
    }

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
    static constexpr const char *DefaultTimeServerUrl = "http://time.cmasccp.cl/";
    static constexpr size_t TimeServerResponseBufferSize = 4096;

    bool ok = true;
    bool powerLost = false;

    // Inicializa el RTC interno del ESP32. Siempre devuelve true.
    bool begin()
    {
        // El RTC interno no requiere inicialización especial.
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
        (void)time;
        return false;
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
        int64_t utcUnix = 0;
        if (!extractJsonInt(json, "unix", utcUnix))
            return false;

        int64_t offset = 0;
        if (!extractJsonInt(json, "utc_offset", offset))
            return false;

        if (offset < -12 * 3600 || offset > 14 * 3600)
            return false;

        int64_t localUnix = utcUnix + offset;
        if (localUnix < 0 || localUnix > UINT32_MAX)
            return false;

        DateTime parsed = DateTime((uint32_t)localUnix);
        if (parsed.year() < 2024 || parsed.year() > 2099)
            return false;

        localTime = parsed;
        offsetSeconds = (int32_t)offset;
        return true;
    }

private:
    static bool extractJsonInt(const char *json, const char *key, int64_t &value)
    {
        if (json == nullptr || key == nullptr)
            return false;

        char pattern[40];
        int written = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
        if (written < 0 || written >= (int)sizeof(pattern))
            return false;

        const char *pos = strstr(json, pattern);
        if (pos == nullptr)
            return false;

        pos = strchr(pos + written, ':');
        if (pos == nullptr)
            return false;
        pos++;

        while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n')
            pos++;

        bool negative = false;
        if (*pos == '-')
        {
            negative = true;
            pos++;
        }

        if (*pos < '0' || *pos > '9')
            return false;

        int64_t result = 0;
        while (*pos >= '0' && *pos <= '9')
        {
            result = result * 10 + (*pos - '0');
            pos++;
        }

        value = negative ? -result : result;
        return true;
    }
};
#endif
#endif
