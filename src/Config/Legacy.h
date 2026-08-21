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
#define HOUR (EoloConfig::kHour)
#define DAY (EoloConfig::kDay)

#define DRONE_TARGET_FLOW_LPM (EoloConfig::droneTargetFlowLpm)
#define AFM07_FLOW_DIVISOR (EoloConfig::afm07FlowDivisor)
#define NTC_MOTOR_OVERHEAT_HIGH_C (EoloConfig::motorOverheatHighC)
#define NTC_MOTOR_OVERHEAT_LOW_C (EoloConfig::motorOverheatLowC)
#define NTC_MOTOR_OVERHEAT_LOG_INTERVAL_MS (EoloConfig::motorOverheatLogIntervalMs)
#define MOTOR_RAMP_STEP (EoloConfig::motorRampStep)
#define MOTOR_RAMP_INTERVAL_MS (EoloConfig::motorRampIntervalMs)
#define MOTOR_PWM_RESOLUTION_BITS (EoloConfig::motorPwmResolutionBits)

#define FLOW_PID_INTERVAL_MS (EoloConfig::flowPid.intervalMs)
#define FLOW_PID_DEADBAND (EoloConfig::flowPid.deadband)
#define FLOW_PID_KP (EoloConfig::flowPid.kp)
#define FLOW_PID_KI (EoloConfig::flowPid.ki)
#define FLOW_PID_KD (EoloConfig::flowPid.kd)
#define FLOW_PID_INTEGRAL_LIMIT (EoloConfig::flowPid.integralLimit)
#define FLOW_PID_MAX_STEP (EoloConfig::flowPid.maxStep)
#define FLOW_PID_BASE_PWM (EoloConfig::flowPidInitialPwm)
#define FLOW_PID_FILTER_ALPHA (EoloConfig::flowPid.filterAlpha)
#define FLOW_PID_MIN_ACTIVE (EoloConfig::flowPid.minActive)
#define FLOW_PID_MAX_DT_MS (EoloConfig::flowPid.maxDtMs)
#define FLOW_PID_SENSOR_STALE_MS (EoloConfig::flowPid.sensorStaleMs)
#define FLOW_PID_KICK_PWM (EoloConfig::flowPid.kickPwm)
#define FLOW_PID_KICK_MS (EoloConfig::flowPid.kickMs)
#define FLOW_PID_STALL_FLOW_LPM (EoloConfig::flowPid.stallFlowLpm)
#define FLOW_PID_RESTALL_COOLDOWN_MS (EoloConfig::flowPid.restallCooldownMs)
#define FLOW_PID_STALL_CONFIRM_MS (EoloConfig::flowPid.stallConfirmMs)
#define FLOW_PID_SOFT_TRIM_MAX (EoloConfig::flowPid.softTrimMax)
#define FLOW_PID_SOFT_MAX_STEP (EoloConfig::flowPid.softMaxStep)
#define FLOW_PID_SENSITIVITY (EoloConfig::flowPid.sensitivity)
#define FLOW_PID_RECENTER_DELAY_MS (EoloConfig::flowPid.recenterDelayMs)

#define DRONE_DURATION_INFINITE UINT32_MAX

#define FONT_BOLD u8g2_font_helvB10_tf
#define FONT_REGULAR u8g2_font_helvR10_tf
#define FONT_BOLD_S u8g2_font_helvB08_tf
#define FONT_REGULAR_S u8g2_font_helvR08_tf
#define FONT_BOLD_L u8g2_font_helvB12_tf
#define FONT_REGULAR_L u8g2_font_helvR12_tf

#define MODEM_DEFAULT_APN (EoloConfig::modemApn)
#define RTC_TIME_SERVER_URL (EoloConfig::rtcTimeServerUrl)
#endif
