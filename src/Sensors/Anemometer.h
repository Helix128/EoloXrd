#ifndef ANEMOMETER_H
#define ANEMOMETER_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "../Board/RS485Bus.h"
#include <Eolo/Core/Sensors/AnemometerModel.h>
#include <Eolo/Types/AnemometerData.h>

#define ANEM_ID 1

class Anemometer {
private:
    SemaphoreHandle_t _dataMutex;
    AnemometerData _data;
    uint32_t _lastSuccessMs = 0;
    static constexpr uint16_t REG_START = 0x0000;
    static constexpr uint8_t REG_COUNT = 2;
    static constexpr uint32_t FRESH_DATA_MS = 1600;
    static constexpr uint32_t STALE_DATA_MS = 15000;

    static void onRead(void* context, bool success, const uint16_t* registers, uint8_t count, uint8_t) {
        Anemometer* self = static_cast<Anemometer*>(context);
        const uint32_t now = millis();
        if (!self || count != REG_COUNT || xSemaphoreTake(self->_dataMutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
        if (success && registers)
            AnemometerModel::applyReadSuccess(self->_data, self->_lastSuccessMs, (int)registers[0], (int)registers[1], now);
        else
            AnemometerModel::applyReadFailure(self->_data, self->_lastSuccessMs, now, STALE_DATA_MS);
        AnemometerModel::refreshAge(self->_data, self->_lastSuccessMs, now, FRESH_DATA_MS, STALE_DATA_MS);
        xSemaphoreGive(self->_dataMutex);
    }

public:
    Anemometer() : _dataMutex(xSemaphoreCreateMutex()) {}
    ~Anemometer() { if (_dataMutex) vSemaphoreDelete(_dataMutex); }

    bool begin() {
        if (!_dataMutex) return false;
        return RS485Bus::getInstance().registerEndpoint(ANEM_ID, REG_START, REG_COUNT, onRead, this);
    }
    bool getData(AnemometerData& output) {
        if (!_dataMutex || xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
        AnemometerModel::refreshAge(_data, _lastSuccessMs, millis(), FRESH_DATA_MS, STALE_DATA_MS);
        output = _data;
        const bool valid = _data.valid;
        xSemaphoreGive(_dataMutex);
        return valid;
    }
};

#endif
