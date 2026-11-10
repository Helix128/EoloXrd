#include <cassert>
#include "../rtc_startup_stubs/Arduino.h"
#include "../rtc_startup_stubs/RTClib.h"

#define EOLO_CONFIG_LEGACY_H
#define I2C_BUS_H
#define EOLO_DIRECT_DS3231_H
namespace EoloConfig { constexpr const char *rtcTimeServerUrl = "http://test"; }
static void (*duringRtcWrite)() = nullptr;

class I2CBus {
public:
    class Guard { public: bool acquired() const { return true; } };
    static I2CBus &getInstance() { static I2CBus bus; return bus; }
    bool applyProfile() { return true; }
};

class DirectDS3231 {
public:
    enum class TimeReadResult { Ok, Invalid, TransportError };
    bool begin(bool &lost, DateTime &time, bool &valid) {
        if (!fakeRtc.available) return false;
        lost = fakeRtc.powerLost;
        time = DateTime(fakeRtc.time);
        valid = fakeRtc.valid;
        return true;
    }
    bool readTime(DateTime &time) const {
        if (!fakeRtc.available || !fakeRtc.valid || !fakeRtc.verifySucceeds) return false;
        time = DateTime(fakeRtc.time);
        return true;
    }
    TimeReadResult readTimeResult(DateTime &time) const {
        if (!fakeRtc.available) return TimeReadResult::TransportError;
        if (!fakeRtc.valid) return TimeReadResult::Invalid;
        time = DateTime(fakeRtc.time);
        return TimeReadResult::Ok;
    }
    bool readTemperature(float &value) const { value = 20; return true; }
    bool adjust(const DateTime &value) const {
        ++fakeRtc.writes;
        if (duringRtcWrite) {
            auto callback = duringRtcWrite;
            duringRtcWrite = nullptr;
            callback();
        }
        if (!fakeRtc.writeSucceeds) return false;
        fakeRtc.time = value.unixtime();
        fakeRtc.valid = true;
        return true;
    }
};

#include "../../src/Common/Board/RTCManager.h"

static RTCManager *concurrentRtc = nullptr;
static constexpr uint32_t newRequestTime = 1800000000UL;
static void enqueueNewerAdjustment() {
    assert(concurrentRtc->adjust(DateTime(newRequestTime)));
}

int main() {
    constexpr uint32_t previousTime = 1780000000UL;
    fakeRtc = {};
    RTCManager intact;
    assert(intact.begin());
    assert(intact.now().unixtime() == previousTime);
    assert(fakeRtc.writes == 0);
    assert(!intact.lostPower());

    fakeRtc.available = false;
    assert(!intact.poll());
    assert(fakeRtc.writes == 0);
    assert(intact.now().unixtime() == previousTime);

    fakeRtc = {};
    fakeRtc.powerLost = true;
    RTCManager flagged;
    assert(flagged.begin());
    assert(flagged.now().unixtime() == previousTime);
    assert(fakeRtc.writes == 0);
    assert(flagged.lostPower());

    fakeRtc = {};
    fakeRtc.available = false;
    RTCManager unavailable;
    assert(!unavailable.begin());
    assert(fakeRtc.writes == 0);

    fakeRtc = {};
    fakeRtc.valid = false;
    fakeRtc.time = 0;
    RTCManager invalid;
    assert(invalid.begin());
    assert(fakeRtc.writes == 1);
    assert(invalid.lostPower());
    assert(invalid.now().unixtime() == RTCManager::fallbackTime().unixtime());

    fakeRtc = {};
    RTCManager invalidDuringPoll;
    assert(invalidDuringPoll.begin());
    fakeRtc.valid = false;
    fakeRtc.time = 0;
    assert(invalidDuringPoll.poll());
    assert(fakeRtc.writes == 1);
    assert(invalidDuringPoll.lostPower());

    fakeRtc = {};
    fakeRtc.valid = false;
    fakeRtc.time = 0;
    fakeRtc.writeSucceeds = false;
    RTCManager failedRecovery;
    assert(!failedRecovery.begin());
    assert(fakeRtc.writes == 1);

    fakeRtc = {};
    fakeMillis = 0;
    RTCManager retry;
    assert(retry.begin());
    constexpr uint32_t requestedTime = 1790000000UL;
    assert(retry.adjust(DateTime(requestedTime)));
    fakeRtc.writeSucceeds = false;
    assert(retry.poll());
    assert(fakeRtc.writes == 1 && fakeRtc.time == previousTime);
    fakeRtc.writeSucceeds = true;
    fakeMillis = 5000;
    assert(retry.poll());
    assert(fakeRtc.writes == 2 && fakeRtc.time == requestedTime + 5);
    assert(retry.poll());
    assert(fakeRtc.writes == 2);

    fakeRtc = {};
    fakeMillis = 0;
    RTCManager verificationRetry;
    assert(verificationRetry.begin());
    assert(verificationRetry.adjust(DateTime(requestedTime)));
    fakeRtc.verifySucceeds = false;
    assert(verificationRetry.poll());
    assert(fakeRtc.writes == 1);
    fakeRtc.verifySucceeds = true;
    assert(verificationRetry.poll());
    assert(fakeRtc.writes == 2 && fakeRtc.time == requestedTime);

    fakeRtc = {};
    fakeMillis = 0;
    RTCManager concurrent;
    assert(concurrent.begin());
    assert(concurrent.adjust(DateTime(requestedTime)));
    concurrentRtc = &concurrent;
    duringRtcWrite = enqueueNewerAdjustment;
    assert(concurrent.poll());
    assert(fakeRtc.writes == 1 && fakeRtc.time == requestedTime);
    assert(concurrent.poll());
    assert(fakeRtc.writes == 2 && fakeRtc.time == newRequestTime);
    assert(concurrent.poll());
    assert(fakeRtc.writes == 2);

    fakeRtc = {};
    RTCManager newerAfterFailure;
    assert(newerAfterFailure.begin());
    assert(newerAfterFailure.adjust(DateTime(requestedTime)));
    fakeRtc.writeSucceeds = false;
    concurrentRtc = &newerAfterFailure;
    duringRtcWrite = enqueueNewerAdjustment;
    assert(newerAfterFailure.poll());
    fakeRtc.writeSucceeds = true;
    assert(newerAfterFailure.poll());
    assert(fakeRtc.writes == 2 && fakeRtc.time == newRequestTime);
}
