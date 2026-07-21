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
    TaskHandle_t _taskHandle = nullptr;
    SemaphoreHandle_t _dataMutex;
    FlowData _data;
    uint32_t _lastSuccessMs = 0;
    static const uint16_t REG_INSTANT_FLOW = 0x0000;
    static constexpr float FLOW_DIVISOR = AFM07_FLOW_DIVISOR;
    // 800 ms: el AFM07 actualiza el registro de caudal a ~1 Hz; sondear a 800 ms deja margen sin saturar el bus RS485.
    // Diferente a Anemometer (1100 ms) para escalonar las peticiones Modbus y evitar colisiones en el bus compartido.
    static const uint32_t READ_INTERVAL_MS = 800;
    // 5000 ms: backoff tras error Modbus; evita spam de retries ante fallo prolongado del sensor.
    static const uint32_t ERROR_BACKOFF_MS = 5000;
    // 1200 ms: dato considerado fresco si llegó en el último ciclo de lectura (800 ms + margen de jitter).
    static const uint32_t FRESH_DATA_MS = 1200;
    // 15000 ms: sin respuesta en 15 s → dato obsoleto; umbral conservador para alertar fallo de sensor.
    static const uint32_t STALE_DATA_MS = 15000;
    static const BaseType_t TASK_CORE = 0;
    // Prio 1: por debajo del driver RS485 (prio 2); bloquea en mutex durante la lectura Modbus.
    // Stack 4096: suficiente para el frame Modbus + buffer local; medir con uxTaskGetStackHighWaterMark en DEBUG.
    static const UBaseType_t TASK_PRIORITY   = 1;
    static const uint32_t TASK_STACK_BYTES   = 4096;

    static void taskWorker(void* arg) {
        AFM07* self = (AFM07*)arg;
        
        uint16_t rawData[1];

        vTaskDelay(pdMS_TO_TICKS(500)); // Delay inicial para evitar colisión con Anemómetro al arrancar
        while (true) {
            // Solicitar lectura del bus centralizado
            bool success = RS485Bus::getInstance().readRegisters(AFM_ID, REG_INSTANT_FLOW, 1, rawData);
            unsigned long now = millis();

            if (xSemaphoreTake(self->_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (success) {
                    AFM07Model::applyReadSuccess(self->_data, self->_lastSuccessMs, rawData[0], now, FLOW_DIVISOR);
                    if (EoloDebug::verboseLogsEnabled()) {
                        LOG_F("AFM07: raw=%d, flow=%.2f L/min \n", rawData[0], self->_data.flow);
                    }
                } else {
                    AFM07Model::applyReadFailure(self->_data, self->_lastSuccessMs, now, FRESH_DATA_MS, STALE_DATA_MS);
                }
                xSemaphoreGive(self->_dataMutex);
            } else {
                static unsigned long lastTimeoutLogMs = 0;
                if (now - lastTimeoutLogMs > 10000) {
                    LOG_LN("[AFM07] Semaphore timeout - dato anterior puede estar desactualizado");
                    lastTimeoutLogMs = now;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(success ? READ_INTERVAL_MS : ERROR_BACKOFF_MS));
        }
    }

public:
    AFM07() {
        _dataMutex = xSemaphoreCreateMutex();
        _data.valid = false;
        _data.fresh = false;
        _data.stale = false;
        _data.ageMs = 0;
        _data.flow = 0.0;
        _data.velocity = 0.0;
        _lastSuccessMs = 0;
    }

    ~AFM07() {
        if (_taskHandle) vTaskDelete(_taskHandle);
        if (_dataMutex) vSemaphoreDelete(_dataMutex);
    }

    bool begin() {
        if (_taskHandle != nullptr)
            return true;
        if (_dataMutex == nullptr) {
            LOG_LN("AFM07 sin mutex");
            return false;
        }
        RS485Bus::getInstance().begin();
        BaseType_t ok = xTaskCreatePinnedToCore(taskWorker, "AFM07Task", TASK_STACK_BYTES, this, TASK_PRIORITY, &_taskHandle, TASK_CORE);
        if (ok != pdPASS) {
            _taskHandle = nullptr;
            LOG_LN("No se pudo crear AFM07Task");
            return false;
        }
        return true;
    }

    bool getData(FlowData& output) {
        bool success = false;
        if (_dataMutex == nullptr) return false;
        if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            uint32_t now = millis();
            AFM07Model::refreshAge(_data, _lastSuccessMs, now, FRESH_DATA_MS, STALE_DATA_MS);
            output = _data;
            success = _data.valid;
            xSemaphoreGive(_dataMutex);
        }
        return success;
    }
};

#endif
