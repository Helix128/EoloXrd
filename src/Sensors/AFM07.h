#ifndef AFM07_HPP
#define AFM07_HPP

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "./Board/RS485Bus.h"

#define AFM_ID 2

struct FlowData {
    float flow;
    float velocity;
    bool valid;
};

class AFM07 {
private:
    TaskHandle_t _taskHandle = nullptr;
    SemaphoreHandle_t _dataMutex;
    FlowData _data;
    unsigned long _lastSuccessMs = 0;
    static const uint16_t REG_INSTANT_FLOW = 0x0000;
    static const uint8_t FACTOR_LECTURA = 10;
    static const uint32_t READ_INTERVAL_MS = 800;
    static const uint32_t ERROR_BACKOFF_MS = 5000;
    static const uint32_t STALE_DATA_MS = 15000;
    static const BaseType_t TASK_CORE = 0;

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
                    float val = (float)rawData[0] / FACTOR_LECTURA;
                    self->_data.flow = val;
                    self->_data.velocity = val;
                    self->_data.valid = true;
                    self->_lastSuccessMs = now;
#if PROFILE_VERBOSE
                    LOG_F("AFM07: raw=%d, flow=%.2f L/min \n", rawData[0], self->_data.flow);
#endif
                } else {
                    self->_data.valid = self->_lastSuccessMs > 0 && (now - self->_lastSuccessMs) <= STALE_DATA_MS;
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
        _data.flow = 0.0;
        _data.velocity = 0.0;
        _lastSuccessMs = 0;
    }

    ~AFM07() {
        if (_taskHandle) vTaskDelete(_taskHandle);
        if (_dataMutex) vSemaphoreDelete(_dataMutex);
    }

    bool begin() {
        if (_dataMutex == nullptr) {
            LOG_LN("AFM07 sin mutex");
            return false;
        }
        RS485Bus::getInstance().begin();
        BaseType_t ok = xTaskCreatePinnedToCore(taskWorker, "AFM07Task", 4096, this, 1, &_taskHandle, TASK_CORE);
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
            if (_lastSuccessMs == 0 || (millis() - _lastSuccessMs) > STALE_DATA_MS) {
                _data.valid = false;
            }
            output = _data;
            success = _data.valid;
            xSemaphoreGive(_dataMutex);
        }
        return success;
    }
};

#endif
