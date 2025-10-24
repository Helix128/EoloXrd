#ifndef RTC_H
#define RTC_H

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include "../Config.h"

#if BAREBONES == false
// Manejo del RTC (DS3231 via RTClib)
class RTCManager
{
private:
    RTC_DS3231 rtc;

public:
    bool ok = false;
    // Inicializa I2C y el RTC. Devuelve true si está presente.
    bool begin()
    {
        Wire.begin();
        ok = true;
        if (!rtc.begin())
        {
            ok = false;
        }

        if (rtc.lostPower())
        {
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        
        #if CHECK_SENSORS
        testRTC();
        #endif
        return ok;
    }

    void testRTC(){
        if(!ok){
            Serial.println("RTC no está inicializado correctamente");
        }
        Serial.println("Probando RTC...");
        DateTime now = rtc.now();
        Serial.print("Fecha y hora actual: ");
        Serial.print(now.year(), DEC);
        Serial.print('/');
        Serial.print(now.month(), DEC);
        Serial.print('/');
        Serial.print(now.day(), DEC);
        Serial.print(' ');
        Serial.print(now.hour(), DEC);
        Serial.print(':');
        Serial.print(now.minute(), DEC);
        Serial.print(':');
        Serial.print(now.second(), DEC);
        Serial.println();
    }
    // Devuelve la fecha/hora actual en formato "YYYY-MM-DD HH:MM:SS"
    String getTimeString()
    {
        if(!ok)
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
        if(!ok)
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
        if(!ok)
            return DateTime();
        return rtc.now();
    }
};
#else
// Manejo del RTC interno del ESP32
class RTCManager
{
public:
    bool ok = true;

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
            Serial.println("RTC no está inicializado correctamente");
        }
        Serial.println("Probando RTC...");
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        Serial.print("Fecha y hora actual: ");
        Serial.printf("%04d/%02d/%02d %02d:%02d:%02d\n",
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

        DateTime dt = DateTime(timeinfo.tm_sec+timeinfo.tm_min*60+timeinfo.tm_hour*3600+
                                  timeinfo.tm_mday*86400+(timeinfo.tm_mon+timeinfo.tm_year*365)*86400);
        return dt;
    }
};
#endif
#endif