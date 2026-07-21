#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "../Config/Legacy.h"
#include <Eolo/Core/Power/BatteryMath.h>
#include <Eolo/Core/Power/BatteryProtocol.h>

#ifdef FEATURE_DUAL_BATTERY
#include "I2CBus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

#include "Profiler.h"

class Battery {
public:
    static constexpr int BATTERY_COUNT = 2;
    static constexpr int ADC_MAX = BatteryMath::AdcMax;
    static constexpr float ADC_VREF = BatteryMath::AdcVref;
    static constexpr float DIVIDER_RATIO = BatteryMath::DividerRatio;
    static constexpr float BATT_MAX_VOLTAGE = BatteryMath::MaxVoltage;

#ifdef FEATURE_DUAL_BATTERY
    static constexpr uint8_t DEFAULT_I2C_ADDR = 0x0A;

    using BatteryFrame = BatteryProtocolPacket;
#endif

    Battery()
#ifdef FEATURE_DUAL_BATTERY
        : _dataMutex(xSemaphoreCreateRecursiveMutex())
#endif
    {}

    void begin(int batteryPin = BATTERY_ADC_PIN, float ema_alpha = 0.005f) {
#ifndef FEATURE_DUAL_BATTERY
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
#else
        (void)batteryPin;
        alpha = ema_alpha;
        if (lockData()) {
            emaLevel = 0.0f;
            emaInitialized = false;
            battVoltage[0] = battVoltage[1] = 0.0f;
            dcVoltage = 0.0f;
            activeMosfet = 0;
            valid = false;
            lastI2CReadMs = 0;
            unlockData();
        }
#endif
    }

#ifndef FEATURE_DUAL_BATTERY
    float getPct() {
        if (battery_pin < 0)
            return -1.0f;
        float sample = (float)getLevel();
        emaLevel = (alpha * sample) + ((1.0f - alpha) * emaLevel);
        return pctFromVoltage(voltageFromLevel(emaLevel));
    }

    int getRawLevel() { return battery_pin < 0 ? -1 : getLevel(); }
    float getVoltage() { return battery_pin < 0 ? -1.0f : voltageFromLevel((float)getLevel()); }
#else
    bool pollFromI2C(uint8_t addr = DEFAULT_I2C_ADDR) {
        PROFILE_SCOPE("battery.i2c");

        uint8_t raw[BatteryProtocol::FrameSize] = {};
        I2CBus::Result result = I2CBus::getInstance().readBytesResult(
            addr, raw, sizeof(raw), false);
        if (result != I2CBus::Result::Ok)
            return false;

        BatteryFrame frame{};
        if (!BatteryProtocol::decode(raw, sizeof(raw), frame)) {
            markInvalidFrame();
            return false;
        }

        if (!lockData())
            return false;
        activeMosfet = frame.activeMosfet;
        dcVoltage = frame.dcVoltage;
        battVoltage[0] = frame.batt0Voltage;
        battVoltage[1] = frame.batt1Voltage;
        lastI2CReadMs = millis();
        valid = true;
        consecutiveInvalidFrames = 0;
        unlockData();
        return true;
    }

    bool hasValidData() const {
        if (!lockData())
            return false;
        bool result = valid;
        unlockData();
        return result;
    }

    bool isStale(uint32_t staleMs = 5000UL) const {
        if (!lockData())
            return true;
        bool result = !valid || (millis() - lastI2CReadMs > staleMs);
        unlockData();
        return result;
    }

    uint32_t getLastSuccessMs() const {
        if (!lockData())
            return 0;
        uint32_t result = lastI2CReadMs;
        unlockData();
        return result;
    }

    float getBatteryVoltage(uint8_t idx) const {
        if (idx >= BATTERY_COUNT)
            return -1.0f;
        if (!lockData())
            return -1.0f;
        float result = battVoltage[idx];
        bool hasValue = valid;
        unlockData();
        return hasValue ? result : -1.0f;
    }

    float getDCVoltage() const {
        if (!lockData())
            return -1.0f;
        float result = valid ? dcVoltage : -1.0f;
        unlockData();
        return result;
    }

    uint8_t getActiveMosfet() const {
        if (!lockData())
            return 0;
        uint8_t result = activeMosfet;
        unlockData();
        return result;
    }

    int getRawLevel() const {
        if (!lockData())
            return -1;
        if (!valid) {
            unlockData();
            return -1;
        }
        if (activeMosfet < 1 || activeMosfet > 3) {
            unlockData();
            return -1;
        }
        float voltage = activeVoltageUnlocked();
        float level = activeMosfet == 1
                          ? voltage * (ADC_MAX / ADC_VREF) * DIVIDER_RATIO
                          : ((voltage - 0.8f) / DIVIDER_RATIO) * (ADC_MAX / ADC_VREF);
        int result = (int)level;
        if (result < 0) result = 0;
        if (result > ADC_MAX) result = ADC_MAX;
        unlockData();
        return result;
    }

    float getVoltage() const {
        if (!lockData())
            return -1.0f;
        float result = valid ? activeVoltageUnlocked() : -1.0f;
        unlockData();
        return result;
    }

    bool isPoweredByDC() const {
        if (!lockData())
            return false;
        bool result = valid && activeMosfet == 1;
        unlockData();
        return result;
    }

    float getPct(uint8_t idx = 0xFF) const {
        if (!lockData())
            return -1.0f;
        if (!valid) {
            unlockData();
            return -1.0f;
        }
        float voltage = idx == 0xFF
                            ? activeVoltageUnlocked()
                            : (idx < BATTERY_COUNT ? battVoltage[idx] : -1.0f);
        float pct = voltage < 0.0f ? -1.0f : (voltage / BATT_MAX_VOLTAGE) * 100.0f;
        if (pct > 100.0f) pct = 100.0f;
        if (pct < 0.0f && voltage >= 0.0f) pct = 0.0f;
        unlockData();
        return pct;
    }

    uint8_t getConsecutiveInvalidFrames() const {
        if (!lockData())
            return 0;
        uint8_t result = consecutiveInvalidFrames;
        unlockData();
        return result;
    }
#endif

private:
#ifndef FEATURE_DUAL_BATTERY
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
#else
    mutable SemaphoreHandle_t _dataMutex = nullptr;
    float battVoltage[BATTERY_COUNT] = {0.0f, 0.0f};
    float dcVoltage = 0.0f;
    uint8_t activeMosfet = 0;
    bool valid = false;
    uint8_t consecutiveInvalidFrames = 0;
    uint32_t lastI2CReadMs = 0;

    float emaLevel = 0.0f;
    float alpha = 0.005f;
    bool emaInitialized = false;

    bool lockData() const {
        if (!_dataMutex)
            return true;
        return xSemaphoreTakeRecursive(_dataMutex, pdMS_TO_TICKS(5)) == pdTRUE;
    }

    void unlockData() const {
        if (_dataMutex)
            xSemaphoreGiveRecursive(_dataMutex);
    }

    float activeVoltageUnlocked() const {
        switch (activeMosfet) {
        case 1: return dcVoltage;
        case 2: return battVoltage[0];
        case 3: return battVoltage[1];
        default: return -1.0f;
        }
    }

    void markInvalidFrame() {
        if (!lockData())
            return;
        if (consecutiveInvalidFrames < 255)
            ++consecutiveInvalidFrames;
        uint8_t count = consecutiveInvalidFrames;
        unlockData();
        static uint32_t lastLogMs = 0;
        uint32_t now = millis();
        if (count == 1 || now - lastLogMs >= 10000UL) {
            LOG_LN("Battery I2C frame invalido; se conserva el ultimo dato valido");
            lastLogMs = now;
        }
    }
#endif
};

#endif
