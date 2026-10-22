#ifndef EOLO_CONFIG_PROFILE_DRON_H
#define EOLO_CONFIG_PROFILE_DRON_H

#include "../Types.h"
#include <Eolo/Core/Flow/FlowMotorController.h>

// OLED 1.3" SH1106 por I2C hardware en el bus principal (Wire). El SSD1306 de
// otras variantes deja una franja blanca/texto cortado en este panel.
#define EOLO_DISPLAY_MODEL U8G2_SH1106_128X64_NONAME_F_HW_I2C
#define EOLO_DISPLAY_SPI 0
#define EOLO_DISPLAY_HW_SPI 0

namespace EoloConfig::Profile
{
inline constexpr const char kModelName[] = "EOLO Dron";
inline constexpr bool kUseCalibrationSeed = false;

inline constexpr uint32_t kI2cClockHz = 100000UL;
inline constexpr uint32_t kDisplaySpiClockHz = 0UL;
inline constexpr uint8_t kAttinyAddress = 8;
inline constexpr bool kMotorPwmInverted = false;
inline constexpr uint8_t kMotorPwmResolutionBits = 11;
inline constexpr int kMotorRampStep = 8;
inline constexpr uint32_t kMotorRampIntervalMs = 8UL;
inline constexpr ModemPowerMode kModemPowerMode = ModemPowerMode::AlwaysOn;
inline constexpr int kFlowPidInitialPwm = 300;
inline constexpr uint32_t kI2cWarmupMs = 0UL;
inline constexpr uint32_t kI2cTransactionTimeoutMs = 20UL;
inline constexpr bool kEncoderInverted = false;
// 0: el encoder comparte el bus I2C principal; otro valor: Wire1 a esa frecuencia
// (pines ENCODER_SDA_PIN/ENCODER_SCL_PIN).
inline constexpr uint32_t kEncoderBusClockHz = 10000UL;
inline constexpr BatteryModel kBatteryModel = BatteryModel::Pack2sLiIon;

inline constexpr float kMotorOverheatHighC = 70.0f;
inline constexpr float kMotorOverheatLowC = 60.0f;
inline constexpr uint32_t kMotorOverheatLogIntervalMs = 5000UL;

inline constexpr const char kModemApn[] = "gigsky-02";
inline constexpr const char kRtcTimeServerUrl[] = "http://time.cmasccp.cl/";
inline constexpr bool kModemAllowHttpFallback = false;

inline constexpr float kAfm07FlowDivisor = 100.0f;
inline constexpr float kDroneTargetFlowLpm = 5.0f;

// Barrido de duty con tubo conectado (una bomba, 0..2047). Ver
// docs/optimizacion-pid-flujo-dron.md, sección 2.1.
inline constexpr FlowFeedForwardPoint kFlowFeedForward[] = {
    {184, 0.00f}, {410, 0.80f}, {594, 1.71f}, {819, 2.51f}, {1003, 3.11f},
    {1228, 3.78f}, {1412, 4.60f}, {1638, 5.28f}, {1823, 5.87f}, {2047, 6.58f},
};

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
    config.feedForward = {kFlowFeedForward,
                          static_cast<uint8_t>(sizeof(kFlowFeedForward) / sizeof(kFlowFeedForward[0]))};
    config.rampCountsPerSecond = kMotorRampStep * 1000.0f / static_cast<float>(kMotorRampIntervalMs);
    // Una sola bomba: el PWM virtual no supera 2047 (sin enrollamiento oculto).
    config.motorCount = 1;
    // Medido en banco (AFM07 cada 360 ms, sigma <= 0,007 L/min, resolución
    // 0,01): la zona muerta de 0,08 dejaba -3,9 % a 1 L/min. El CUSUM
    // recupera una pérdida de ganancia de -15 % en 4-6 s (antes: -12 %
    // sostenido).
    config.deadband = 0.012f;
    config.deadbandRel = 0.005f;
    config.driftThreshold = 0.01f;
    config.freezeIntegral = true;
    config.cusumK = 0.006f;
    config.cusumH = 0.12f;
    config.cusumJumpMax = 300;
    config.cusumJumpFrac = 0.55f;
    config.cusumHoldMs = 2000UL;
    return config;
}

inline constexpr FlowPidConfig kFlowPid = makeFlowPidConfig();
static_assert(kFlowPid.feedForward.valid((1 << kMotorPwmResolutionBits) - 1),
              "kFlowFeedForward debe ser monótona y caber en el PWM de una bomba");
}

#endif
