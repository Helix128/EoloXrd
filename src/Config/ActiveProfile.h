#ifndef EOLO_CONFIG_ACTIVE_PROFILE_H
#define EOLO_CONFIG_ACTIVE_PROFILE_H

#if (defined(EOLO_TARGET_DRON) + defined(EOLO_TARGET_STANDARD) + \
     defined(EOLO_TARGET_EXPRESS) + defined(EOLO_TARGET_EXPRESS_LEGACY)) != 1
  #error "Define exactamente un target EOLO_TARGET_* en platformio.ini"
#endif

#if defined(EOLO_TARGET_DRON)
  #include "Profiles/Dron.h"
#elif defined(EOLO_TARGET_STANDARD)
  #include "Profiles/Standard.h"
#elif defined(EOLO_TARGET_EXPRESS_LEGACY)
  #include "Profiles/ExpressLegacy.h"
#else
  #include "Profiles/Express.h"
#endif

#include "../Board/Pinout.h"

namespace EoloConfig
{
inline constexpr const char *kModelName = Profile::kModelName;
inline constexpr uint32_t i2cClockHz = Profile::kI2cClockHz;
inline constexpr uint32_t displaySpiClockHz = Profile::kDisplaySpiClockHz;
inline constexpr uint8_t attinyAddress = Profile::kAttinyAddress;
inline constexpr bool motorPwmInverted = Profile::kMotorPwmInverted;
inline constexpr uint8_t motorPwmResolutionBits = Profile::kMotorPwmResolutionBits;
inline constexpr int motorRampStep = Profile::kMotorRampStep;
inline constexpr uint32_t motorRampIntervalMs = Profile::kMotorRampIntervalMs;
inline constexpr ModemPowerMode modemPowerMode = Profile::kModemPowerMode;
inline constexpr int flowPidInitialPwm = Profile::kFlowPidInitialPwm;
inline constexpr uint32_t i2cWarmupMs = Profile::kI2cWarmupMs;
inline constexpr bool encoderInverted = Profile::kEncoderInverted;

inline constexpr int displayCsPin = EOLO_DISPLAY_CS_PIN;
inline constexpr int displayDcPin = EOLO_DISPLAY_DC_PIN;
inline constexpr int displayResetPin = EOLO_DISPLAY_RESET_PIN;
inline constexpr int displayClockPin = EOLO_DISPLAY_SCK_PIN;
inline constexpr int displayMosiPin = EOLO_DISPLAY_MOSI_PIN;

inline constexpr float motorOverheatHighC = Profile::kMotorOverheatHighC;
inline constexpr float motorOverheatLowC = Profile::kMotorOverheatLowC;
inline constexpr uint32_t motorOverheatLogIntervalMs = Profile::kMotorOverheatLogIntervalMs;

inline constexpr const char *modemApn = Profile::kModemApn;
inline constexpr const char *rtcTimeServerUrl = Profile::kRtcTimeServerUrl;
inline constexpr bool modemAllowHttpFallback = Profile::kModemAllowHttpFallback;

inline constexpr float afm07FlowDivisor = Profile::kAfm07FlowDivisor;
inline constexpr float droneTargetFlowLpm = Profile::kDroneTargetFlowLpm;

inline constexpr FlowPidConfig flowPid = Profile::kFlowPid;
inline constexpr bool useCalibrationSeed = Profile::kUseCalibrationSeed;
}

#include "VariantValidation.h"

#endif
