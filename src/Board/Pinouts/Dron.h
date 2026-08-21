#ifndef EOLO_BOARD_PINOUTS_DRON_H
#define EOLO_BOARD_PINOUTS_DRON_H

// EOLO Dron: mapa completo. No heredar pines de otro modelo.
#define SDA_PIN 21
#define SCL_PIN 22

#define SD_CS_PIN 5
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN 18

// El Dron no monta display; se mantienen sentinelas para que el mapa sea
// completo y auditable sin heredar valores de una variante UI.
#define EOLO_DISPLAY_CS_PIN EOLO_PIN_UNUSED
#define EOLO_DISPLAY_DC_PIN EOLO_PIN_UNUSED
#define EOLO_DISPLAY_RESET_PIN EOLO_PIN_UNUSED
#define EOLO_DISPLAY_SCK_PIN EOLO_PIN_UNUSED
#define EOLO_DISPLAY_MOSI_PIN EOLO_PIN_UNUSED

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

#define EOLO_PINOUT_COMPLETE 1

#endif
