#ifndef RTC_STARTUP_TEST_RTCLIB_H
#define RTC_STARTUP_TEST_RTCLIB_H
#include "Arduino.h"
#include "Wire.h"

class DateTime {
    uint32_t unixValue;
    uint16_t yearValue;
    uint8_t monthValue, dayValue, hourValue, minuteValue, secondValue;
public:
    DateTime(uint32_t value = 0) : unixValue(value), yearValue(value >= 1704067200UL ? 2026 : 2000),
        monthValue(1), dayValue(1), hourValue(0), minuteValue(0), secondValue(0) {}
    DateTime(const char *, const char *) : DateTime(1767225600UL) {}
    DateTime(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t min, uint8_t s)
        : unixValue(1780000000UL), yearValue(y), monthValue(m), dayValue(d),
          hourValue(h), minuteValue(min), secondValue(s) {}
    uint32_t unixtime() const { return unixValue; }
    uint16_t year() const { return yearValue; }
    uint8_t month() const { return monthValue; }
    uint8_t day() const { return dayValue; }
    uint8_t hour() const { return hourValue; }
    uint8_t minute() const { return minuteValue; }
    uint8_t second() const { return secondValue; }
    uint8_t dayOfTheWeek() const { return 1; }
    String timestamp() const { return String("test"); }
};

struct FakeRtcState {
    bool available = true;
    bool valid = true;
    bool powerLost = false;
    bool writeSucceeds = true;
    bool verifySucceeds = true;
    uint32_t time = 1780000000UL;
    int writes = 0;
};
inline FakeRtcState fakeRtc;

class RTC_DS3231 {
public:
    bool begin(TwoWire *) { return fakeRtc.available; }
    bool lostPower() { return fakeRtc.powerLost; }
    DateTime now() { return DateTime(fakeRtc.time); }
    float getTemperature() { return 20.0f; }
    void adjust(const DateTime &value) {
        ++fakeRtc.writes;
        if (fakeRtc.writeSucceeds) fakeRtc.time = value.unixtime();
    }
};
#endif
