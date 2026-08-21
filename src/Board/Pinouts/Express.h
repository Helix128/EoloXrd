#ifndef EOLO_BOARD_PINOUTS_EXPRESS_H
#define EOLO_BOARD_PINOUTS_EXPRESS_H

// EOLO Express: mapa completo de la demo secuencial de referencia.
#define SDA_PIN 21
#define SCL_PIN 22

#define SD_CS_PIN 5
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN 18

// OLED SPI por software. La colisión CS/RS485 sigue documentada como una
// decisión física pendiente; este mapa conserva el valor productivo actual.
#define EOLO_DISPLAY_CS_PIN 27
#define EOLO_DISPLAY_DC_PIN 15
#define EOLO_DISPLAY_RESET_PIN 2
#define EOLO_DISPLAY_SCK_PIN 18
#define EOLO_DISPLAY_MOSI_PIN 23

// Express no es headless.
#define WAIT_SW0_PIN EOLO_PIN_UNUSED
#define WAIT_SW1_PIN EOLO_PIN_UNUSED
#define DURATION_SW0_PIN EOLO_PIN_UNUSED
#define DURATION_SW1_PIN EOLO_PIN_UNUSED

#define NEOPIXEL_PIN EOLO_PIN_UNUSED
#define NEOPIXEL_COUNT 1
#define NEOPIXEL_BRIGHTNESS 60

#define NTC_PIN EOLO_PIN_UNUSED
#define BATTERY_ADC_PIN 34

// AFM07 RS485 (MAX485): RO(RX)=27, DI(TX)=25, DE/RE=26.
#define RS485_RX_PIN 27
#define RS485_TX_PIN 25
#define RS485_DE_RE_PIN 26

// Bombas DC (MOSFET low-side, PWM directo): Bomba1=32, Bomba2=33.
#define MOTOR_PWM_PIN_0 32
#define MOTOR_PWM_PIN_1 33
#define MOTOR_PWM_PIN_COUNT 2
#define MOTOR_FG_PIN EOLO_PIN_UNUSED
#define MOTOR_POWER_PIN EOLO_PIN_UNUSED

// PMS/Plantower (Serial2): ESP RX=17, ESP TX=16.
#define PT_RX 17
#define PT_TX 16
#define PPH_PWR_PIN 4

#define MODEM_PWR_PIN EOLO_PIN_UNUSED
#define MODEM_RX_PIN EOLO_PIN_UNUSED
#define MODEM_TX_PIN EOLO_PIN_UNUSED

#define EOLO_PINOUT_COMPLETE 1

#endif
