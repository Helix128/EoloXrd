#ifndef EOLO_CONFIG_PROFILE_EXPRESS_LEGACY_H
#define EOLO_CONFIG_PROFILE_EXPRESS_LEGACY_H

#include "../Types.h"
#include <Eolo/Core/Flow/DualPumpFlowConfig.h>

#define EOLO_DEFAULT_DISPLAY SSD1306_I2C
#include "../Displays.h"

// Este perfil es deliberadamente completo. Aunque hoy comparte sus valores de
// tuning con Express, no debe heredar cambios futuros de otra revisión.

namespace EoloConfig::Profile
{
inline constexpr DisplayKind kDefaultDisplay = static_cast<DisplayKind>(EOLO_DEFAULT_DISPLAY);
inline constexpr const char kModelName[] = "EOLO Express FE"; // FE = FS3000 Edition
// Dos bombas con orden debil->fuerte: prueba rapida de caudal al arrancar si no hay orden guardado.
inline constexpr bool kPumpOrderTest = true;
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

inline constexpr float kAfm07FlowDivisor = 10.0f;
inline constexpr float kDroneTargetFlowLpm = 5.0f;

// Dos bombas en serie con la ley continua del Dron, afinada para el FS3000
// (ver DualPumpFlowConfig.h). La tabla PWM->caudal es la nominal de dos bombas.
inline constexpr FlowPidConfig kFlowPid =
    DualPumpFlow::makeFs3000Config(kMotorRampStep * 1000.0f / static_cast<float>(kMotorRampIntervalMs));
static_assert(kFlowPid.feedForward.valid(2 * ((1 << kMotorPwmResolutionBits) - 1)),
              "La tabla de dos bombas debe caber en el PWM virtual");
}

#endif
