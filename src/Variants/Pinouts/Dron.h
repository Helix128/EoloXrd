#ifndef EOLO_BOARD_PINOUTS_DRON_H
#define EOLO_BOARD_PINOUTS_DRON_H

// EOLO Dron (base, hardware V2): mapa de la demo demos/DronBase. No heredar
// pines de otro modelo; el Dron anterior es EOLO Dron Legacy.
// I2C1 (Wire): OLED SH1106, RTC DS3231 y BME280.
#define SDA_PIN 21
#define SCL_PIN 22

// I2C2 (Wire1): encoder (ATmega328 esclavo).
#define ENCODER_SDA_PIN 32
#define ENCODER_SCL_PIN 33

#define SD_CS_PIN 5
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN 18

// La OLED va por I2C: no hay pines de display SPI.
#define EOLO_DISPLAY_CS_PIN EOLO_PIN_UNUSED
#define EOLO_DISPLAY_DC_PIN EOLO_PIN_UNUSED
#define EOLO_DISPLAY_RESET_PIN EOLO_PIN_UNUSED
#define EOLO_DISPLAY_SCK_PIN EOLO_PIN_UNUSED
#define EOLO_DISPLAY_MOSI_PIN EOLO_PIN_UNUSED

// No es headless.
#define WAIT_SW0_PIN EOLO_PIN_UNUSED
#define WAIT_SW1_PIN EOLO_PIN_UNUSED
#define DURATION_SW0_PIN EOLO_PIN_UNUSED
#define DURATION_SW1_PIN EOLO_PIN_UNUSED

#define NEOPIXEL_PIN EOLO_PIN_UNUSED
#define NEOPIXEL_COUNT 1
#define NEOPIXEL_BRIGHTNESS 60
#define NEOPIXEL_STRENGTH_PERCENT 100

#define NTC_PIN EOLO_PIN_UNUSED
// Pack 18650 2S por divisor 220k/100k (3.2:1), ADC1_CH7 (solo entrada).
#define BATTERY_ADC_PIN 35

// AFM07 RS485 (MAX3485): RO(RX)=17, DI(TX)=4, DE+RE=16. No coincide con el
// mapa del Dron Legacy (RX=16, TX=17, DE/RE=4).
#define RS485_RX_PIN 17
#define RS485_TX_PIN 4
#define RS485_DE_RE_PIN 16

// Bomba de vacío DC (N-FET low-side, PWM directo).
#define MOTOR_PWM_PIN_0 26
#define MOTOR_PWM_PIN_1 EOLO_PIN_UNUSED
#define MOTOR_PWM_PIN_COUNT 1
#define MOTOR_FG_PIN EOLO_PIN_UNUSED
#define MOTOR_POWER_PIN EOLO_PIN_UNUSED

#define PT_RX EOLO_PIN_UNUSED
#define PT_TX EOLO_PIN_UNUSED
#define PPH_PWR_PIN EOLO_PIN_UNUSED
#define MODEM_PWR_PIN EOLO_PIN_UNUSED
#define MODEM_RX_PIN EOLO_PIN_UNUSED
#define MODEM_TX_PIN EOLO_PIN_UNUSED

#define EOLO_PINOUT_COMPLETE 1

#endif
