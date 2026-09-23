#ifndef EOLO_CONFIG_LEGACY_H
#define EOLO_CONFIG_LEGACY_H

// Transitional names for application code that has not yet been migrated to
// EoloConfig. New code must include ActiveProfile.h and use the typed values.
#include "ActiveProfile.h"
#include "../Utility/Log.h"
#include "../Utility/DebugFlags.h"

#define CHECK_SENSORS 0
#define BAREBONES 0
#define SERIAL_INPUT 0

#define DisplayModel EOLO_DISPLAY_MODEL
#define I2C_CLOCK (EoloConfig::i2cClockHz)
#define I2C_WARMUP_MS (EoloConfig::i2cWarmupMs)
#if defined(EOLO_TARGET_STANDARD)
  // La demo secuencial estable usa 50 ms. Los ProMini del bus pueden hacer
  // clock stretching y 20 ms resultó demasiado agresivo en Standard.
  #define I2C_TRANSACTION_TIMEOUT_MS 50UL
#else
  #define I2C_TRANSACTION_TIMEOUT_MS 20UL
#endif
#define ATTINY_ADDRESS (EoloConfig::attinyAddress)
#define ENCODER_INVERTED (EoloConfig::encoderInverted)
#define CMD_RESET_COUNTER 0x01
#define CMD_RESET_BUTTON 0x02

#define MINUTE (EoloConfig::kMinute)

#define DRONE_TARGET_FLOW_LPM (EoloConfig::droneTargetFlowLpm)
#define AFM07_FLOW_DIVISOR (EoloConfig::afm07FlowDivisor)
#define NTC_MOTOR_OVERHEAT_HIGH_C (EoloConfig::motorOverheatHighC)
#define NTC_MOTOR_OVERHEAT_LOW_C (EoloConfig::motorOverheatLowC)
#define NTC_MOTOR_OVERHEAT_LOG_INTERVAL_MS (EoloConfig::motorOverheatLogIntervalMs)
#define MOTOR_RAMP_STEP (EoloConfig::motorRampStep)

#define DRONE_DURATION_INFINITE UINT32_MAX

#define FONT_BOLD u8g2_font_helvB10_tf
#define FONT_REGULAR u8g2_font_helvR10_tf
#define FONT_BOLD_S u8g2_font_helvB08_tf
#define FONT_REGULAR_S u8g2_font_helvR08_tf
#define FONT_BOLD_L u8g2_font_helvB12_tf

#define MODEM_DEFAULT_APN (EoloConfig::modemApn)
#define RTC_TIME_SERVER_URL (EoloConfig::rtcTimeServerUrl)
#endif
