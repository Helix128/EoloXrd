#ifndef EOLO_CONFIG_PROFILE_EXPRESS_LEGACY_H
#define EOLO_CONFIG_PROFILE_EXPRESS_LEGACY_H

#include "../Types.h"
#include <Eolo/Core/Flow/FlowMotorController.h>

// Este perfil es deliberadamente completo. Aunque hoy comparte sus valores de
// tuning con Express, no debe heredar cambios futuros de otra revisión.
#define EOLO_DISPLAY_MODEL U8G2_SSD1309_128X64_NONAME2_F_4W_SW_SPI
#define EOLO_DISPLAY_SPI 1
#define EOLO_DISPLAY_HW_SPI 0

namespace EoloConfig::Profile
{
inline constexpr const char kModelName[] = "EOLO Express FE"; // FE = FS3000 Edition
inline constexpr bool kUseCalibrationSeed = true;

inline constexpr uint32_t kI2cClockHz = 150000UL;
inline constexpr uint32_t kDisplaySpiClockHz = 8000000UL;
inline constexpr uint8_t kAttinyAddress = 8;
inline constexpr bool kMotorPwmInverted = false;
inline constexpr uint8_t kMotorPwmResolutionBits = 11;
inline constexpr int kMotorRampStep = 0;
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
    config.intervalMs = 100UL;
    config.deadband = 0.08f;
    config.kp = 20.0f;
    config.ki = 0.8f;
    config.integralLimit = 20.0f;
    config.maxStep = 24;
    config.filterAlpha = 0.30f;
    config.minActive = 0.20f;
    config.kd = 0.0f;
    config.maxDtMs = 1000UL;
    config.sensorStaleMs = 1200UL;
    config.kickPwm = 800;
    config.kickMs = 250UL;
    config.stallFlowLpm = 0.10f;
    config.restallCooldownMs = 8000UL;
    config.stallConfirmMs = 1500UL;
    config.softTrimMax = 80;
    config.softMaxStep = 2;
    config.sensitivity = 1.0f;
    config.recenterDelayMs = 4000UL;
    return config;
}

inline constexpr FlowPidConfig kFlowPid = makeFlowPidConfig();
}

#endif
