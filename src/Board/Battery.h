#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include <string.h>

#ifdef FEATURE_DUAL_BATTERY
#include "I2CBus.h"
#endif

#include "Profiler.h" 

class Battery {
public:
    static constexpr int BATTERY_COUNT = 2;
    static constexpr int ADC_MAX = 4095;
    static constexpr float ADC_VREF = 3.3f;
    static constexpr float DIVIDER_RATIO = 7.8f;
    static constexpr float BATT_MAX_VOLTAGE = 16.8f;

#ifdef FEATURE_DUAL_BATTERY
    static constexpr uint8_t DEFAULT_I2C_ADDR = 10;
#endif

    void begin(uint8_t batteryPin = 34, float ema_alpha = 0.005f) {
#ifndef FEATURE_DUAL_BATTERY
        pinMode(batteryPin, INPUT);
        battery_pin = batteryPin;
        alpha = ema_alpha;
        emaLevel = (float)getLevel();
        emaInitialized = true;
#else
        alpha = ema_alpha;
        emaLevel = 0.0f;
        emaInitialized = false;
        battVoltage[0] = battVoltage[1] = 0.0f;
        dcVoltage = 0.0f;
        activeMosfet = 0;
#endif
    }

#ifndef FEATURE_DUAL_BATTERY
    float getPct() {
        float sample = (float)getLevel();
        emaLevel = (alpha * sample) + ((1.0f - alpha) * emaLevel);
        return pctFromVoltage(voltageFromLevel(emaLevel));
    }

    int getRawLevel() { return getLevel(); }

    float getVoltage() { return voltageFromLevel((float)getLevel()); }
#else
    bool pollFromI2C(uint8_t addr = DEFAULT_I2C_ADDR) {
        PROFILE_SCOPE("battery.i2c");
        
        uint8_t buffer[sizeof(uint8_t) + 3 * sizeof(float)];
        const size_t toRead = sizeof(buffer);
        if (!I2CBus::getInstance().readBytes(addr, buffer, toRead)) {
            return false;
        }

        size_t offset = 0;
        memcpy(&activeMosfet, buffer + offset, sizeof(activeMosfet));
        offset += sizeof(activeMosfet);
        memcpy(&dcVoltage, buffer + offset, sizeof(dcVoltage));
        offset += sizeof(dcVoltage);
        memcpy(&battVoltage[0], buffer + offset, sizeof(battVoltage[0]));
        offset += sizeof(battVoltage[0]);
        memcpy(&battVoltage[1], buffer + offset, sizeof(battVoltage[1]));
        lastI2CReadMs = millis();

        //Serial.printf("Battery I2C Read: ActiveMosfet=%d, DC=%.2fV, Batt0=%.2fV, Batt1=%.2fV\n",activeMosfet, dcVoltage, battVoltage[0], battVoltage[1]);
        return true;
    }

    float getBatteryVoltage(uint8_t idx) {
        if (idx >= BATTERY_COUNT) return 0.0f;
        return battVoltage[idx];
    }

    float getDCVoltage() { return dcVoltage; }

    uint8_t getActiveMosfet() { return activeMosfet; }

    int getRawLevel(){
        if(isPoweredByDC()) return dcVoltage * (ADC_MAX / ADC_VREF) * (DIVIDER_RATIO);
        float maxV = max(battVoltage[0], battVoltage[1]);
        float level = ((maxV - 0.8f) / (DIVIDER_RATIO)) * (ADC_MAX / ADC_VREF);
        if (level < 0.0f) level = 0.0f;
        if (level > (float)ADC_MAX) level = (float)ADC_MAX;
        return (int)level;  
    }
    float getVoltage() {
        if (isPoweredByDC()) return dcVoltage;
        return (battVoltage[0] + battVoltage[1]) / 2.0f;
    }

    bool isPoweredByDC() {
        return activeMosfet == 1;
    }

    float getPct(uint8_t idx = 0xFF) {
        float v = (idx == 0xFF) ? getVoltage() : getBatteryVoltage(idx);
        float pct = (v / BATT_MAX_VOLTAGE) * 100.0f;
        if (pct > 100.0f) pct = 100.0f;
        if (pct < 0.0f) pct = 0.0f;
        return pct;
    }
#endif

private:
#ifndef FEATURE_DUAL_BATTERY
    float emaLevel = 0.0f;
    float alpha = 0.005f;
    bool emaInitialized = false;

    int getLevel() { return analogRead(battery_pin); }

    float voltageFromLevel(float level) {
        float voltage = (level / (float)ADC_MAX) * ADC_VREF;
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

    uint8_t battery_pin = 34;
#else
    float battVoltage[BATTERY_COUNT];
    float dcVoltage = 0.0f;
    uint8_t activeMosfet = 0;
    unsigned long lastI2CReadMs = 0;

    float emaLevel = 0.0f;
    float alpha = 0.005f;
    bool emaInitialized = false;

    float pctFromVoltage(float v) {
        float pct = (v / BATT_MAX_VOLTAGE) * 100.0f;
        if (pct > 100.0f) pct = 100.0f;
        if (pct < 0.0f) pct = 0.0f;
        return pct;
    }
#endif
};

#endif
