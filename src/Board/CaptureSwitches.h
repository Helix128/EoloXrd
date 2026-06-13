#ifndef CAPTURE_SWITCHES_H
#define CAPTURE_SWITCHES_H

#include <Arduino.h>
#include <stdint.h>
#include "../Config.h"
#include <Eolo/Core/Input/CaptureSwitchLogic.h>

class CaptureSwitches
{
public:
  static constexpr const uint32_t *kWaitSeconds = CaptureSwitchLogic::kWaitSeconds;
  static constexpr const uint32_t *kDurationSeconds = CaptureSwitchLogic::kDurationSeconds;

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
    return snapshot().selection;
  }

  CaptureSwitchSnapshot snapshot() const
  {
    CaptureSwitchSnapshot snap;
    snap.waitSw0 = readPin("WAIT_SW0", _waitSw0);
    snap.waitSw1 = readPin("WAIT_SW1", _waitSw1);
    snap.durationSw0 = readPin("DURATION_SW0", _durationSw0);
    snap.durationSw1 = readPin("DURATION_SW1", _durationSw1);

    snap.waitCode = (snap.waitSw0.bit << 0) | (snap.waitSw1.bit << 1);
    snap.durationCode = (snap.durationSw0.bit << 0) | (snap.durationSw1.bit << 1);
    snap.selection = decode(snap.waitCode, snap.durationCode);
    return snap;
  }

  static CaptureSwitchSelection decode(uint8_t waitCode, uint8_t durationCode)
  {
    return CaptureSwitchLogic::decode(waitCode, durationCode);
  }

  static const char* waitDescription(uint8_t waitCode)
  {
    return CaptureSwitchLogic::waitDescription(waitCode);
  }

  static const char* durationDescription(uint8_t durationCode)
  {
    return CaptureSwitchLogic::durationDescription(durationCode);
  }

  static const char* modeDescription(const CaptureSwitchSelection& selection)
  {
    return CaptureSwitchLogic::modeDescription(selection);
  }

  static void printSummary(Print& out, const CaptureSwitchSnapshot& snap)
  {
    out.printf("Switches: espera %s | duracion %s | modo %s\n",
               waitDescription(snap.waitCode),
               durationDescription(snap.durationCode),
               modeDescription(snap.selection));
  }

  static void printSnapshot(Print& out, const CaptureSwitchSnapshot& snap)
  {
    printSummary(out, snap);
    out.println("Detalle switches:");
    printPin(out, snap.waitSw0);
    printPin(out, snap.waitSw1);
    printPin(out, snap.durationSw0);
    printPin(out, snap.durationSw1);
    out.println("Parseado:");
    out.printf("  espera: %s (code %u%u)\n",
               waitDescription(snap.waitCode),
               (unsigned int)((snap.waitCode >> 1) & 0x01),
               (unsigned int)(snap.waitCode & 0x01));
    out.printf("  duracion: %s (code %u%u)\n",
               durationDescription(snap.durationCode),
               (unsigned int)((snap.durationCode >> 1) & 0x01),
               (unsigned int)(snap.durationCode & 0x01));
    out.printf("  shouldStart: %s\n", snap.selection.shouldStart ? "si" : "no");
    out.printf("  waitSeconds: %lu\n", (unsigned long)snap.selection.waitSeconds);
    out.printf("  durationSeconds: %lu\n", (unsigned long)snap.selection.durationSeconds);
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
    // GPIO34-39 no tienen pull-up interno; se configuran como entrada simple.
    pinMode(pin, supportsInternalPullup(pin) ? INPUT_PULLUP : INPUT);
  }

  static CaptureSwitchPinState readPin(const char* name, int pin)
  {
    CaptureSwitchPinState state;
    state.name = name;
    state.pin = pin;
    state.level = digitalRead(pin);
    state.bit = state.level == LOW ? 1 : 0;
    return state;
  }

  static const char* levelDescription(int level)
  {
    return level == LOW ? "LOW" : "HIGH";
  }

  static void printPin(Print& out, const CaptureSwitchPinState& state)
  {
    out.printf("  %s pin %d nivel %s bit %u\n",
               state.name,
               state.pin,
               levelDescription(state.level),
               (unsigned int)state.bit);
  }
};

#endif
