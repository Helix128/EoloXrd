#ifndef EOLO_TYPES_CAPTURE_SWITCH_DATA_H
#define EOLO_TYPES_CAPTURE_SWITCH_DATA_H

#include <stdint.h>

struct CaptureSwitchSelection
{
  uint8_t waitCode = 0;
  uint8_t durationCode = 0;
  uint32_t waitSeconds = 0;
  uint32_t durationSeconds = 0;
  bool waitEnabled = false;
  bool durationEnabled = false;
  bool instantStart = false;
  bool infiniteDuration = false;
  bool shouldStart = false;
};

struct CaptureSwitchPinState
{
  const char* name = "";
  int pin = -1;
  int level = 1;
  uint8_t bit = 0;
};

struct CaptureSwitchSnapshot
{
  CaptureSwitchPinState waitSw0;
  CaptureSwitchPinState waitSw1;
  CaptureSwitchPinState durationSw0;
  CaptureSwitchPinState durationSw1;
  uint8_t waitCode = 0;
  uint8_t durationCode = 0;
  CaptureSwitchSelection selection;
};

#endif
