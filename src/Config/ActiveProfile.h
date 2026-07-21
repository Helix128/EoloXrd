#ifndef EOLO_CONFIG_ACTIVE_PROFILE_H
#define EOLO_CONFIG_ACTIVE_PROFILE_H

#if (defined(EOLO_TARGET_DRON) + defined(EOLO_TARGET_STANDARD) + \
     defined(EOLO_TARGET_EXPRESS) + defined(EOLO_TARGET_EXPRESS_LEGACY)) != 1
  #error "Define exactamente un target EOLO_TARGET_* en platformio.ini"
#endif

#if defined(EOLO_TARGET_DRON)
  #include "Profiles/Dron.h"
#elif defined(EOLO_TARGET_STANDARD)
  #include "Profiles/Standard.h"
#elif defined(EOLO_TARGET_EXPRESS_LEGACY)
  #include "Profiles/ExpressLegacy.h"
#else
  #include "Profiles/Express.h"
#endif

#include "../Board/Pinout.h"

#define EOLO_DISPLAY_SPI 1
#ifndef EOLO_DISPLAY_HW_SPI
#define EOLO_DISPLAY_HW_SPI 0
#endif

namespace EoloConfig
{
inline constexpr const BoardConfig &board = Profile::kBoard;
inline constexpr const SafetyConfig &safety = Profile::kSafety;
inline constexpr const ServiceDefaults &services = Profile::kServices;
inline constexpr const SensorConfig &sensors = Profile::kSensors;
inline constexpr const FlowPidConfig &flowPid = Profile::kFlowPid;
}

static_assert(EoloConfig::safety.motorOverheatLowC < EoloConfig::safety.motorOverheatHighC,
              "El umbral NTC bajo debe ser menor al alto");
static_assert(EoloConfig::board.motorPwmResolutionBits >= 1 &&
              EoloConfig::board.motorPwmResolutionBits <= 15,
              "La resolución PWM debe estar entre 1 y 15 bits");

// EOLO Standard usa el mapa SPI que ya fue probado en la demo secuencial:
// VSPI SCK=18/MISO=19/MOSI=23, OLED CS=27/DC=15/RES=2 y SD CS=5.
// Los asserts convierten cualquier regresión de pinout en un error de
// compilación, antes de que vuelva a llegar a la placa.
#if defined(EOLO_TARGET_STANDARD)
static_assert(EoloConfig::board.displayClockPin == 18 &&
              EoloConfig::board.displayMosiPin == 23 &&
              EoloConfig::board.displayCsPin == 27 &&
              EoloConfig::board.displayDcPin == 15 &&
              EoloConfig::board.displayResetPin == 2,
              "EOLO Standard: mapa OLED SPI no coincide con el hardware probado");
static_assert(EoloConfig::board.displayCsPin != SD_CS_PIN,
              "EOLO Standard: OLED CS no puede compartir CS con SD");
static_assert(EoloConfig::board.displayCsPin != MOTOR_PWM_PIN_0 &&
              EoloConfig::board.displayCsPin != MOTOR_PWM_PIN_1 &&
              EoloConfig::board.displayDcPin != MOTOR_PWM_PIN_0 &&
              EoloConfig::board.displayDcPin != MOTOR_PWM_PIN_1 &&
              EoloConfig::board.displayResetPin != MOTOR_PWM_PIN_0 &&
              EoloConfig::board.displayResetPin != MOTOR_PWM_PIN_1,
              "EOLO Standard: OLED duplica un pin de motor PWM");
static_assert(EoloConfig::board.displayCsPin != RS485_RX_PIN &&
              EoloConfig::board.displayCsPin != RS485_TX_PIN &&
              EoloConfig::board.displayCsPin != RS485_DE_RE_PIN &&
              EoloConfig::board.displayDcPin != RS485_RX_PIN &&
              EoloConfig::board.displayDcPin != RS485_TX_PIN &&
              EoloConfig::board.displayDcPin != RS485_DE_RE_PIN &&
              EoloConfig::board.displayResetPin != RS485_RX_PIN &&
              EoloConfig::board.displayResetPin != RS485_TX_PIN &&
              EoloConfig::board.displayResetPin != RS485_DE_RE_PIN,
              "EOLO Standard: OLED duplica un pin de RS485");
static_assert(EoloConfig::board.displayClockPin != MOTOR_PWM_PIN_0 &&
              EoloConfig::board.displayClockPin != MOTOR_PWM_PIN_1 &&
              EoloConfig::board.displayMosiPin != MOTOR_PWM_PIN_0 &&
              EoloConfig::board.displayMosiPin != MOTOR_PWM_PIN_1,
              "EOLO Standard: VSPI duplica un pin de motor PWM");
#endif

#if defined(FEATURE_FLOW_AFM07) && defined(FEATURE_FLOW_FS3000)
  #error "Conflicto: define solo un sensor de flujo"
#endif
#if defined(FEATURE_FLOW_CALIBRATION) && defined(FEATURE_FLOW_PID)
  #error "Conflicto: define solo un modo de flujo"
#endif
#if defined(FEATURE_MOTOR_PWM) && defined(FEATURE_MOTOR_PWM_POWER_PIN)
  #error "Conflicto: define solo un tipo de motor"
#endif

// Compatibilidad con ambientes externos que solo seleccionan un target.
#if !defined(FEATURE_FLOW_CALIBRATION) && !defined(FEATURE_FLOW_PID)
  #define FEATURE_FLOW_CALIBRATION
#endif
#if !defined(FEATURE_MOTOR_PWM) && !defined(FEATURE_MOTOR_PWM_POWER_PIN)
  #define FEATURE_MOTOR_PWM
#endif

#if defined(EOLO_TARGET_DRON) && \
    (DURATION_SW0_PIN >= 34 || DURATION_SW1_PIN >= 34) && \
    !defined(DRONE_SWITCHES_HAVE_EXTERNAL_PULLS)
  #error "GPIO34-39 no tienen pull-up/down interno para switches de dron"
#endif

#endif
