#ifndef CAPTURE_SWITCHES_H
#define CAPTURE_SWITCHES_H

#include <Arduino.h>
#include <stdint.h>
#include "../Config.h"

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

class CaptureSwitches
{
public:
  static constexpr uint32_t kWaitSeconds[4] = {
      0,
      5UL * MINUTE,
      15UL * MINUTE,
      0
  };
  static constexpr uint32_t kDurationSeconds[4] = {
      0,
      5UL * MINUTE,
      15UL * MINUTE,
      DRONE_DURATION_INFINITE
  };

  CaptureSwitches(int waitSw0 = WAIT_SW0_PIN,
                  int waitSw1 = WAIT_SW1_PIN,
                  int durationSw0 = DURATION_SW0_PIN,
                  int durationSw1 = DURATION_SW1_PIN)
      : _waitSw0(waitSw0),
        _waitSw1(waitSw1),
        _durationSw0(durationSw0),
        _durationSw1(durationSw1)
  {
  }

  void begin() const
  {
    configureInput(_waitSw0);
    configureInput(_waitSw1);
    configureInput(_durationSw0);
    configureInput(_durationSw1);
  }

  CaptureSwitchSelection read() const
  {
    return decode(readCode(_waitSw0, _waitSw1), readCode(_durationSw0, _durationSw1));
  }

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

private:
  int _waitSw0;
  int _waitSw1;
  int _durationSw0;
  int _durationSw1;

  static bool supportsInternalPullup(int pin)
  {
    return pin < 34 || pin > 39;
  }

  static void configureInput(int pin)
  {
    // GPIO34-39, incluyendo los defaults 36/39 de duración, requieren pull-up/down externo.
    pinMode(pin, supportsInternalPullup(pin) ? INPUT_PULLUP : INPUT);
  }

  static uint8_t readBit(int pin)
  {
    return digitalRead(pin) == LOW ? 1 : 0;
  }

  static uint8_t readCode(int sw0Pin, int sw1Pin)
  {
    return (readBit(sw0Pin) << 0) | (readBit(sw1Pin) << 1);
  }
};

#endif
