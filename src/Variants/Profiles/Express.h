#ifndef EOLO_CONFIG_PROFILE_EXPRESS_H
#define EOLO_CONFIG_PROFILE_EXPRESS_H

#include "../Types.h"
#include <Eolo/Core/Flow/FlowMotorController.h>

// Pantalla por defecto: SSD1309 NONAME0 por I2C, reloj SPI inactivo (0).
// El mapa SPI heredado colisiona con RS485 y no se puede activar sin otro pinout.
#define EOLO_DEFAULT_DISPLAY SSD1309_I2C
#include "../Displays.h"

namespace EoloConfig::Profile
{
inline constexpr DisplayKind kDefaultDisplay = static_cast<DisplayKind>(EOLO_DEFAULT_DISPLAY);
inline constexpr const char kModelName[] = "EOLO Express";
inline constexpr bool kUseCalibrationSeed = true;

inline constexpr uint32_t kI2cClockHz = 150000UL;
inline constexpr uint8_t kAttinyAddress = 8;
inline constexpr bool kMotorPwmInverted = false;
inline constexpr uint8_t kMotorPwmResolutionBits = 11;
inline constexpr int kMotorRampStep = 0;
inline constexpr uint32_t kMotorRampIntervalMs = 8UL;
inline constexpr ModemPowerMode kModemPowerMode = ModemPowerMode::AlwaysOn;
inline constexpr int kFlowPidInitialPwm = 300;
inline constexpr uint32_t kI2cWarmupMs = 0UL;
inline constexpr uint32_t kI2cTransactionTimeoutMs = 20UL;
inline constexpr bool kEncoderInverted = false;
// 0: el encoder comparte el bus I2C principal; otro valor: Wire1 a esa frecuencia
// (pines ENCODER_SDA_PIN/ENCODER_SCL_PIN).
inline constexpr uint32_t kEncoderBusClockHz = 0UL;
inline constexpr BatteryModel kBatteryModel = BatteryModel::LegacyDivider;

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
    config.zeroFlowConfirmSamples = 5;
    return config;
}

inline constexpr FlowPidConfig kFlowPid = makeFlowPidConfig();
}

#endif
