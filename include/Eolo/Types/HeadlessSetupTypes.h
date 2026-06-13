#ifndef EOLO_TYPES_HEADLESS_SETUP_TYPES_H
#define EOLO_TYPES_HEADLESS_SETUP_TYPES_H

#include <math.h>
#include <stdint.h>
#include <string.h>
#include <Eolo/Types/Session.h>

struct HeadlessSetupConfig
{
  uint32_t waitSeconds = 0;
  uint32_t durationSeconds = 5UL * 60UL;
  float targetFlow = 5.0f;
  uint8_t flowSectionCount = 0;
  FlowSection flowSections[MaxFlowSections];
};

struct HeadlessSetupPreset
{
  char name[24] = "";
  HeadlessSetupConfig config;
};

namespace HeadlessSetup
{
  constexpr float kMinFlowLpm = 0.0f;
  constexpr float kMaxFlowLpm = 8.0f;
  constexpr uint32_t kMaxWaitSeconds = 24UL * 60UL * 60UL;
  constexpr uint32_t kMaxDurationSeconds = 24UL * 60UL * 60UL;
  constexpr uint32_t kInfiniteDuration = UINT32_MAX;
  constexpr uint8_t kMaxPresets = 8;
  constexpr size_t kMaxPresetNameLength = sizeof(HeadlessSetupPreset::name) - 1;

  inline bool shouldEnterWebSetup(uint8_t waitCode)
  {
    return (waitCode & 0x03) == 0;
  }

  inline bool validateConfig(const HeadlessSetupConfig &config)
  {
    if (config.waitSeconds > kMaxWaitSeconds)
      return false;

    if (config.durationSeconds != kInfiniteDuration &&
        (config.durationSeconds == 0 || config.durationSeconds > kMaxDurationSeconds))
      return false;

    if (isnan(config.targetFlow) || config.targetFlow < kMinFlowLpm || config.targetFlow > kMaxFlowLpm)
      return false;

    if (config.flowSectionCount > MaxFlowSections)
      return false;

    uint32_t sectionDuration = 0;
    for (uint8_t i = 0; i < config.flowSectionCount; i++)
    {
      const FlowSection &section = config.flowSections[i];
      if (section.durationSeconds == 0)
        return false;
      if (isnan(section.targetFlow) || section.targetFlow < kMinFlowLpm || section.targetFlow > kMaxFlowLpm)
        return false;
      sectionDuration += section.durationSeconds;
    }

    if (config.durationSeconds != kInfiniteDuration && sectionDuration > config.durationSeconds)
      return false;

    return true;
  }

  inline void applyToSession(const HeadlessSetupConfig &config, Session &session, const DateTime &now)
  {
    session.usePlantower = false;
    session.targetFlow = config.flowSectionCount > 0 ? config.flowSections[0].targetFlow : config.targetFlow;
    session.startDate = DateTime(now.unixtime() + config.waitSeconds);
    session.duration = config.durationSeconds;
    session.elapsedTime = 0;
    session.lastLog = 0;
    session.capturedVolume = 0.0f;
    session.flowSectionCount = config.flowSectionCount;
    for (uint8_t i = 0; i < MaxFlowSections; i++)
      session.flowSections[i] = i < config.flowSectionCount ? config.flowSections[i] : FlowSection();
  }

  inline bool isSafeLogBasename(const char *name)
  {
    if (name == nullptr)
      return false;
    const char *p = name;
    if (strncmp(p, "log_", 4) != 0)
      return false;
    size_t len = strlen(p);
    if (len < 8 || strcmp(p + len - 4, ".csv") != 0)
      return false;
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

  inline bool isSafePresetName(const char *name)
  {
    if (name == nullptr)
      return false;
    size_t len = strlen(name);
    if (len == 0 || len > kMaxPresetNameLength)
      return false;
    if (strstr(name, "/") != nullptr || strstr(name, "\\") != nullptr || strstr(name, "..") != nullptr)
      return false;

    for (size_t i = 0; i < len; i++)
    {
      char c = name[i];
      bool ok = (c >= 'A' && c <= 'Z') ||
                (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') ||
                c == '_' || c == '-';
      if (!ok)
        return false;
    }

    return true;
  }
}

#endif
