#ifndef EOLO_DEMO_PINOUT_H
#define EOLO_DEMO_PINOUT_H

#if defined(EOLO_MODEL_DRON)
#define EOLO_TARGET_DRON
#elif defined(EOLO_MODEL_STANDARD)
#define EOLO_TARGET_STANDARD
#elif defined(EOLO_MODEL_EXPRESS)
#define EOLO_TARGET_EXPRESS
#elif defined(EOLO_MODEL_EXPRESS_LEGACY)
#define EOLO_TARGET_EXPRESS_LEGACY
#else
#error "Define un EOLO_MODEL_* antes de incluir EoloDemoPinout.h"
#endif

// Las demos comparten la fuente autoritativa del firmware.
#include "../src/Board/Pinout.h"
#include "../src/Config/ActiveProfile.h"

static constexpr int I2C_SDA_PIN = SDA_PIN;
static constexpr int I2C_SCL_PIN = SCL_PIN;
static constexpr int DIP_PIN_1 = WAIT_SW0_PIN;
static constexpr int DIP_PIN_2 = WAIT_SW1_PIN;
static constexpr int DIP_PIN_3 = DURATION_SW0_PIN;
static constexpr int DIP_PIN_4 = DURATION_SW1_PIN;
static constexpr int MOTOR_PWM_PIN_A = MOTOR_PWM_PIN_0;
static constexpr int MOTOR_PWM_PIN_B = MOTOR_PWM_PIN_1;
// Alias histórico para demos de un solo motor.
static constexpr int MOTOR_PWM_PIN = MOTOR_PWM_PIN_A;
static constexpr int PT_RX_PIN = PT_RX;
static constexpr int PT_TX_PIN = PT_TX;
static constexpr int DISPLAY_CS_PIN = EoloConfig::board.displayCsPin;
static constexpr int DISPLAY_DC_PIN = EoloConfig::board.displayDcPin;
static constexpr int DISPLAY_RESET_PIN = EoloConfig::board.displayResetPin;
static constexpr int DISPLAY_SCK_PIN = EoloConfig::board.displayClockPin;
static constexpr int DISPLAY_MISO_PIN = SD_MISO_PIN;
static constexpr int DISPLAY_MOSI_PIN = EoloConfig::board.displayMosiPin;

static inline void enableDemoPeripheralPower()
{
  if (PPH_PWR_PIN >= 0)
  {
    pinMode(PPH_PWR_PIN, OUTPUT);
    digitalWrite(PPH_PWR_PIN, HIGH);
  }
}

#endif
