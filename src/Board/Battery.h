#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>

class Battery
{
public:

    const int BATTERY_COUNT = 2;
    uint8_t BATTERY_PIN = 34;
    const int ADC_MAX = 4095;
    const float ADC_VREF = 3.3f;
    const float DIVIDER_RATIO = 7.8f;
    const float BATT_MAX_VOLTAGE = 16.8f;

    void begin(uint8_t batteryPin = 34, float ema_alpha = 0.005f) {
        pinMode(batteryPin, INPUT);
        BATTERY_PIN = batteryPin;
        alpha = ema_alpha;
        
        // Pre-calentamos el filtro con una lectura inicial
        emaLevel = (float)getLevel();
        emaInitialized = true;
    }

    float getPct() {
        // 1. Actualizar la EMA (lógica del antiguo addSample())
        float sample = (float)getLevel();
        emaLevel = (alpha * sample) + ((1.0f - alpha) * emaLevel);
        
        // 2. Devolver el % filtrado (lógica del antiguo getPctEMA())
        return pctFromVoltage( voltageFromLevel(emaLevel) );
    }

    float emaLevel = 0.0f;
    float alpha = 0.005f;
    bool emaInitialized = false;

    // Ahora es privada, solo para uso interno
    int getLevel() {
        return analogRead(BATTERY_PIN);
    }

    float voltageFromLevel(float level) {
        float voltage = (level / 4095.0f) * ADC_VREF;
        voltage *= 1.96f / 1.84f;
        voltage *= DIVIDER_RATIO;
        voltage += 0.8f;
        return voltage;
    }

    float pctFromVoltage(float v) {
        float pct = (v / BATT_MAX_VOLTAGE) * 100.0f;
        
        if (pct > 100.0f) pct = 100.0f;
        if (pct < 0.0f) pct = 0.0f;
        
        return pct;
    }
};

#endif