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

    // Pack 18650 2S medido con divisor 220k/100k (Vbat = 3.2 * Vpin). La
    // lectura llega en mV de pin ya calibrados por el ADC del ESP32.
    static constexpr float Pack2sDividerRatio = 3.2f;

    static float pack2sVoltageFromPinMv(float pinMv,
                                        float dividerRatio = Pack2sDividerRatio)
    {
        return (pinMv / 1000.0f) * dividerRatio;
    }

    // Curva de descarga típica de dos celdas LiIon en serie (no lineal: la
    // mayor parte de la carga está en el plateau 7.4-7.8 V). Interpola entre
    // puntos y satura en 0 y 100.
    static float pack2sPctFromVoltage(float voltage)
    {
        struct Point { float voltage; float pct; };
        static constexpr Point curve[] = {
            {8.40f, 100.0f}, {8.30f, 95.0f}, {8.22f, 90.0f}, {8.16f, 85.0f},
            {8.04f, 80.0f},  {7.96f, 75.0f}, {7.90f, 70.0f}, {7.82f, 65.0f},
            {7.74f, 60.0f},  {7.70f, 55.0f}, {7.68f, 50.0f}, {7.64f, 45.0f},
            {7.60f, 40.0f},  {7.58f, 35.0f}, {7.54f, 30.0f}, {7.50f, 25.0f},
            {7.46f, 20.0f},  {7.42f, 15.0f}, {7.38f, 10.0f}, {7.22f, 5.0f},
            {6.54f, 0.0f},
        };
        constexpr size_t count = sizeof(curve) / sizeof(curve[0]);
        if (voltage >= curve[0].voltage)
            return 100.0f;
        if (voltage <= curve[count - 1].voltage)
            return 0.0f;
        for (size_t i = 0; i + 1 < count; ++i)
        {
            if (voltage <= curve[i].voltage && voltage >= curve[i + 1].voltage)
            {
                float ratio = (voltage - curve[i + 1].voltage) /
                              (curve[i].voltage - curve[i + 1].voltage);
                return curve[i + 1].pct + ratio * (curve[i].pct - curve[i + 1].pct);
            }
        }
        return 0.0f;
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
