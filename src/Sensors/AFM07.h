#ifndef AFM07_HPP
#define AFM07_HPP

#include <Arduino.h>
#include "../Board/RS485Bus.h"
#include <Eolo/Core/Sensors/AFM07Model.h>
#include <Eolo/Types/FlowData.h>

#define AFM_ID 2

class AFM07 {
private:
    portMUX_TYPE _dataMux = portMUX_INITIALIZER_UNLOCKED;
    FlowData _data;
    uint32_t _lastSuccessMs = 0;
    static constexpr uint16_t REG_INSTANT_FLOW = 0x0000;
    static constexpr float FLOW_DIVISOR = AFM07_FLOW_DIVISOR;
    static constexpr uint32_t FRESH_DATA_MS = 1200;
    static constexpr uint32_t STALE_DATA_MS = 15000;

    static void onRead(void* context, bool success, const uint16_t* registers, uint8_t count, uint8_t) {
        AFM07* self = static_cast<AFM07*>(context);
        const uint32_t now = millis();
        if (!self || count != 1) return;
        portENTER_CRITICAL(&self->_dataMux);
        if (success && registers)
            AFM07Model::applyReadSuccess(self->_data, self->_lastSuccessMs, registers[0], now, FLOW_DIVISOR);
        else
            AFM07Model::applyReadFailure(self->_data, self->_lastSuccessMs, now, FRESH_DATA_MS, STALE_DATA_MS);
        portEXIT_CRITICAL(&self->_dataMux);
    }

public:
    bool begin() {
        return RS485Bus::getInstance().registerEndpoint(AFM_ID, REG_INSTANT_FLOW, 1, onRead, this);
    }
    bool getData(FlowData& output) {
        portENTER_CRITICAL(&_dataMux);
        AFM07Model::refreshAge(_data, _lastSuccessMs, millis(), FRESH_DATA_MS, STALE_DATA_MS);
        output = _data;
        const bool valid = _data.valid;
        portEXIT_CRITICAL(&_dataMux);
        return valid;
    }
};

#endif
