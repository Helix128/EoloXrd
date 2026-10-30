#ifndef EOLO_BOARD_PINOUTS_EXPRESS_LEGACY_H
#define EOLO_BOARD_PINOUTS_EXPRESS_LEGACY_H

// EOLO Express Legacy (Rev.1, FS3000): mapa del firmware original de noviembre
// 2025 que corre en el equipo físico: OLED SSD1306 por I2C y bombas en
// GPIO26/27. La variante con OLED SPI es ExpressLegacySpi.h.
#define SDA_PIN 21
#define SCL_PIN 22

#define SD_CS_PIN 5
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN 18

// La OLED va por I2C (bus principal): no hay pines de display SPI.
#define EOLO_DISPLAY_CS_PIN EOLO_PIN_UNUSED
#define EOLO_DISPLAY_DC_PIN EOLO_PIN_UNUSED
#define EOLO_DISPLAY_RESET_PIN EOLO_PIN_UNUSED
#define EOLO_DISPLAY_SCK_PIN EOLO_PIN_UNUSED
#define EOLO_DISPLAY_MOSI_PIN EOLO_PIN_UNUSED

#define WAIT_SW0_PIN EOLO_PIN_UNUSED
#define WAIT_SW1_PIN EOLO_PIN_UNUSED
#define DURATION_SW0_PIN EOLO_PIN_UNUSED
#define DURATION_SW1_PIN EOLO_PIN_UNUSED

#define NEOPIXEL_PIN EOLO_PIN_UNUSED
#define NEOPIXEL_COUNT 1
#define NEOPIXEL_BRIGHTNESS 60

#define NTC_PIN EOLO_PIN_UNUSED
#define BATTERY_ADC_PIN 34

// FS3000 va por I2C: sin RS485. GPIO26 es una bomba, no DE/RE.
#define RS485_RX_PIN EOLO_PIN_UNUSED
#define RS485_TX_PIN EOLO_PIN_UNUSED
#define RS485_DE_RE_PIN EOLO_PIN_UNUSED

// Bombas en GPIO26/27 (firmware original, nov 2025). El orden físico PWM0/PWM1
// no está confirmado en placa.
#define MOTOR_PWM_PIN_0 26
#define MOTOR_PWM_PIN_1 27
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

// Bus I2C secundario dedicado al encoder; sin usar en este modelo.
#define ENCODER_SDA_PIN EOLO_PIN_UNUSED
#define ENCODER_SCL_PIN EOLO_PIN_UNUSED

#define EOLO_PINOUT_COMPLETE 1

#endif
