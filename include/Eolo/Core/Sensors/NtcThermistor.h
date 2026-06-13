#ifndef EOLO_CORE_SENSORS_NTC_THERMISTOR_H
#define EOLO_CORE_SENSORS_NTC_THERMISTOR_H

#include <math.h>

class NtcThermistor
{
public:
    static constexpr float DefaultFixedResistance = 10000.0f;
    static constexpr float DefaultR25 = 10000.0f;
    static constexpr float DefaultBeta = 3950.0f;
    static constexpr float T25Kelvin = 298.15f;
    static constexpr float AdcVref = 3.3f;
    static constexpr int AdcMax = 4095;

    static float rawToVoltage(int raw)
    {
        return ((float)raw * AdcVref) / (float)AdcMax;
    }

    static bool computeTemperature(int raw, float &temperature, float &resistance,
                                   float fixedResistance = DefaultFixedResistance,
                                   float r25 = DefaultR25,
                                   float beta = DefaultBeta)
    {
        if (raw <= 10 || raw >= 4085)
            return false;

        float voltage = rawToVoltage(raw);
        if (voltage <= 0.0f || voltage >= AdcVref)
            return false;

        resistance = fixedResistance * voltage / (AdcVref - voltage);
        if (resistance <= 0.0f)
            return false;

        float tempK = 1.0f / ((1.0f / T25Kelvin) + (logf(resistance / r25) / beta));
        temperature = tempK - 273.15f;
        return isfinite(temperature);
    }

    static bool motorOverheatLatched(bool current,
                                     bool valid,
                                     float temperature,
                                     float highThreshold,
                                     float lowThreshold)
    {
        if (!valid || !isfinite(temperature))
            return current;
        if (temperature >= highThreshold)
            return true;
        if (temperature <= lowThreshold)
            return false;
        return current;
    }
};

#endif // EOLO_CORE_SENSORS_NTC_THERMISTOR_H
