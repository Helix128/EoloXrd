#ifndef EOLO_DEMO_PINOUT_H
#define EOLO_DEMO_PINOUT_H

#define EOLO_PIN_UNUSED -1

#if defined(EOLO_MODEL_DRON)
static const int I2C_SDA_PIN = 21;
static const int I2C_SCL_PIN = 22;
static const int SD_CS_PIN = 5;

static const int DIP_PIN_1 = 32;
static const int DIP_PIN_2 = 33;
static const int DIP_PIN_3 = 14;
static const int DIP_PIN_4 = 13;

static const int NEOPIXEL_PIN = 27;

static const int RS485_RX_PIN = 16;
static const int RS485_TX_PIN = 17;
static const int RS485_DE_RE_PIN = 4;

static const int MOTOR_PWM_PIN = 26;
static const int MOTOR_FG_PIN = 35;

static const int PT_RX_PIN = EOLO_PIN_UNUSED;
static const int PT_TX_PIN = EOLO_PIN_UNUSED;
static const int PPH_PWR_PIN = EOLO_PIN_UNUSED;
static const int MODEM_PWR_PIN = EOLO_PIN_UNUSED;
#elif defined(EOLO_MODEL_STANDARD) || defined(EOLO_MODEL_EXPRESS) || defined(EOLO_MODEL_EXPRESS_LEGACY)
static const int I2C_SDA_PIN = 21;
static const int I2C_SCL_PIN = 22;
static const int SD_CS_PIN = 5;

static const int DIP_PIN_1 = 32;
static const int DIP_PIN_2 = 14;
static const int DIP_PIN_3 = 36;
static const int DIP_PIN_4 = 39;

static const int NEOPIXEL_PIN = 27;

static const int RS485_RX_PIN = 35;
static const int RS485_TX_PIN = 33;
static const int RS485_DE_RE_PIN = 26;

static const int MOTOR_PWM_PIN = 25;
static const int MOTOR_FG_PIN = EOLO_PIN_UNUSED;

static const int PT_RX_PIN = 34;
static const int PT_TX_PIN = 32;
static const int PPH_PWR_PIN = 4;
static const int MODEM_PWR_PIN = 13;
#else
#error "Define EOLO_MODEL_DRON, EOLO_MODEL_STANDARD, EOLO_MODEL_EXPRESS o EOLO_MODEL_EXPRESS_LEGACY antes de incluir EoloDemoPinout.h"
#endif

static inline void enableDemoPeripheralPower()
{
  if (PPH_PWR_PIN >= 0) {
    pinMode(PPH_PWR_PIN, OUTPUT);
    digitalWrite(PPH_PWR_PIN, HIGH);
  }
}

#endif
