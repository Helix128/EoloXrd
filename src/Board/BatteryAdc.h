#ifndef EOLO_BOARD_BATTERY_ADC_H
#define EOLO_BOARD_BATTERY_ADC_H

#include <Arduino.h>
#include <math.h>
#include "../Config/Legacy.h"
#include <Eolo/Core/Power/BatteryMath.h>

class Battery {
public:
    static constexpr int BATTERY_COUNT = 2;
    static constexpr int ADC_MAX = BatteryMath::AdcMax;
    static constexpr float ADC_VREF = BatteryMath::AdcVref;
    static constexpr float DIVIDER_RATIO = BatteryMath::DividerRatio;
    static constexpr float BATT_MAX_VOLTAGE = BatteryMath::MaxVoltage;

    Battery() {}

    void begin(int batteryPin = BATTERY_ADC_PIN, float ema_alpha = 0.005f) {
        battery_pin = batteryPin;
        alpha = ema_alpha;
        if (battery_pin < 0) {
            emaLevel = 0.0f;
            emaInitialized = false;
            return;
        }
        pinMode(battery_pin, INPUT);
        emaLevel = (float)getLevel();
        emaInitialized = true;
    }

    float getPct() {
        if (battery_pin < 0)
            return -1.0f;
        float sample = (float)getLevel();
        emaLevel = (alpha * sample) + ((1.0f - alpha) * emaLevel);
        return pctFromVoltage(voltageFromLevel(emaLevel));
    }

    int getRawLevel() { return battery_pin < 0 ? -1 : getLevel(); }
    float getVoltage() { return battery_pin < 0 ? -1.0f : voltageFromLevel((float)getLevel()); }

private:
    float emaLevel = 0.0f;
    float alpha = 0.005f;
    bool emaInitialized = false;

    int getLevel() { return analogRead(battery_pin); }

    float voltageFromLevel(float level) {
        return BatteryMath::voltageFromAdcLevel(level, ADC_MAX, ADC_VREF, DIVIDER_RATIO);
    }

    float pctFromVoltage(float v) {
        return BatteryMath::pctFromVoltage(v, BATT_MAX_VOLTAGE);
    }

    int battery_pin = 34;
};

#endif
