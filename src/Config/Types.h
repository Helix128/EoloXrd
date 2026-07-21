#ifndef EOLO_CONFIG_TYPES_H
#define EOLO_CONFIG_TYPES_H

#include <stdint.h>
#include <Eolo/Core/Flow/FlowMotorController.h>

namespace EoloConfig
{
enum class ModemPowerMode : uint8_t
{
    AlwaysOn = 1,
    OnDemand = 2
};

struct BoardConfig
{
    uint32_t i2cClockHz;
    uint32_t displaySpiClockHz;
    uint8_t attinyAddress;
    bool motorPwmInverted;
    uint8_t motorPwmResolutionBits;
    int motorRampStep;
    uint32_t motorRampIntervalMs;
    ModemPowerMode modemPowerMode;
    int displayCsPin;
    int displayDcPin;
    int displayResetPin;
    int displayClockPin;
    int displayMosiPin;
    int flowPidInitialPwm;
    uint32_t i2cWarmupMs;
    bool encoderInverted;
};

struct SafetyConfig
{
    float motorOverheatHighC;
    float motorOverheatLowC;
    uint32_t motorOverheatLogIntervalMs;
};

struct ServiceDefaults
{
    const char *modemApn;
    const char *rtcTimeServerUrl;
};

struct SensorConfig
{
    float afm07FlowDivisor;
    float droneTargetFlowLpm;
};

constexpr uint32_t kMinute = 60UL;
constexpr uint32_t kHour = 3600UL;
constexpr uint32_t kDay = 86400UL;
}

#endif
