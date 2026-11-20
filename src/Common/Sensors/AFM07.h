#ifndef AFM07_HPP
#define AFM07_HPP

#include <Arduino.h>
#include "../Board/RS485Bus.h"
#include "../Data/PreferencesSettingsStore.h"
#include <Eolo/Core/Sensors/AFM07Model.h>
#include <Eolo/Types/FlowData.h>

#define AFM_ID EoloCore::ModbusRtuProtocol::Afm07SlaveId

class AFM07 {
private:
    portMUX_TYPE _dataMux = portMUX_INITIALIZER_UNLOCKED;
    FlowData _data;
    uint32_t _lastSuccessMs = 0;
    static constexpr uint16_t REG_INSTANT_FLOW = EoloCore::ModbusRtuProtocol::Afm07FlowRegister;
    static constexpr const char *SETTINGS_NAMESPACE = "eolo_afm";
    float _divisor = EoloConfig::afm07FlowDivisor;
    uint16_t _lastRaw = 0;
    // El contrato Dron considera válida una muestra durante la ventana de
    // 1,5 s usada por la calificación de cadencia RS485.
    static constexpr uint32_t FRESH_DATA_MS = 1500;
    static constexpr uint32_t STALE_DATA_MS = 15000;

    static void onRead(void* context, bool success, const uint16_t* registers, uint8_t count, uint8_t) {
        AFM07* self = static_cast<AFM07*>(context);
        const uint32_t now = millis();
        if (!self || count != 1) return;
        portENTER_CRITICAL(&self->_dataMux);
        if (success && registers) {
            self->_lastRaw = registers[0];
            AFM07Model::applyReadSuccess(self->_data, self->_lastSuccessMs, registers[0], now, self->_divisor);
        }
        else
            AFM07Model::applyReadFailure(self->_data, self->_lastSuccessMs, now, FRESH_DATA_MS, STALE_DATA_MS);
        portEXIT_CRITICAL(&self->_dataMux);
    }

public:
    bool begin() {
        {
            PreferencesSettingsStore store(SETTINGS_NAMESPACE);
            _divisor = AFM07Model::loadDivisor(store, EoloConfig::afm07FlowDivisor);
        }
        return RS485Bus::getInstance().registerEndpoint(AFM_ID, REG_INSTANT_FLOW, 1, onRead, this);
    }
    float divisor() {
        portENTER_CRITICAL(&_dataMux);
        const float d = _divisor;
        portEXIT_CRITICAL(&_dataMux);
        return d;
    }
    // Último registro crudo leído; 0 si aún no hay muestra.
    uint16_t lastRaw() {
        portENTER_CRITICAL(&_dataMux);
        const uint16_t raw = _data.sampleId != 0 ? _lastRaw : 0;
        portEXIT_CRITICAL(&_dataMux);
        return raw;
    }
    // Aplica y persiste el divisor. Las lecturas siguientes lo usan de inmediato.
    bool setDivisor(float divisor) {
        if (!AFM07Model::isSupportedDivisor(divisor)) return false;
        portENTER_CRITICAL(&_dataMux);
        _divisor = divisor;
        portEXIT_CRITICAL(&_dataMux);
        PreferencesSettingsStore store(SETTINGS_NAMESPACE);
        return AFM07Model::saveDivisor(store, divisor);
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
