#include <cassert>
#include <cstring>
#include "../rtc_startup_stubs/Arduino.h"
#include "../rtc_startup_stubs/RTClib.h"

#define I2C_BUS_H
class I2CBus {
public:
    enum class Result { Ok, Nack };
    bool failTime = false;
    bool failStatus = false;
    uint8_t status = 0;
    uint8_t date[7] = {0x25, 0x34, 0x12, 0x02, 0x10, 0x11, 0x26};
    static I2CBus &getInstance() { static I2CBus bus; return bus; }
    Result probe(uint8_t) { return Result::Ok; }
    bool writeThenRead(uint8_t, const uint8_t *reg, size_t, uint8_t *out, size_t len, bool) {
        if (*reg == 0x0F && len == 1 && !failStatus) { *out = status; return true; }
        if (*reg == 0x00 && len == sizeof(date) && !failTime) {
            std::memcpy(out, date, len);
            return true;
        }
        return false;
    }
    bool writeBytes(uint8_t, const uint8_t *, size_t, bool) { return true; }
};

#include "../../src/Common/Board/DirectDS3231.h"

int main() {
    auto &bus = I2CBus::getInstance();
    DirectDS3231 rtc;
    DateTime time;
    bool lost = false, valid = false;
    assert(rtc.begin(lost, time, valid));
    assert(valid && !lost);
    assert(time.year() == 2026 && time.month() == 11 && time.day() == 10);

    bus.status = 0x80;
    assert(rtc.begin(lost, time, valid));
    assert(lost && valid);

    bus.date[1] = 0x6A;
    assert(rtc.begin(lost, time, valid));
    assert(!valid);
    assert(rtc.readTimeResult(time) == DirectDS3231::TimeReadResult::Invalid);

    bus.date[1] = 0x34;
    bus.date[4] = 0x31;
    bus.date[5] = 0x02;
    assert(rtc.readTimeResult(time) == DirectDS3231::TimeReadResult::Invalid);

    bus.failTime = true;
    assert(!rtc.begin(lost, time, valid));
    assert(rtc.readTimeResult(time) == DirectDS3231::TimeReadResult::TransportError);
    bus.failTime = false;
    bus.failStatus = true;
    assert(!rtc.begin(lost, time, valid));
}
