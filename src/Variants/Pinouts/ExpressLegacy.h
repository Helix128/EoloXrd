#ifndef EOLO_BOARD_PINOUTS_EXPRESS_LEGACY_H
#define EOLO_BOARD_PINOUTS_EXPRESS_LEGACY_H

// EOLO Express Legacy: mapa completo de la revisión FS3000.
#define SDA_PIN 21
#define SCL_PIN 22

#define SD_CS_PIN 5
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN 18

// OLED SPI por software de la revisión Legacy.
#define EOLO_DISPLAY_CS_PIN 27
#define EOLO_DISPLAY_DC_PIN 15
#define EOLO_DISPLAY_RESET_PIN 2
#define EOLO_DISPLAY_SCK_PIN 18
#define EOLO_DISPLAY_MOSI_PIN 23

#define WAIT_SW0_PIN EOLO_PIN_UNUSED
#define WAIT_SW1_PIN EOLO_PIN_UNUSED
#define DURATION_SW0_PIN EOLO_PIN_UNUSED
#define DURATION_SW1_PIN EOLO_PIN_UNUSED

#define NEOPIXEL_PIN EOLO_PIN_UNUSED
#define NEOPIXEL_COUNT 1
#define NEOPIXEL_BRIGHTNESS 60

#define NTC_PIN EOLO_PIN_UNUSED
#define BATTERY_ADC_PIN 34

// Este bus no es usado por FS3000, pero se conserva el mapa compilado actual.
#define RS485_RX_PIN 35
#define RS485_TX_PIN 33
#define RS485_DE_RE_PIN 26

// Se conserva el orden de producción 25/14 hasta validación física del
// cableado Legacy; no normalizarlo según documentación histórica.
#define MOTOR_PWM_PIN_0 25
#define MOTOR_PWM_PIN_1 14
#define MOTOR_PWM_PIN_COUNT 2
#define MOTOR_FG_PIN EOLO_PIN_UNUSED
#define MOTOR_POWER_PIN EOLO_PIN_UNUSED

#define PT_RX 16
#define PT_TX 17
#define PPH_PWR_PIN 4

// Pines declarados por compatibilidad; FEATURE_MODEM permanece deshabilitada.
#define MODEM_PWR_PIN 13
#define MODEM_RX_PIN 16
#define MODEM_TX_PIN 17

#define EOLO_PINOUT_COMPLETE 1

#endif
