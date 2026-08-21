#ifndef EOLO_DIRECT_DS3231_H
#define EOLO_DIRECT_DS3231_H

#include <Arduino.h>
#include <RTClib.h>
#include "I2CBus.h"

// Transporte DS3231 sin RTClib/Adafruit BusIO. Todas las transferencias pasan
// por I2CBus, por lo que se pueden contabilizar y fallar explícitamente.
class DirectDS3231 {
public:
    static constexpr uint8_t Address = 0x68;

    bool begin(bool &lostPower, DateTime &time, bool &timeValid) {
        if (I2CBus::getInstance().probe(Address) != I2CBus::Result::Ok)
            return false;
        uint8_t status = 0;
        if (!readRegister(0x0F, &status, 1))
            return false;
        lostPower = (status & 0x80U) != 0;
        if (!readTime(time))
            return false;
        timeValid = isCalendarValid(time);
        return true;
    }

    bool readTime(DateTime &time) const {
        uint8_t reg = 0x00;
        uint8_t raw[7] = {};
        if (!I2CBus::getInstance().writeThenRead(Address, &reg, 1, raw, sizeof(raw), false))
            return false;

        uint8_t second = 0, minute = 0, hour = 0, day = 0, month = 0, year = 0;
        if (!decodeBcd(raw[0] & 0x7F, second) || !decodeBcd(raw[1] & 0x7F, minute) ||
            !decodeHour(raw[2], hour) || !decodeBcd(raw[4] & 0x3F, day) ||
            !decodeBcd(raw[5] & 0x1F, month) || !decodeBcd(raw[6], year))
            return false;

        time = DateTime(2000U + year, month, day, hour, minute, second);
        return isCalendarValid(time);
    }

    bool adjust(const DateTime &time) const {
        if (!isCalendarValid(time))
            return false;
        uint8_t buffer[8] = {
            0x00,
            encodeBcd(time.second()), encodeBcd(time.minute()), encodeBcd(time.hour()),
            encodeBcd(time.dayOfTheWeek() == 0 ? 7 : time.dayOfTheWeek()),
            encodeBcd(time.day()), encodeBcd(time.month()),
            encodeBcd((uint8_t)(time.year() - 2000U))
        };
        if (!I2CBus::getInstance().writeBytes(Address, buffer, sizeof(buffer), false))
            return false;

        uint8_t status = 0;
        if (!readRegister(0x0F, &status, 1))
            return false;
        status &= (uint8_t)~0x80U;
        uint8_t statusWrite[2] = {0x0F, status};
        return I2CBus::getInstance().writeBytes(Address, statusWrite, sizeof(statusWrite), false);
    }

    bool readLostPower(bool &lostPower) const {
        uint8_t status = 0;
        if (!readRegister(0x0F, &status, 1))
            return false;
        lostPower = (status & 0x80U) != 0;
        return true;
    }

    bool readTemperature(float &temperatureC) const {
        uint8_t reg = 0x11;
        uint8_t raw[2] = {};
        if (!I2CBus::getInstance().writeThenRead(Address, &reg, 1, raw, sizeof(raw), false))
            return false;
        int16_t quarterDegrees = (int16_t)((int8_t)raw[0]) * 4 + ((raw[1] >> 6) & 0x03);
        temperatureC = quarterDegrees / 4.0f;
        return isfinite(temperatureC) && temperatureC >= -55.0f && temperatureC <= 125.0f;
    }

    static bool isCalendarValid(const DateTime &time) {
        uint16_t year = time.year();
        uint8_t month = time.month();
        uint8_t day = time.day();
        if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 ||
            time.hour() > 23 || time.minute() > 59 || time.second() > 59)
            return false;
        static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        uint8_t limit = days[month - 1];
        if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
            limit = 29;
        return day <= limit;
    }

private:
    bool readRegister(uint8_t reg, uint8_t *out, size_t len) const {
        return I2CBus::getInstance().writeThenRead(Address, &reg, 1, out, len, false);
    }

    static bool decodeBcd(uint8_t value, uint8_t &out) {
        if ((value & 0x0FU) > 9 || ((value >> 4) & 0x0FU) > 9)
            return false;
        out = (uint8_t)(((value >> 4) * 10U) + (value & 0x0FU));
        return true;
    }

    static bool decodeHour(uint8_t value, uint8_t &out) {
        if ((value & 0x40U) == 0)
            return decodeBcd(value & 0x3FU, out) && out <= 23;
        uint8_t hour12 = 0;
        if (!decodeBcd(value & 0x1FU, hour12) || hour12 < 1 || hour12 > 12)
            return false;
        bool pm = (value & 0x20U) != 0;
        out = (uint8_t)((hour12 % 12U) + (pm ? 12U : 0U));
        return true;
    }

    static uint8_t encodeBcd(uint8_t value) {
        return (uint8_t)(((value / 10U) << 4) | (value % 10U));
    }
};

#endif
