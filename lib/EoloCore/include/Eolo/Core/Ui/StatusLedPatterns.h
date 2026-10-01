#ifndef EOLO_CORE_UI_STATUS_LED_PATTERNS_H
#define EOLO_CORE_UI_STATUS_LED_PATTERNS_H

#include <stdint.h>

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

// Colores y cadencias del LED de estado, independientes del hardware. El modo
// de bajo consumo (`lowPower`) atenúa colores y espacia los destellos.
namespace StatusLedPalette
{
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

    inline uint8_t temperaturePeak(bool lowPower)
    {
        return lowPower ? 45 : 55;
    }

    inline Color temperatureRed(bool lowPower)
    {
        return Color{temperaturePeak(lowPower), 0, 0};
    }

    // Verde hasta 30 °C, pasando por amarillo y naranja, rojo desde 60 °C.
    inline Color temperatureColor(float temperatureC, bool lowPower)
    {
        const uint8_t peak = temperaturePeak(lowPower);
        if (temperatureC <= 30.0f)
            return Color{0, peak, 0};
        if (temperatureC < 40.0f)
            return Color{static_cast<uint8_t>(peak / 4U), peak, 0};
        if (temperatureC < 50.0f)
            return Color{peak, peak, 0};
        if (temperatureC < 60.0f)
            return Color{peak, static_cast<uint8_t>(peak / 2U), 0};
        return temperatureRed(lowPower);
    }

    inline PatternProfile normalProfile(StatusLedPattern pattern)
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

    inline PatternProfile lowPowerProfile(StatusLedPattern pattern)
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

    inline PatternProfile profile(StatusLedPattern pattern, bool lowPower)
    {
        return lowPower ? lowPowerProfile(pattern) : normalProfile(pattern);
    }
}

#endif
