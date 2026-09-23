#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <Arduino.h>
#include <math.h>
#include "../Config/Legacy.h"

#ifdef FEATURE_NEOPIXEL
#include <Adafruit_NeoPixel.h>
#ifndef NEOPIXEL_COLOR_ORDER
#define NEOPIXEL_COLOR_ORDER NEO_RGB
#endif
#endif

enum class StatusLedPattern : uint8_t
{
  Off,
  Boot,
  Setup,
  Waiting,
  Capturing,
  Busy,
  MotorOverheat,
  Error,
  Finished
};

class StatusLed
{
public:
  void begin()
  {
#ifdef FEATURE_NEOPIXEL
    if (_ready)
      return;

    _pixel.begin();
    _pixel.setBrightness(NEOPIXEL_BRIGHTNESS);
    _pixel.clear();
    _pixel.show();
#ifdef EOLO_TARGET_DRON
    _cycleStartedAtMs = millis();
#endif
    _hasLastShownColor = false;
    _ready = true;
#ifdef NEOPIXEL_STRENGTH_PERCENT
    setStrengthPercent(NEOPIXEL_STRENGTH_PERCENT);
#endif
#endif
  }

  // Scales the existing NeoPixel brightness without changing the color profiles.
  // 100 preserves the configured brightness; 0 turns off its light output.
  void setStrengthPercent(unsigned int strengthPercent)
  {
#ifdef FEATURE_NEOPIXEL
    _strengthPercent = static_cast<uint8_t>(strengthPercent > 100U ? 100U : strengthPercent);
    poll(true);
#else
    (void)strengthPercent;
#endif
  }

  uint8_t strengthPercent() const
  {
#ifdef FEATURE_NEOPIXEL
    return _strengthPercent;
#else
    return 100;
#endif
  }

  void setPattern(StatusLedPattern pattern)
  {
#ifdef FEATURE_NEOPIXEL
    if (_pattern == pattern)
      return;

    _pattern = pattern;
    resetSequence();
    poll(true);
#else
    (void)pattern;
#endif
  }

#if defined(FEATURE_NEOPIXEL) && defined(EOLO_TARGET_DRON)
  void setDronePresentation(StatusLedPattern pattern, bool temperatureValid, float temperatureC)
  {
    if (_pattern != pattern)
    {
      _pattern = pattern;
      resetSequence();
    }
    _motorTemperatureValid = temperatureValid && isfinite(temperatureC);
    _motorTemperatureC = _motorTemperatureValid ? temperatureC : 0.0f;
    poll(true);
  }

  void setDroneTerminalError()
  {
    if (!_terminalErrorLatched)
    {
      _pattern = StatusLedPattern::Error;
      _terminalErrorLatched = true;
      resetSequence();
    }
    poll(true);
  }
#endif

  StatusLedPattern pattern() const
  {
#ifdef FEATURE_NEOPIXEL
    return _pattern;
#else
    return StatusLedPattern::Off;
#endif
  }

  void poll(bool force = false)
  {
#ifdef FEATURE_NEOPIXEL
    if (!_ready)
      return;

#ifdef EOLO_TARGET_DRON
    (void)force;
    pollDroneCycle(millis());
#else
    PatternProfile profile = profileFor(_pattern);
    uint32_t now = millis();

    if (profile.flashes == 0)
    {
      if (force || _lastStepMs == 0)
      {
        show(profile.primary);
        _lastStepMs = now;
      }
      return;
    }

    if (force || _lastStepMs == 0)
    {
      _lit = true;
      _flashIndex = 0;
      _lastStepMs = now;
      show(profile.primary);
      return;
    }

    uint32_t duration = _lit ? profile.onMs : offDuration(profile);
    if (now - _lastStepMs < duration)
      return;

    _lastStepMs = now;
    if (_lit)
    {
      _lit = false;
      show(profile.secondary);
      return;
    }

    if (_flashIndex + 1 >= profile.flashes)
      _flashIndex = 0;
    else
      _flashIndex++;

    _lit = true;
    show(profile.primary);
#endif
#else
    (void)force;
#endif
  }

private:
#ifdef FEATURE_NEOPIXEL
  struct Color
  {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  };

  struct PatternProfile
  {
    Color primary;
    Color secondary;
    uint16_t onMs;
    uint16_t offMs;
    uint16_t gapMs;
    uint8_t flashes;
  };

  Adafruit_NeoPixel _pixel = Adafruit_NeoPixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEOPIXEL_COLOR_ORDER + NEO_KHZ800);
  StatusLedPattern _pattern = StatusLedPattern::Off;
  bool _ready = false;
  bool _lit = false;
  uint8_t _flashIndex = 0;
  uint8_t _strengthPercent = 100;
  uint32_t _lastStepMs = 0;
  bool _hasLastShownColor = false;
  Color _lastShownColor{0, 0, 0};
#ifdef EOLO_TARGET_DRON
  uint32_t _cycleStartedAtMs = 0;
  bool _terminalErrorLatched = false;
  bool _motorTemperatureValid = false;
  float _motorTemperatureC = 0.0f;
#endif

  void resetSequence()
  {
    _lit = false;
    _flashIndex = 0;
    _lastStepMs = 0;
  }

  uint32_t offDuration(const PatternProfile &profile) const
  {
    return (_flashIndex + 1 >= profile.flashes) ? profile.gapMs : profile.offMs;
  }

  void show(Color color)
  {
    color.r = scaleStrength(color.r);
    color.g = scaleStrength(color.g);
    color.b = scaleStrength(color.b);
    if (_hasLastShownColor && color.r == _lastShownColor.r &&
        color.g == _lastShownColor.g && color.b == _lastShownColor.b)
      return;

    _pixel.setPixelColor(0, _pixel.Color(color.r, color.g, color.b));
    _pixel.show();
    _lastShownColor = color;
    _hasLastShownColor = true;
  }

  uint8_t scaleStrength(uint8_t value) const
  {
    return static_cast<uint8_t>((static_cast<uint16_t>(value) * _strengthPercent + 50U) / 100U);
  }

#ifdef EOLO_TARGET_DRON
  void pollDroneCycle(uint32_t now)
  {
    if (_terminalErrorLatched)
    {
      show(profileFor(StatusLedPattern::Error).primary);
      return;
    }

    static constexpr uint32_t kCycleMs = 4000UL;
    const uint32_t phase = (now - _cycleStartedAtMs) % kCycleMs;
    if (phase < 2000UL)
    {
      show(Color{0, 0, 0});
      return;
    }

    const PatternProfile profile = profileFor(_pattern);
    if (phase < 3000UL)
    {
      show(profile.primary);
      return;
    }

    if (!_motorTemperatureValid)
    {
      show(profileFor(StatusLedPattern::Setup).primary);
      return;
    }

    if (_motorTemperatureC > 65.0f)
    {
      const uint32_t thermalPhase = phase - 3000UL;
      const bool blinkOn = ((thermalPhase / 100UL) % 2UL) == 0UL;
      show(blinkOn ? temperatureRed() : Color{0, 0, 0});
      return;
    }

    show(temperatureColor(_motorTemperatureC));
  }

  static uint8_t temperaturePeak()
  {
#ifdef STATUS_LED_LOW_POWER
    return 45;
#else
    return 55;
#endif
  }

  static Color temperatureGreen()
  {
    return Color{0, temperaturePeak(), 0};
  }

  static Color temperatureYellow()
  {
    const uint8_t peak = temperaturePeak();
    return Color{peak, peak, 0};
  }

  static Color temperatureRed()
  {
    return Color{temperaturePeak(), 0, 0};
  }

  static Color temperatureColor(float temperatureC)
  {
    const uint8_t peak = temperaturePeak();
    if (temperatureC <= 30.0f)
      return temperatureGreen();
    if (temperatureC < 40.0f)
      return Color{static_cast<uint8_t>(peak / 4U), peak, 0};
    if (temperatureC < 50.0f)
      return temperatureYellow();
    if (temperatureC < 60.0f)
      return Color{peak, static_cast<uint8_t>(peak / 2U), 0};
    return temperatureRed();
  }
#endif

  static PatternProfile profileFor(StatusLedPattern pattern)
  {
#ifdef STATUS_LED_LOW_POWER
    return lowPowerProfileFor(pattern);
#else
    return normalProfileFor(pattern);
#endif
  }

  static PatternProfile normalProfileFor(StatusLedPattern pattern)
  {
    switch (pattern)
    {
    case StatusLedPattern::Boot:
      return PatternProfile{Color{0, 18, 48}, Color{0, 3, 10}, 350, 0, 350, 1};
    case StatusLedPattern::Setup:
      return PatternProfile{Color{38, 0, 48}, Color{6, 0, 10}, 450, 0, 450, 1};
    case StatusLedPattern::Waiting:
      return PatternProfile{Color{42, 24, 0}, Color{0, 0, 0}, 220, 0, 1000, 1};
    case StatusLedPattern::Capturing:
      return PatternProfile{Color{0, 42, 8}, Color{0, 7, 1}, 900, 0, 900, 1};
    case StatusLedPattern::Busy:
      return PatternProfile{Color{0, 28, 42}, Color{0, 0, 0}, 140, 0, 140, 1};
    case StatusLedPattern::MotorOverheat:
      return PatternProfile{Color{55, 10, 0}, Color{0, 0, 0}, 160, 120, 900, 3};
    case StatusLedPattern::Error:
      return PatternProfile{Color{55, 0, 0}, Color{0, 0, 0}, 220, 0, 220, 1};
    case StatusLedPattern::Finished:
      return PatternProfile{Color{24, 24, 24}, Color{0, 0, 0}, 0, 0, 0, 0};
    case StatusLedPattern::Off:
    default:
      return PatternProfile{Color{0, 0, 0}, Color{0, 0, 0}, 0, 0, 0, 0};
    }
  }

  static PatternProfile lowPowerProfileFor(StatusLedPattern pattern)
  {
    switch (pattern)
    {
    case StatusLedPattern::Boot:
      return PatternProfile{Color{0, 10, 28}, Color{0, 0, 0}, 70, 0, 3000, 1};
    case StatusLedPattern::Setup:
      return PatternProfile{Color{22, 0, 30}, Color{0, 0, 0}, 80, 0, 2200, 1};
    case StatusLedPattern::Waiting:
      return PatternProfile{Color{28, 14, 0}, Color{0, 0, 0}, 60, 0, 5000, 1};
    case StatusLedPattern::Capturing:
      return PatternProfile{Color{0, 24, 5}, Color{0, 0, 0}, 70, 0, 4000, 1};
    case StatusLedPattern::Busy:
      return PatternProfile{Color{0, 18, 28}, Color{0, 0, 0}, 50, 150, 2500, 2};
    case StatusLedPattern::MotorOverheat:
      return PatternProfile{Color{45, 8, 0}, Color{0, 0, 0}, 80, 160, 1200, 3};
    case StatusLedPattern::Error:
      return PatternProfile{Color{45, 0, 0}, Color{0, 0, 0}, 120, 120, 800, 3};
    case StatusLedPattern::Finished:
      return PatternProfile{Color{18, 18, 18}, Color{0, 0, 0}, 120, 0, 8000, 1};
    case StatusLedPattern::Off:
    default:
      return PatternProfile{Color{0, 0, 0}, Color{0, 0, 0}, 0, 0, 0, 0};
    }
  }
#endif
};

#endif
