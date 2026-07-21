#ifndef EOLO_CORE_POWER_BATTERY_MATH_H
#define EOLO_CORE_POWER_BATTERY_MATH_H

#include <stddef.h>
#include <stdint.h>

class BatteryMath
{
public:
    static constexpr int AdcMax = 4095;
    static constexpr float AdcVref = 3.3f;
    static constexpr float DividerRatio = 7.8f;
    static constexpr float MaxVoltage = 16.8f;

    static float voltageFromAdcLevel(float level,
                                     int adcMax = AdcMax,
                                     float adcVref = AdcVref,
                                     float dividerRatio = DividerRatio)
    {
        float voltage = (level / (float)adcMax) * adcVref;
        voltage *= 1.96f / 1.84f;
        voltage *= dividerRatio;
        voltage += 0.8f;
        return voltage;
    }

    static float pctFromVoltage(float voltage, float maxVoltage = MaxVoltage)
    {
        float pct = (voltage / maxVoltage) * 100.0f;
        if (pct > 100.0f) pct = 100.0f;
        if (pct < 0.0f) pct = 0.0f;
        return pct;
    }

    static float mvToVolts(int16_t mv)
    {
        return (float)mv / 1000.0f;
    }

    static uint8_t crc8(const uint8_t *data, size_t len)
    {
        uint8_t crc = 0;
        for (size_t i = 0; i < len; ++i)
        {
            crc ^= data[i];
            for (uint8_t bit = 0; bit < 8; ++bit)
                crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
        return crc;
    }
};

#endif
