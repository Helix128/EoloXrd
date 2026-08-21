#ifndef EOLO_CONFIG_PROFILE_DRON_H
#define EOLO_CONFIG_PROFILE_DRON_H

#include "../Types.h"
#include <Eolo/Core/Flow/FlowMotorController.h>

#define EOLO_DISPLAY_MODEL U8G2_SSD1309_128X64_NONAME2_F_4W_SW_SPI
#define EOLO_DISPLAY_SPI 1
#define EOLO_DISPLAY_HW_SPI 0

namespace EoloConfig::Profile
{
inline constexpr const char kModelName[] = "EOLO Dron";
inline constexpr bool kUseCalibrationSeed = true;

inline constexpr uint32_t kI2cClockHz = 150000UL;
inline constexpr uint32_t kDisplaySpiClockHz = 8000000UL;
inline constexpr uint8_t kAttinyAddress = 8;
inline constexpr bool kMotorPwmInverted = false;
inline constexpr uint8_t kMotorPwmResolutionBits = 11;
inline constexpr int kMotorRampStep = 16;
inline constexpr uint32_t kMotorRampIntervalMs = 8UL;
inline constexpr ModemPowerMode kModemPowerMode = ModemPowerMode::AlwaysOn;
inline constexpr int kFlowPidInitialPwm = 300;
inline constexpr uint32_t kI2cWarmupMs = 0UL;
inline constexpr bool kEncoderInverted = false;

inline constexpr float kMotorOverheatHighC = 70.0f;
inline constexpr float kMotorOverheatLowC = 60.0f;
inline constexpr uint32_t kMotorOverheatLogIntervalMs = 5000UL;

inline constexpr const char kModemApn[] = "gigsky-02";
inline constexpr const char kRtcTimeServerUrl[] = "http://time.cmasccp.cl/";
inline constexpr bool kModemAllowHttpFallback = false;

inline constexpr float kAfm07FlowDivisor = 100.0f;
inline constexpr float kDroneTargetFlowLpm = 5.0f;

constexpr FlowPidConfig makeFlowPidConfig()
{
    FlowPidConfig config{};
    config.intervalMs = 200UL;
    config.deadband = 0.06f;
    config.kp = 35.0f;
    config.ki = 1.0f;
    config.integralLimit = 16.0f;
    config.maxStep = 32;
    config.filterAlpha = 0.35f;
    config.minActive = 0.30f;
    config.kd = 0.0f;
    config.maxDtMs = 1500UL;
    config.sensorStaleMs = 1500UL;
    config.kickPwm = 1650;
    config.kickMs = 300UL;
    config.stallFlowLpm = 0.15f;
    config.restallCooldownMs = 10000UL;
    config.stallConfirmMs = 2000UL;
    config.softTrimMax = 64;
    config.softMaxStep = 3;
    config.sensitivity = 0.8f;
    config.recenterDelayMs = 1500UL;
    return config;
}

inline constexpr FlowPidConfig kFlowPid = makeFlowPidConfig();
}

#endif
