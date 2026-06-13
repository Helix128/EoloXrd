#ifndef EOLO_CORE_TIME_RTC_TIME_PARSER_H
#define EOLO_CORE_TIME_RTC_TIME_PARSER_H

#include <RTClib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

class RtcTimeParser
{
public:
    static bool parseFixedInt(const char *text, size_t start, size_t len, int &value)
    {
        if (text == nullptr)
            return false;

        int result = 0;
        for (size_t i = 0; i < len; i++)
        {
            char c = text[start + i];
            if (c < '0' || c > '9')
                return false;
            result = result * 10 + (c - '0');
        }
        value = result;
        return true;
    }

    static bool parseDateTime(const char *text, DateTime &time)
    {
        if (text == nullptr || strlen(text) != 19)
            return false;

        if (text[4] != '-' || text[7] != '-' ||
            (text[10] != ' ' && text[10] != 'T') ||
            text[13] != ':' || text[16] != ':')
            return false;

        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;

        if (!parseFixedInt(text, 0, 4, year) ||
            !parseFixedInt(text, 5, 2, month) ||
            !parseFixedInt(text, 8, 2, day) ||
            !parseFixedInt(text, 11, 2, hour) ||
            !parseFixedInt(text, 14, 2, minute) ||
            !parseFixedInt(text, 17, 2, second))
            return false;

        if (year < 2024 || year > 2099 ||
            month < 1 || month > 12 ||
            day < 1 || day > 31 ||
            hour < 0 || hour > 23 ||
            minute < 0 || minute > 59 ||
            second < 0 || second > 59)
            return false;

        DateTime parsed(year, month, day, hour, minute, second);
        if (parsed.year() != year || parsed.month() != month || parsed.day() != day ||
            parsed.hour() != hour || parsed.minute() != minute || parsed.second() != second)
            return false;

        time = parsed;
        return true;
    }

    static bool fromUnixWithOffset(uint32_t unixUtc, int32_t offsetSeconds, DateTime &time)
    {
        if (offsetSeconds < -12 * 3600 || offsetSeconds > 14 * 3600)
            return false;

        int64_t localUnix = (int64_t)unixUtc + offsetSeconds;
        if (localUnix < 0 || localUnix > UINT32_MAX)
            return false;

        DateTime parsed((uint32_t)localUnix);
        if (parsed.year() < 2024 || parsed.year() > 2099)
            return false;

        time = parsed;
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

        if (utcUnix < 0 || utcUnix > UINT32_MAX)
            return false;

        DateTime parsed;
        if (!fromUnixWithOffset((uint32_t)utcUnix, (int32_t)offset, parsed))
            return false;

        localTime = parsed;
        offsetSeconds = (int32_t)offset;
        return true;
    }

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

namespace RTCParse
{
    inline bool parseFixedInt(const char *text, size_t start, size_t len, int &value)
    {
        return RtcTimeParser::parseFixedInt(text, start, len, value);
    }

    inline bool parseDateTime(const char *text, DateTime &time)
    {
        return RtcTimeParser::parseDateTime(text, time);
    }

    inline bool fromUnixWithOffset(uint32_t unixUtc, int32_t offsetSeconds, DateTime &time)
    {
        return RtcTimeParser::fromUnixWithOffset(unixUtc, offsetSeconds, time);
    }
}

#endif
