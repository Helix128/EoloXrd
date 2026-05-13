#ifndef CLOCK_SETTINGS_H
#define CLOCK_SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>

class ClockSettings
{
public:
    enum DstMode : uint8_t
    {
        Winter = 0,
        Summer = 1
    };

    static constexpr const char *Namespace = "eolo_clock";
    static constexpr const char *DstModeKey = "dstMode";

    DstMode getDstMode() const
    {
        Preferences preferences;
        preferences.begin(Namespace, false);
        uint8_t mode = preferences.getUChar(DstModeKey, Winter);
        preferences.end();
        return mode == Summer ? Summer : Winter;
    }

    void setDstMode(DstMode mode)
    {
        Preferences preferences;
        preferences.begin(Namespace, false);
        preferences.putUChar(DstModeKey, mode);
        preferences.end();
    }

    int utcOffsetSeconds() const
    {
        return getDstMode() == Summer ? -3 * 3600 : -4 * 3600;
    }

    const char *label() const
    {
        return getDstMode() == Summer ? "Verano UTC-3" : "Invierno UTC-4";
    }
};

#endif
