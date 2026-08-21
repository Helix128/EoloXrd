#ifndef EOLO_CONFIG_PROFILE_STANDARD_H
#define EOLO_CONFIG_PROFILE_STANDARD_H

#include "../Types.h"
#include <Eolo/Core/Flow/FlowMotorController.h>

#define EOLO_DISPLAY_MODEL U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI
#define EOLO_DISPLAY_SPI 1
#define EOLO_DISPLAY_HW_SPI 1

namespace EoloConfig::Profile
{
inline constexpr const char kModelName[] = "EOLO Standard";
inline constexpr bool kUseCalibrationSeed = false;

// La demo probada maneja las bombas con PWM directo: duty 0 = apagado.
// U8g2 usa 4 MHz por defecto para este SSD1306; se conserva ese valor.
// El bus principal comparte cableado con ATmega328P; 50 kHz y 5 s de
// estabilizacion son las condiciones verificadas en Eolo Grande.
inline constexpr uint32_t kI2cClockHz = 50000UL;
inline constexpr uint32_t kDisplaySpiClockHz = 4000000UL;
inline constexpr uint8_t kAttinyAddress = 8;
inline constexpr bool kMotorPwmInverted = false;
inline constexpr uint8_t kMotorPwmResolutionBits = 11;
inline constexpr int kMotorRampStep = 0;
inline constexpr uint32_t kMotorRampIntervalMs = 8UL;
inline constexpr ModemPowerMode kModemPowerMode = ModemPowerMode::AlwaysOn;
inline constexpr int kFlowPidInitialPwm = 736;
inline constexpr uint32_t kI2cWarmupMs = 5000UL;
inline constexpr bool kEncoderInverted = true;

inline constexpr float kMotorOverheatHighC = 70.0f;
inline constexpr float kMotorOverheatLowC = 60.0f;
inline constexpr uint32_t kMotorOverheatLogIntervalMs = 5000UL;

inline constexpr const char kModemApn[] = "gigsky-02";
inline constexpr const char kRtcTimeServerUrl[] = "https://time.cmasccp.cl/";
// HTTP downgrade is a build-profile decision, never a console/NVS setting.
inline constexpr bool kModemAllowHttpFallback = false;

inline constexpr float kAfm07FlowDivisor = 10.0f;
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
