#ifndef ANEMOMETER_H
#define ANEMOMETER_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "../Board/RS485Bus.h"
#include <Eolo/Types/AnemometerData.h>

#define ANEM_ID 1

class Anemometer {
private:
    TaskHandle_t _taskHandle = nullptr;
    SemaphoreHandle_t _dataMutex;
    AnemometerData _data;
    unsigned long _lastSuccessMs = 0;

    static const uint16_t REG_START = 0x0000;
    static const uint8_t REG_COUNT = 2;
    static const uint32_t READ_INTERVAL_MS = 1100;
    static const uint32_t ERROR_BACKOFF_MS = 5000;
    static const uint32_t STALE_DATA_MS = 15000;
    static const BaseType_t TASK_CORE = 0;

    static void taskWorker(void* arg) {
        Anemometer* self = (Anemometer*)arg;
        
        uint16_t buffer[2];

        vTaskDelay(pdMS_TO_TICKS(100)); // Diferencia con el AFM07 al arrancar

        while (true) {
            // Solicitar lectura del bus centralizado
            bool success = RS485Bus::getInstance().readRegisters(ANEM_ID, REG_START, REG_COUNT, buffer);
            unsigned long now = millis();

            if (xSemaphoreTake(self->_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (success) {
                    int rawSpeed = (int)buffer[0];
                    self->_data.speed = rawSpeed / 100.0f;
                    self->_data.windKph = self->_data.speed * 3.6f;
                    self->_data.direction = (int)buffer[1];
                    self->_data.valid = true;
                    self->_lastSuccessMs = now;
                    if (EoloDebug::verboseLogsEnabled()) {
                        LOG_F("Anemometro: rawSpeed=%d, speed=%.2f m/s, windKph=%.2f, direction=%d\n",
                              rawSpeed, self->_data.speed, self->_data.windKph, self->_data.direction);
                    }
                } else {
                    self->_data.valid = self->_lastSuccessMs > 0 && (now - self->_lastSuccessMs) <= STALE_DATA_MS;
                }
                xSemaphoreGive(self->_dataMutex);
            } else {
                static unsigned long lastTimeoutLogMs = 0;
                if (now - lastTimeoutLogMs > 10000) {
                    LOG_LN("[Anemometro] Semaphore timeout - dato anterior puede estar desactualizado");
                    lastTimeoutLogMs = now;
                }
            }
            
            vTaskDelay(pdMS_TO_TICKS(success ? READ_INTERVAL_MS : ERROR_BACKOFF_MS));
        }
    }

public:
    Anemometer() {
        _dataMutex = xSemaphoreCreateMutex();
        _data.valid = false;
        _data.speed = 0.0f;
        _data.windKph = 0.0f;
        _data.direction = 0;
        _lastSuccessMs = 0;
    }

    ~Anemometer() {
        if (_taskHandle) vTaskDelete(_taskHandle);
        if (_dataMutex) vSemaphoreDelete(_dataMutex);
    }

    void begin() {
        RS485Bus::getInstance().begin();
        xTaskCreatePinnedToCore(taskWorker, "AnemTask", 4096, this, 1, &_taskHandle, TASK_CORE);
    }

    bool getData(AnemometerData& output) {
        bool success = false;
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
