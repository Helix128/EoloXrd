#ifndef HEADLESS_SETUP_TYPES_H
#define HEADLESS_SETUP_TYPES_H

#include <Arduino.h>
#include <stdint.h>
#include "../Config.h"
#include "../Data/Session.h"

struct HeadlessSetupConfig
{
  uint32_t waitSeconds = 0;
  uint32_t durationSeconds = 5UL * MINUTE;
  float targetFlow = DRONE_TARGET_FLOW_LPM;
};

namespace HeadlessSetup
{
  constexpr float kMinFlowLpm = 0.0f;
  constexpr float kMaxFlowLpm = 8.0f;
  constexpr uint32_t kMaxWaitSeconds = 24UL * HOUR;
  constexpr uint32_t kMaxDurationSeconds = 24UL * HOUR;

  inline bool shouldEnterWebSetup(uint8_t waitCode)
  {
    return (waitCode & 0x03) == 0;
  }

  inline bool validateConfig(const HeadlessSetupConfig &config)
  {
    if (config.waitSeconds > kMaxWaitSeconds)
      return false;

    if (config.durationSeconds != DRONE_DURATION_INFINITE &&
        (config.durationSeconds == 0 || config.durationSeconds > kMaxDurationSeconds))
      return false;

    if (isnan(config.targetFlow) ||
        config.targetFlow < kMinFlowLpm ||
        config.targetFlow > kMaxFlowLpm)
      return false;

    return true;
  }

  inline void applyToSession(const HeadlessSetupConfig &config, Session &session, const DateTime &now)
  {
    session.usePlantower = false;
    session.targetFlow = config.targetFlow;
    session.startDate = DateTime(now.unixtime() + config.waitSeconds);
    session.duration = config.durationSeconds;
    session.elapsedTime = 0;
    session.lastLog = 0;
    session.capturedVolume = 0.0f;
  }

  inline bool isSafeLogBasename(const char *name)
  {
    if (name == nullptr)
      return false;
    // must start with log_ and end with .csv
    const char *p = name;
    if (strncmp(p, "log_", 4) != 0)
      return false;
    size_t len = strlen(p);
    if (len < 5) // at least log_.csv
      return false;
    if (len < 4 + 4) // log_ + .csv
      return false;
    if (len < 8 || strcmp(p + len - 4, ".csv") != 0)
      return false;

    // disallow slashes or backrefs
    if (strstr(p, "/") != nullptr || strstr(p, "\\") != nullptr || strstr(p, "..") != nullptr)
      return false;

    for (size_t i = 0; i < len; i++)
    {
      char c = p[i];
      bool ok = (c >= 'A' && c <= 'Z') ||
                (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') ||
                c == '_' || c == '-' || c == '.';
      if (!ok)
        return false;
    }

    return true;
  }

  inline bool isSafeLogBasename(const String &name)
  {
    return isSafeLogBasename(name.c_str());
  }
}

#endif
