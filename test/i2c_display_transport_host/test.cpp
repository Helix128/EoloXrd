#include <cassert>
#include <cstdint>

#define I2C_BUS_H
struct I2CBus {
    enum class Result { Ok, BusError };
    bool deny = false;
    unsigned locks = 0;
    unsigned unlocks = 0;
    unsigned failures = 0;

    static I2CBus &getInstance() { static I2CBus bus; return bus; }
    void begin() {}
    bool lock(uint32_t) { ++locks; return !deny; }
    void unlock() { ++unlocks; }
    static Result resultFromTransmissionError(uint8_t error, uint32_t) {
        return error ? Result::BusError : Result::Ok;
    }
    void recordResult(uint8_t, Result result, uint32_t) {
        if (result != Result::Ok) ++failures;
    }
};

#include "../../src/Common/Board/I2CDisplayTransport.h"

using namespace I2CDisplayTransport;

static void transfer() {
    static u8x8_t device;
    uint8_t data[] = {0x40, 0xAA, 0x55};
    byteCallback(&device, U8X8_MSG_BYTE_START_TRANSFER, 0, nullptr);
    byteCallback(&device, U8X8_MSG_BYTE_SEND, sizeof(data), data);
    byteCallback(&device, U8X8_MSG_BYTE_END_TRANSFER, 0, nullptr);
}

static void reset() {
    I2CBus::getInstance() = {};
    Wire = {};
    yields = 0;
    skipping = dropReported = frameActive = frameFailed = shortWrite = lost = false;
    dropCount = consecutiveFailures = 0;
}

static void testValidFrame() {
    reset();
    beginFrame();
    transfer();
    transfer();
    assert(endFrame());
    assert(Wire.starts == 2 && Wire.ends == 2);
    assert(I2CBus::getInstance().locks == 2 && I2CBus::getInstance().unlocks == 2);
    assert(yields == 2 && dropCount == 0);
}

static void testFailureAbortsFrame(bool lockFailure, bool nack, bool shortBuffer) {
    reset();
    auto &bus = I2CBus::getInstance();
    beginFrame();
    transfer(); // e.g. successful page command
    bus.deny = lockFailure;
    Wire.error = nack ? 2 : 0;
    Wire.shortWrite = shortBuffer;
    transfer(); // failed data or address command
    const unsigned starts = Wire.starts;
    const unsigned locks = bus.locks;
    bus.deny = false;
    Wire.error = 0;
    Wire.shortWrite = false;
    transfer(); // must not touch the bus for the rest of this frame
    assert(!endFrame());
    assert(Wire.starts == starts && bus.locks == locks);
    assert(dropCount == 1);
    assert(bus.unlocks == Wire.starts);
    assert(bus.failures == (lockFailure ? 0U : 1U));

    beginFrame();
    transfer();
    assert(endFrame());
    assert(Wire.starts == starts + 1);
}

static void testLostAndRecovery() {
    reset();
    lost = true;
    beginFrame();
    transfer();
    assert(!endFrame());
    assert(Wire.starts == 0 && I2CBus::getInstance().locks == 0);
    markRecovered();
    beginFrame();
    transfer();
    assert(endFrame());
    assert(Wire.starts == 1);
}

static void testConsecutiveFailuresAcrossFrames() {
    reset();
    Wire.error = 2;
    for (unsigned i = 0; i < LostAfterFailures; ++i) {
        beginFrame();
        transfer();
        assert(!endFrame());
    }
    assert(lost && Wire.starts == LostAfterFailures);
    beginFrame();
    transfer();
    assert(!endFrame() && Wire.starts == LostAfterFailures);
    markRecovered();
    Wire.error = 0;
    beginFrame();
    transfer();
    assert(endFrame() && !lost);
}

int main() {
    testValidFrame();
    testFailureAbortsFrame(true, false, false);
    testFailureAbortsFrame(false, true, false);
    testFailureAbortsFrame(false, false, true);
    testLostAndRecovery();
    testConsecutiveFailuresAcrossFrames();
}
