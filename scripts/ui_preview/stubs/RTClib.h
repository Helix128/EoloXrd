// DateTime minimo (UTC) para la vista previa.
#ifndef PREVIEW_RTCLIB_H
#define PREVIEW_RTCLIB_H
#include "Arduino.h"

class DateTime
{
    uint32_t t = 0;
    static long daysFromCivil(long y, unsigned m, unsigned d)
    {
        y -= m <= 2;
        const long era = (y >= 0 ? y : y - 399) / 400;
        const unsigned yoe = (unsigned)(y - era * 400);
        const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + (long)doe - 719468;
    }
    void civil(long &y, unsigned &m, unsigned &d) const
    {
        long z = (long)(t / 86400) + 719468;
        const long era = (z >= 0 ? z : z - 146096) / 146097;
        const unsigned doe = (unsigned)(z - era * 146097);
        const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        y = (long)yoe + era * 400;
        const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const unsigned mp = (5 * doy + 2) / 153;
        d = doy - (153 * mp + 2) / 5 + 1;
        m = mp < 10 ? mp + 3 : mp - 9;
        y += m <= 2;
    }
public:
    enum timestampOpt { TIMESTAMP_FULL, TIMESTAMP_TIME, TIMESTAMP_DATE };
    DateTime() {}
    DateTime(uint32_t unix) : t(unix) {}
    DateTime(int y, int mo, int d, int h, int mi, int s)
    {
        t = (uint32_t)(daysFromCivil(y, mo, 1) * 86400L + (long)(d - 1) * 86400L + h * 3600L + mi * 60L + s);
    }
    uint32_t unixtime() const { return t; }
    uint8_t hour() const { return (t / 3600) % 24; }
    uint8_t minute() const { return (t / 60) % 60; }
    uint8_t second() const { return t % 60; }
    uint16_t year() const { long y; unsigned m, d; civil(y, m, d); return (uint16_t)y; }
    uint8_t month() const { long y; unsigned m, d; civil(y, m, d); return (uint8_t)m; }
    uint8_t day() const { long y; unsigned m, d; civil(y, m, d); return (uint8_t)d; }
    String timestamp(timestampOpt o = TIMESTAMP_FULL) const
    {
        char b[32];
        if (o == TIMESTAMP_TIME) snprintf(b, sizeof(b), "%02d:%02d:%02d", hour(), minute(), second());
        else snprintf(b, sizeof(b), "%04d-%02d-%02dT%02d:%02d:%02d", year(), month(), day(), hour(), minute(), second());
        return String(b);
    }
    DateTime operator+(uint32_t secs) const { return DateTime(t + secs); }
};
#endif
