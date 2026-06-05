#ifndef EOLO_BOARD_PINOUT_H
#define EOLO_BOARD_PINOUT_H

#define EOLO_PIN_UNUSED -1

#define EOLO_INPUT_ONLY_PIN(pin) ((pin) >= 34 && (pin) <= 39)
#define EOLO_VALID_PIN(pin) ((pin) >= 0)
#define EOLO_SAME_USED_PIN(a, b) (EOLO_VALID_PIN(a) && EOLO_VALID_PIN(b) && ((a) == (b)))

#if defined(EOLO_TARGET_DRON)
  #define SDA_PIN 21
  #define SCL_PIN 22
  #define SD_CS_PIN 5
  #define SD_MOSI_PIN 23
  #define SD_MISO_PIN 19
  #define SD_SCK_PIN 18

  #define WAIT_SW0_PIN 32
  #define WAIT_SW1_PIN 33
  #define DURATION_SW0_PIN 14
  #define DURATION_SW1_PIN 13

  #define NEOPIXEL_PIN 27
  #define NEOPIXEL_COUNT 1
  #define NEOPIXEL_BRIGHTNESS 60

  #define NTC_PIN 34
  #define BATTERY_ADC_PIN EOLO_PIN_UNUSED

  #define RS485_RX_PIN 16
  #define RS485_TX_PIN 17
  #define RS485_DE_RE_PIN 4

  #define MOTOR_PWM_PIN_0 26
  #define MOTOR_PWM_PIN_1 EOLO_PIN_UNUSED
  #define MOTOR_PWM_PIN_COUNT 1
  #define MOTOR_FG_PIN 35
  #define MOTOR_POWER_PIN EOLO_PIN_UNUSED

  #define PT_RX EOLO_PIN_UNUSED
  #define PT_TX EOLO_PIN_UNUSED
  #define PPH_PWR_PIN EOLO_PIN_UNUSED
  #define MODEM_PWR_PIN EOLO_PIN_UNUSED
  #define MODEM_RX_PIN EOLO_PIN_UNUSED
  #define MODEM_TX_PIN EOLO_PIN_UNUSED
#elif defined(EOLO_TARGET_STANDARD) || defined(EOLO_TARGET_EXPRESS) || defined(EOLO_TARGET_EXPRESS_LEGACY)
  #define SDA_PIN 21
  #define SCL_PIN 22
  #define SD_CS_PIN 5
  #define SD_MOSI_PIN 23
  #define SD_MISO_PIN 19
  #define SD_SCK_PIN 18

  #define WAIT_SW0_PIN 32
  #define WAIT_SW1_PIN 14
  #define DURATION_SW0_PIN 36
  #define DURATION_SW1_PIN 39

  #define NEOPIXEL_PIN 27
  #define NEOPIXEL_COUNT 1
  #define NEOPIXEL_BRIGHTNESS 60

  #define NTC_PIN EOLO_PIN_UNUSED
  #define BATTERY_ADC_PIN 34

  #define RS485_RX_PIN 35
  #define RS485_TX_PIN 33
  #define RS485_DE_RE_PIN 26

  #define MOTOR_PWM_PIN_0 25
  #define MOTOR_PWM_PIN_1 27
  #define MOTOR_PWM_PIN_COUNT 2
  #define MOTOR_FG_PIN EOLO_PIN_UNUSED
  #define MOTOR_POWER_PIN EOLO_PIN_UNUSED

  #define PT_RX 34
  #define PT_TX 32
  #define PPH_PWR_PIN 4
  #define MODEM_PWR_PIN 13
  #define MODEM_RX_PIN 16
  #define MODEM_TX_PIN 17
#else
  #error "Define un target EOLO_TARGET_* antes de incluir Board/Pinout.h"
#endif

#if EOLO_SAME_USED_PIN(RS485_RX_PIN, RS485_TX_PIN) || \
    EOLO_SAME_USED_PIN(RS485_RX_PIN, RS485_DE_RE_PIN) || \
    EOLO_SAME_USED_PIN(RS485_TX_PIN, RS485_DE_RE_PIN)
  #error "Pinout invalido: RS485 tiene pines duplicados"
#endif

#if EOLO_SAME_USED_PIN(MOTOR_PWM_PIN_0, MOTOR_PWM_PIN_1)
  #error "Pinout invalido: motores PWM duplican GPIO"
#endif

#if EOLO_SAME_USED_PIN(WAIT_SW0_PIN, WAIT_SW1_PIN) || \
    EOLO_SAME_USED_PIN(WAIT_SW0_PIN, DURATION_SW0_PIN) || \
    EOLO_SAME_USED_PIN(WAIT_SW0_PIN, DURATION_SW1_PIN) || \
    EOLO_SAME_USED_PIN(WAIT_SW1_PIN, DURATION_SW0_PIN) || \
    EOLO_SAME_USED_PIN(WAIT_SW1_PIN, DURATION_SW1_PIN) || \
    EOLO_SAME_USED_PIN(DURATION_SW0_PIN, DURATION_SW1_PIN)
  #error "Pinout invalido: switches DIP duplican GPIO"
#endif

#if EOLO_INPUT_ONLY_PIN(RS485_TX_PIN) || EOLO_INPUT_ONLY_PIN(RS485_DE_RE_PIN)
  #error "Pinout invalido: RS485 TX y DE/RE no pueden usar GPIO34-39"
#endif

#if EOLO_INPUT_ONLY_PIN(MOTOR_PWM_PIN_0) || EOLO_INPUT_ONLY_PIN(MOTOR_PWM_PIN_1) || EOLO_INPUT_ONLY_PIN(MOTOR_POWER_PIN)
  #error "Pinout invalido: control de motor no puede usar GPIO34-39"
#endif

#if EOLO_INPUT_ONLY_PIN(PT_TX) || EOLO_INPUT_ONLY_PIN(PPH_PWR_PIN) || EOLO_INPUT_ONLY_PIN(MODEM_PWR_PIN) || EOLO_INPUT_ONLY_PIN(MODEM_RX_PIN)
  #error "Pinout invalido: salidas de perifericos/modem no pueden usar GPIO34-39"
#endif

#if defined(EOLO_TARGET_DRON) && PPH_PWR_PIN == 4
  #error "EOLO Dron usa GPIO4 como RS485 DE/RE; no lo configures como PPH_PWR_PIN"
#endif

#if defined(EOLO_TARGET_DRON) && MODEM_PWR_PIN == 13
  #error "EOLO Dron usa GPIO13 como DIP4; no lo configures como modem"
#endif

#if defined(FEATURE_NTC) && NTC_PIN < 0
  #error "FEATURE_NTC requiere NTC_PIN configurado"
#endif

#if defined(FEATURE_NTC) && EOLO_SAME_USED_PIN(NTC_PIN, BATTERY_ADC_PIN)
  #error "Pinout invalido: NTC_PIN y BATTERY_ADC_PIN duplican GPIO"
#endif

#if defined(FEATURE_NTC) && !EOLO_INPUT_ONLY_PIN(NTC_PIN)
  #warning "NTC_PIN recomendado en GPIO34-39 para entrada ADC-only"
#endif

#if defined(FEATURE_NEOPIXEL) && NEOPIXEL_PIN < 0
  #error "FEATURE_NEOPIXEL requiere NEOPIXEL_PIN configurado"
#endif

#endif
