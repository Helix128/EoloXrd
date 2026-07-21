#ifndef EOLO_CORE_POWER_BATTERY_PROTOCOL_H
#define EOLO_CORE_POWER_BATTERY_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

struct BatteryProtocolPacket {
    uint8_t activeMosfet = 0;
    float dcVoltage = 0.0f;
    float batt0Voltage = 0.0f;
    float batt1Voltage = 0.0f;
    uint8_t activeMosfetEcho = 0;
};

class BatteryProtocol {
public:
    static constexpr size_t FrameSize = 14;
    static_assert(sizeof(float) == 4, "El protocolo de bateria requiere float IEEE-754 de 4 bytes");

    static bool decode(const uint8_t *raw, size_t len, BatteryProtocolPacket &output) {
        if (raw == nullptr || len != FrameSize)
            return false;

        BatteryProtocolPacket candidate;
        candidate.activeMosfet = raw[0];
        memcpy(&candidate.dcVoltage, raw + 1, sizeof(float));
        memcpy(&candidate.batt0Voltage, raw + 5, sizeof(float));
        memcpy(&candidate.batt1Voltage, raw + 9, sizeof(float));
        candidate.activeMosfetEcho = raw[13];

        if (candidate.activeMosfet > 3 ||
            candidate.activeMosfet != candidate.activeMosfetEcho)
            return false;
        if (!validVoltage(candidate.dcVoltage) ||
            !validVoltage(candidate.batt0Voltage) ||
            !validVoltage(candidate.batt1Voltage))
            return false;
        output = candidate;
        return true;
    }

private:
    static bool validVoltage(float value) {
        return isfinite(value) && value >= 0.0f && value <= 40.0f;
    }
};

#endif
