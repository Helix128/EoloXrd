#ifndef RTC_H
#define RTC_H

#include <Arduino.h>
#include "Wire.h"
#include <RTClib.h>
#include "../Config.h"
#include "../Data/ClockSettings.h"
#include "ESPJob.h"

#ifdef FEATURE_MODEM
#include "Modem.h"
#endif

#if BAREBONES == false

// Manejo del RTC (DS3231 via RTClib)
class RTCManager
{
private:
    RTC_DS3231 rtc;

public:
    struct NtpServer
    {
        const char *name;
        const char *host;
    };

    static constexpr uint32_t MaxNtpAdjustDiffSeconds = 120;

    bool ok = false;
    bool powerLost = false;

    static NtpServer defaultNtpServer()
    {
        return {"pool.ntp.org", "0.cl.pool.ntp.org"};
    }

    static constexpr size_t NtpServerCount = 4;
    static const NtpServer* defaultNtpServers()
    {
        static const NtpServer servers[NtpServerCount] = {
            {"pool.ntp.org CL 0", "0.cl.pool.ntp.org"},
            {"pool.ntp.org CL 1", "1.cl.pool.ntp.org"},
            {"pool.ntp.org CL 2", "2.cl.pool.ntp.org"},
            {"pool.ntp.org CL 3", "3.cl.pool.ntp.org"},
        };
        return servers;
    }

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

    void adjust(const DateTime &time)
    {
        if (!ok)
            return;
        rtc.adjust(time);
        powerLost = false;
    }

    bool lostPower() const
    {
        return powerLost;
    }

    bool isValid(const DateTime &time) const
    {
        return time.year() >= 2024 && time.year() <= 2099 &&
               time.month() >= 1 && time.month() <= 12 &&
               time.day() >= 1 && time.day() <= 31;
    }

#ifdef FEATURE_MODEM
    bool syncNtp(Modem &modem, const ClockSettings &settings)
    {
        const NtpServer* servers = defaultNtpServers();
        for (size_t i = 0; i < NtpServerCount; i++)
        {
            LOG_F("Intentando NTP %s (%s) [%u/%u]\n", servers[i].name, servers[i].host, (unsigned)(i + 1), (unsigned)NtpServerCount);
            if (syncNtp(modem, settings, servers[i]))
                return true;
        }
        LOG_LN("Todos los servidores NTP fallaron");
        return false;
    }

    bool syncNtp(Modem &modem, const ClockSettings &settings, const NtpServer &server)
    {
        if (!ok)
        {
            LOG_LN("RTC no disponible; se omite sincronizacion NTP");
            return false;
        }

        DateTime utcTime;
        if (!modem.getNetworkTimeUTC(server.host, utcTime))
        {
            LOG_F("No se pudo obtener hora UTC desde NTP %s (%s)\n", server.name, server.host);
            return false;
        }

        uint32_t utcUnix = utcTime.unixtime();
        int32_t offsetSeconds = settings.utcOffsetSeconds();
        uint32_t localUnix = offsetSeconds >= 0
                                 ? utcUnix + (uint32_t)offsetSeconds
                                 : utcUnix - (uint32_t)(-offsetSeconds);
        DateTime localTime = DateTime(localUnix);
        DateTime current = now();
        bool invalidRtc = !isValid(current);
        uint32_t currentUnix = current.unixtime();
        uint32_t targetUnix = localTime.unixtime();
        uint32_t diff = currentUnix > targetUnix ? currentUnix - targetUnix : targetUnix - currentUnix;

        if (lostPower() || invalidRtc || diff > MaxNtpAdjustDiffSeconds)
        {
            adjust(localTime);
            LOG_F("RTC ajustado desde NTP %s: %s (%s)\n",
                  server.name,
                  localTime.timestamp().c_str(),
                  settings.label());
            return true;
        }

        LOG_F("RTC ya esta sincronizado con NTP; diferencia %lu s\n", (unsigned long)diff);
        return true;
    }
#endif
};
#else
// Manejo del RTC interno del ESP32
class RTCManager
{
public:
    struct NtpServer
    {
        const char *name;
        const char *host;
    };

    static constexpr uint32_t MaxNtpAdjustDiffSeconds = 120;

    bool ok = true;
    bool powerLost = false;

    static NtpServer defaultNtpServer()
    {
        return {"pool.ntp.org", "0.cl.pool.ntp.org"};
    }

    static constexpr size_t NtpServerCount = 4;
    static const NtpServer* defaultNtpServers()
    {
        static const NtpServer servers[NtpServerCount] = {
            {"pool.ntp.org CL 0", "0.cl.pool.ntp.org"},
            {"pool.ntp.org CL 1", "1.cl.pool.ntp.org"},
            {"pool.ntp.org CL 2", "2.cl.pool.ntp.org"},
            {"pool.ntp.org CL 3", "3.cl.pool.ntp.org"},
        };
        return servers;
    }

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

    void adjust(const DateTime &time)
    {
        (void)time;
    }

    bool lostPower() const
    {
        return powerLost;
    }

    bool isValid(const DateTime &time) const
    {
        return time.year() >= 2024 && time.year() <= 2099 &&
               time.month() >= 1 && time.month() <= 12 &&
               time.day() >= 1 && time.day() <= 31;
    }

#ifdef FEATURE_MODEM
    bool syncNtp(Modem &modem, const ClockSettings &settings)
    {
        const NtpServer* servers = defaultNtpServers();
        for (size_t i = 0; i < NtpServerCount; i++)
        {
            LOG_F("Intentando NTP %s (%s) [%u/%u]\n", servers[i].name, servers[i].host, (unsigned)(i + 1), (unsigned)NtpServerCount);
            if (syncNtp(modem, settings, servers[i]))
                return true;
        }
        LOG_LN("Todos los servidores NTP fallaron");
        return false;
    }

    bool syncNtp(Modem &modem, const ClockSettings &settings, const NtpServer &server)
    {
        DateTime utcTime;
        if (!modem.getNetworkTimeUTC(server.host, utcTime))
        {
            LOG_F("No se pudo obtener hora UTC desde NTP %s (%s)\n", server.name, server.host);
            return false;
        }

        (void)settings;
        (void)utcTime;
        return true;
    }
#endif
};
#endif
#endif
