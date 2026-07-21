#ifndef EOLO_CORE_TIME_RTC_TIME_PARSER_H
#define EOLO_CORE_TIME_RTC_TIME_PARSER_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct RtcDateTime
{
    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
    uint32_t unixTime = 0;
};

class RtcTimeParser
{
    static bool isLeapYear(int year)
    {
        return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    }

    static uint8_t daysInMonth(int year, int month)
    {
        static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        return month == 2 && isLeapYear(year) ? 29 : days[month - 1];
    }

    static bool toUnix(int year, int month, int day, int hour, int minute, int second, uint32_t &unixTime)
    {
        if (year < 1970 || year > 2099 || month < 1 || month > 12 ||
            day < 1 || day > daysInMonth(year, month) || hour < 0 || hour > 23 ||
            minute < 0 || minute > 59 || second < 0 || second > 59)
            return false;

        uint64_t days = 0;
        for (int y = 1970; y < year; ++y)
            days += isLeapYear(y) ? 366U : 365U;
        for (int m = 1; m < month; ++m)
            days += daysInMonth(year, m);

        uint64_t result = ((days + static_cast<uint32_t>(day - 1)) * 24U + hour) * 3600U +
                          static_cast<uint32_t>(minute) * 60U + second;
        if (result > UINT32_MAX)
            return false;
        unixTime = static_cast<uint32_t>(result);
        return true;
    }

    static bool fromUnix(uint32_t unixTime, RtcDateTime &time)
    {
        uint32_t days = unixTime / 86400U;
        uint32_t remainder = unixTime % 86400U;
        int year = 1970;
        while (days >= static_cast<uint32_t>(isLeapYear(year) ? 366 : 365))
        {
            days -= isLeapYear(year) ? 366U : 365U;
            ++year;
        }
        int month = 1;
        while (days >= daysInMonth(year, month))
        {
            days -= daysInMonth(year, month);
            ++month;
        }

        time.year = static_cast<uint16_t>(year);
        time.month = static_cast<uint8_t>(month);
        time.day = static_cast<uint8_t>(days + 1U);
        time.hour = static_cast<uint8_t>(remainder / 3600U);
        remainder %= 3600U;
        time.minute = static_cast<uint8_t>(remainder / 60U);
        time.second = static_cast<uint8_t>(remainder % 60U);
        time.unixTime = unixTime;
        return year >= 2024 && year <= 2099;
    }

public:
    static bool parseFixedInt(const char *text, size_t start, size_t len, int &value)
    {
        if (text == nullptr)
            return false;
        int result = 0;
        for (size_t i = 0; i < len; ++i)
        {
            const char c = text[start + i];
            if (c < '0' || c > '9')
                return false;
            result = result * 10 + (c - '0');
        }
        value = result;
        return true;
    }

    static bool parseDateTime(const char *text, RtcDateTime &time)
    {
        if (text == nullptr || strlen(text) != 19 || text[4] != '-' || text[7] != '-' ||
            (text[10] != ' ' && text[10] != 'T') || text[13] != ':' || text[16] != ':')
            return false;

        int year, month, day, hour, minute, second;
        if (!parseFixedInt(text, 0, 4, year) || !parseFixedInt(text, 5, 2, month) ||
            !parseFixedInt(text, 8, 2, day) || !parseFixedInt(text, 11, 2, hour) ||
            !parseFixedInt(text, 14, 2, minute) || !parseFixedInt(text, 17, 2, second) ||
            year < 2024 || year > 2099)
            return false;

        uint32_t unixTime = 0;
        if (!toUnix(year, month, day, hour, minute, second, unixTime))
            return false;
        time = {static_cast<uint16_t>(year), static_cast<uint8_t>(month), static_cast<uint8_t>(day),
                static_cast<uint8_t>(hour), static_cast<uint8_t>(minute), static_cast<uint8_t>(second), unixTime};
        return true;
    }

    static bool fromUnixWithOffset(uint32_t unixUtc, int32_t offsetSeconds, RtcDateTime &time)
    {
        if (offsetSeconds < -12 * 3600 || offsetSeconds > 14 * 3600)
            return false;
        const int64_t localUnix = static_cast<int64_t>(unixUtc) + offsetSeconds;
        return localUnix >= 0 && localUnix <= UINT32_MAX && fromUnix(static_cast<uint32_t>(localUnix), time);
    }

    static bool parseTimeServerResponse(const char *json, RtcDateTime &localTime, int32_t &offsetSeconds)
    {
        int64_t utcUnix = 0;
        int64_t offset = 0;
        if (!extractJsonInt(json, "unix", utcUnix) || !extractJsonInt(json, "utc_offset", offset) ||
            utcUnix < 0 || utcUnix > UINT32_MAX || offset < -12 * 3600 || offset > 14 * 3600 ||
            !fromUnixWithOffset(static_cast<uint32_t>(utcUnix), static_cast<int32_t>(offset), localTime))
            return false;
        offsetSeconds = static_cast<int32_t>(offset);
        return true;
    }

    static bool extractJsonInt(const char *json, const char *key, int64_t &value)
    {
        if (json == nullptr || key == nullptr)
            return false;
        char pattern[40];
        const int written = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
        if (written < 0 || written >= static_cast<int>(sizeof(pattern)))
            return false;
        const char *pos = strstr(json, pattern);
        if (pos == nullptr || (pos = strchr(pos + written, ':')) == nullptr)
            return false;
        do { ++pos; } while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n');
        bool negative = false;
        if (*pos == '-') { negative = true; ++pos; }
        if (*pos < '0' || *pos > '9')
            return false;
        int64_t result = 0;
        while (*pos >= '0' && *pos <= '9')
        {
            if (result > (INT64_MAX - (*pos - '0')) / 10)
                return false;
            result = result * 10 + (*pos++ - '0');
        }
        value = negative ? -result : result;
        return true;
    }
};

#endif
