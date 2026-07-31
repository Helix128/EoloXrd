#ifndef AFM07_HPP
#define AFM07_HPP

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "../Board/RS485Bus.h"
#include <Eolo/Core/Sensors/AFM07Model.h>
#include <Eolo/Types/FlowData.h>

#define AFM_ID 2

class AFM07 {
private:
    SemaphoreHandle_t _dataMutex;
    FlowData _data;
    uint32_t _lastSuccessMs = 0;
    static constexpr uint16_t REG_INSTANT_FLOW = 0x0000;
    static constexpr float FLOW_DIVISOR = AFM07_FLOW_DIVISOR;
    static constexpr uint32_t FRESH_DATA_MS = 1200;
    static constexpr uint32_t STALE_DATA_MS = 15000;

    static void onRead(void* context, bool success, const uint16_t* registers, uint8_t count, uint8_t) {
        AFM07* self = static_cast<AFM07*>(context);
        const uint32_t now = millis();
        if (!self || count != 1 || xSemaphoreTake(self->_dataMutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
        if (success && registers)
            AFM07Model::applyReadSuccess(self->_data, self->_lastSuccessMs, registers[0], now, FLOW_DIVISOR);
        else
            AFM07Model::applyReadFailure(self->_data, self->_lastSuccessMs, now, FRESH_DATA_MS, STALE_DATA_MS);
        xSemaphoreGive(self->_dataMutex);
    }

public:
    AFM07() : _dataMutex(xSemaphoreCreateMutex()) {}
    ~AFM07() { if (_dataMutex) vSemaphoreDelete(_dataMutex); }

    bool begin() {
        if (!_dataMutex) return false;
        return RS485Bus::getInstance().registerEndpoint(AFM_ID, REG_INSTANT_FLOW, 1, onRead, this);
    }
    bool getData(FlowData& output) {
        if (!_dataMutex || xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
        AFM07Model::refreshAge(_data, _lastSuccessMs, millis(), FRESH_DATA_MS, STALE_DATA_MS);
        output = _data;
        const bool valid = _data.valid;
        xSemaphoreGive(_dataMutex);
        return valid;
    }
};

#endif
