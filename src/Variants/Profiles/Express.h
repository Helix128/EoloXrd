#ifndef EOLO_CONFIG_PROFILE_EXPRESS_H
#define EOLO_CONFIG_PROFILE_EXPRESS_H

#include "../Types.h"
#include <Eolo/Core/Flow/DualPumpFlowConfig.h>

// Pantalla por defecto: SSD1309 NONAME0 por I2C, reloj SPI inactivo (0).
// El mapa SPI heredado colisiona con RS485 y no se puede activar sin otro pinout.
#define EOLO_DEFAULT_DISPLAY SSD1309_I2C
#include "../Displays.h"

namespace EoloConfig::Profile
{
inline constexpr DisplayKind kDefaultDisplay = static_cast<DisplayKind>(EOLO_DEFAULT_DISPLAY);
inline constexpr const char kModelName[] = "EOLO Express";
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

// Dos bombas en serie con la ley continua del Dron (ver DualPumpFlowConfig.h).
inline constexpr FlowPidConfig kFlowPid =
    DualPumpFlow::makeConfig(kMotorRampStep * 1000.0f / static_cast<float>(kMotorRampIntervalMs));
static_assert(kFlowPid.feedForward.valid(2 * ((1 << kMotorPwmResolutionBits) - 1)),
              "La tabla de dos bombas debe caber en el PWM virtual");
}

#endif
