#ifndef EOLO_CONFIG_VARIANT_VALIDATION_H
#define EOLO_CONFIG_VARIANT_VALIDATION_H

// Las capacidades deben llegar explícitamente desde el entorno de build. No
// se infieren desde el target ni se completan con defaults silenciosos.
#if defined(FEATURE_FLOW_AFM07) && defined(FEATURE_FLOW_FS3000)
  #error "Conflicto: define solo un sensor de flujo"
#endif
#if !defined(FEATURE_FLOW_AFM07) && !defined(FEATURE_FLOW_FS3000)
  #error "Define explícitamente un sensor de flujo FEATURE_FLOW_*"
#endif

#if defined(FEATURE_FLOW_CALIBRATION) && defined(FEATURE_FLOW_PID)
  #error "Conflicto: define solo un modo de flujo"
#endif
#if !defined(FEATURE_FLOW_CALIBRATION) && !defined(FEATURE_FLOW_PID)
  #error "Define explícitamente un modo de control de flujo"
#endif

#if defined(FEATURE_MOTOR_PWM) && defined(FEATURE_MOTOR_PWM_POWER_PIN)
  #error "Conflicto: define solo un tipo de motor"
#endif
#if !defined(FEATURE_MOTOR_PWM) && !defined(FEATURE_MOTOR_PWM_POWER_PIN)
  #error "Define explícitamente el tipo de motor PWM"
#endif

#ifndef EOLO_I2C_DIRECT_DRIVERS
  #error "Define EOLO_I2C_DIRECT_DRIVERS=0 o 1 explícitamente"
#endif
#if (EOLO_I2C_DIRECT_DRIVERS != 0) && (EOLO_I2C_DIRECT_DRIVERS != 1)
  #error "EOLO_I2C_DIRECT_DRIVERS debe ser 0 o 1"
#endif

#if defined(EOLO_TARGET_DRON)
  #if !defined(FEATURE_HEADLESS) || !defined(FEATURE_MOTOR_PWM) || \
      !defined(FEATURE_FLOW_AFM07) || !defined(FEATURE_FLOW_PID) || \
      !defined(FEATURE_NEOPIXEL) || !defined(FEATURE_NTC) || \
      !defined(DRONE_SWITCHES_HAVE_EXTERNAL_PULLS)
    #error "EOLO Dron requiere sus capacidades headless, PWM, AFM07, PID, NTC, NeoPixel y switches explícitas"
  #endif
  #if defined(FEATURE_MOTOR_PWM_POWER_PIN) || defined(FEATURE_FLOW_FS3000) || \
      defined(FEATURE_FLOW_CALIBRATION) || defined(FEATURE_MODEM) || \
      defined(FEATURE_ANEMOMETER) || defined(FEATURE_DUAL_BATTERY) || \
      defined(FEATURE_PLANTOWER)
    #error "EOLO Dron recibió una capacidad de otro modelo"
  #endif
#elif defined(EOLO_TARGET_EXPRESS)
  #if !defined(FEATURE_MOTOR_PWM) || !defined(FEATURE_FLOW_AFM07) || \
      !defined(FEATURE_FLOW_CALIBRATION) || !defined(FEATURE_PLANTOWER)
    #error "EOLO Express requiere PWM directo, AFM07, calibración y Plantower explícitos"
  #endif
  #if defined(FEATURE_HEADLESS) || defined(FEATURE_MOTOR_PWM_POWER_PIN) || \
      defined(FEATURE_FLOW_FS3000) || defined(FEATURE_FLOW_PID) || \
      defined(FEATURE_MODEM) || defined(FEATURE_ANEMOMETER) || \
      defined(FEATURE_DUAL_BATTERY) || defined(FEATURE_NEOPIXEL) || \
      defined(FEATURE_NTC) || defined(DRONE_SWITCHES_HAVE_EXTERNAL_PULLS)
    #error "EOLO Express recibió una capacidad de otro modelo"
  #endif
#elif defined(EOLO_TARGET_EXPRESS_LEGACY)
  #if !defined(FEATURE_MOTOR_PWM) || !defined(FEATURE_FLOW_FS3000) || \
      !defined(FEATURE_FLOW_CALIBRATION) || !defined(FEATURE_PLANTOWER)
    #error "EOLO Express Legacy requiere PWM directo, FS3000, calibración y Plantower explícitos"
  #endif
  #if defined(FEATURE_HEADLESS) || defined(FEATURE_MOTOR_PWM_POWER_PIN) || \
      defined(FEATURE_FLOW_AFM07) || defined(FEATURE_FLOW_PID) || \
      defined(FEATURE_MODEM) || defined(FEATURE_ANEMOMETER) || \
      defined(FEATURE_DUAL_BATTERY) || defined(FEATURE_NEOPIXEL) || \
      defined(FEATURE_NTC) || defined(DRONE_SWITCHES_HAVE_EXTERNAL_PULLS)
    #error "EOLO Express Legacy recibió una capacidad de otro modelo"
  #endif
#elif defined(EOLO_TARGET_STANDARD)
  #if !defined(FEATURE_MOTOR_PWM) || !defined(FEATURE_FLOW_AFM07) || \
      !defined(FEATURE_FLOW_CALIBRATION) || !defined(FEATURE_PLANTOWER) || \
      !defined(FEATURE_MODEM) || !defined(FEATURE_ANEMOMETER) || \
      !defined(FEATURE_DUAL_BATTERY)
    #error "EOLO Standard requiere sus capacidades de producción explícitas"
  #endif
  #if defined(FEATURE_HEADLESS) || defined(FEATURE_MOTOR_PWM_POWER_PIN) || \
      defined(FEATURE_FLOW_FS3000) || defined(FEATURE_FLOW_PID) || \
      defined(FEATURE_NEOPIXEL) || defined(FEATURE_NTC) || \
      defined(DRONE_SWITCHES_HAVE_EXTERNAL_PULLS)
    #error "EOLO Standard recibió una capacidad de otro modelo"
  #endif
#endif

#if defined(STATUS_LED_LOW_POWER) && !defined(EOLO_TARGET_DRON)
  #error "STATUS_LED_LOW_POWER solo es válido para EOLO Dron"
#endif

static_assert(EoloConfig::motorOverheatLowC < EoloConfig::motorOverheatHighC,
              "El umbral NTC bajo debe ser menor al alto");
static_assert(EoloConfig::motorPwmResolutionBits >= 1 &&
              EoloConfig::motorPwmResolutionBits <= 15,
              "La resolución PWM debe estar entre 1 y 15 bits");

// EOLO Standard usa el mapa SPI que ya fue probado en la demo secuencial:
// VSPI SCK=18/MISO=19/MOSI=23, OLED CS=27/DC=15/RES=2 y SD CS=5.
#if defined(EOLO_TARGET_STANDARD)
static_assert(EoloConfig::displayClockPin == 18 &&
              EoloConfig::displayMosiPin == 23 &&
              EoloConfig::displayCsPin == 27 &&
              EoloConfig::displayDcPin == 15 &&
              EoloConfig::displayResetPin == 2,
              "EOLO Standard: mapa OLED SPI no coincide con el hardware probado");
static_assert(EoloConfig::displayCsPin != SD_CS_PIN,
              "EOLO Standard: OLED CS no puede compartir CS con SD");
static_assert(EoloConfig::displayCsPin != MOTOR_PWM_PIN_0 &&
              EoloConfig::displayCsPin != MOTOR_PWM_PIN_1 &&
              EoloConfig::displayDcPin != MOTOR_PWM_PIN_0 &&
              EoloConfig::displayDcPin != MOTOR_PWM_PIN_1 &&
              EoloConfig::displayResetPin != MOTOR_PWM_PIN_0 &&
              EoloConfig::displayResetPin != MOTOR_PWM_PIN_1,
              "EOLO Standard: OLED duplica un pin de motor PWM");
static_assert(EoloConfig::displayCsPin != RS485_RX_PIN &&
              EoloConfig::displayCsPin != RS485_TX_PIN &&
              EoloConfig::displayCsPin != RS485_DE_RE_PIN &&
              EoloConfig::displayDcPin != RS485_RX_PIN &&
              EoloConfig::displayDcPin != RS485_TX_PIN &&
              EoloConfig::displayDcPin != RS485_DE_RE_PIN &&
              EoloConfig::displayResetPin != RS485_RX_PIN &&
              EoloConfig::displayResetPin != RS485_TX_PIN &&
              EoloConfig::displayResetPin != RS485_DE_RE_PIN,
              "EOLO Standard: OLED duplica un pin de RS485");
static_assert(EoloConfig::displayClockPin != MOTOR_PWM_PIN_0 &&
              EoloConfig::displayClockPin != MOTOR_PWM_PIN_1 &&
              EoloConfig::displayMosiPin != MOTOR_PWM_PIN_0 &&
              EoloConfig::displayMosiPin != MOTOR_PWM_PIN_1,
              "EOLO Standard: VSPI duplica un pin de motor PWM");
#endif

#endif
