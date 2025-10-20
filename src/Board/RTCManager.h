#ifndef RTC_H
#define RTC_H

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

// Manejo del RTC (DS3231 via RTClib)
class RTCManager
{
private:
    RTC_DS3231 rtc;

public:
    // Inicializa I2C y el RTC. Devuelve true si está presente.
    bool begin()
    {
        Wire.begin();
        if (!rtc.begin())
        {
            return false;
        }

        if (rtc.lostPower())
        {
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        return true;
    }

    // Devuelve la fecha/hora actual en formato "YYYY-MM-DD HH:MM:SS"
    String getTimeString()
    {
        DateTime now = rtc.now();
        char buf[20];
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
                 now.year(), now.month(), now.day(),
                 now.hour(), now.minute(), now.second());
        return String(buf);
    }

    // Opcional: devuelve el objeto DateTime para uso avanzado
    DateTime now()
    {
        return rtc.now();
    }
};
#endif