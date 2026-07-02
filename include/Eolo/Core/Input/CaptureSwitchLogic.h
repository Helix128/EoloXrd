#ifndef EOLO_CORE_INPUT_CAPTURE_SWITCH_LOGIC_H
#define EOLO_CORE_INPUT_CAPTURE_SWITCH_LOGIC_H

#include <stdint.h>
#include <Eolo/Types/CaptureSwitchData.h>

class CaptureSwitchLogic
{
public:
  static constexpr uint32_t MinuteSeconds = 60UL;
  static constexpr uint32_t InfiniteDurationSeconds = UINT32_MAX;
  static constexpr uint32_t kWaitSeconds[4] = {
      0,
      1UL * MinuteSeconds,
      5UL * MinuteSeconds,
      0
  };
  static constexpr uint32_t kDurationSeconds[4] = {
      0,
      5UL * MinuteSeconds,
      15UL * MinuteSeconds,
      InfiniteDurationSeconds
  };

  static CaptureSwitchSelection decode(uint8_t waitCode, uint8_t durationCode)
  {
    CaptureSwitchSelection selection;
    selection.waitCode = waitCode & 0x03;
    selection.durationCode = durationCode & 0x03;

    selection.waitSeconds = kWaitSeconds[selection.waitCode];
    selection.durationSeconds = kDurationSeconds[selection.durationCode];
    selection.waitEnabled = selection.waitCode != 0;
    selection.durationEnabled = selection.durationCode != 0;
    selection.instantStart = selection.waitCode == 0b11;
    selection.infiniteDuration = selection.durationCode == 0b11;
    selection.shouldStart = selection.waitEnabled && selection.durationEnabled;
    return selection;
  }

  static const char* waitDescription(uint8_t waitCode)
  {
    switch (waitCode & 0x03)
    {
    case 0b00:
      return "off";
    case 0b01:
      return "5 min";
    case 0b10:
      return "15 min";
    case 0b11:
      return "instantanea";
    default:
      return "desconocida";
    }
  }

  static const char* durationDescription(uint8_t durationCode)
  {
    switch (durationCode & 0x03)
    {
    case 0b00:
      return "off";
    case 0b01:
      return "5 min";
    case 0b10:
      return "15 min";
    case 0b11:
      return "infinita";
    default:
      return "desconocida";
    }
  }

  static const char* modeDescription(const CaptureSwitchSelection& selection)
  {
    if (!selection.waitEnabled)
      return "setup Wi-Fi";
    if (!selection.durationEnabled)
      return "idle";
    if (selection.instantStart)
      return "captura instantanea";
    return "captura programada";
  }
};

#endif
