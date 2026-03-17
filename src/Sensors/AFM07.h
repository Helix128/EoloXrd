#ifndef AFM07_HPP
#define AFM07_HPP

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "./Board/RS485.h" 

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
    static const uint16_t REG_INSTANT_FLOW = 0x0000;
    static const uint8_t FACTOR_LECTURA = 10;

    static void taskWorker(void* arg) {
        AFM07* self = (AFM07*)arg;
        
        TickType_t xLastWakeTime = xTaskGetTickCount();
        const TickType_t xFrequency = pdMS_TO_TICKS(1000); 

        uint16_t rawData[1];

        vTaskDelay(500 / portTICK_PERIOD_MS);
        while (true) {
            bool success = RS485::getInstance().readRegisters(AFM_ID, REG_INSTANT_FLOW, 1, rawData);

            if (!success) {
                Serial.println("DEBUG AFM: RS485 falló internamente (Timeout o CRC)");
            } else {
                Serial.println("DEBUG AFM: Lectura Exitosa, actualizando datos...");
            }
            if (xSemaphoreTake(self->_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (success) {
                    float val = (float)rawData[0] / FACTOR_LECTURA;
                    self->_data.flow = val;
                    self->_data.velocity = val; 
                    self->_data.valid = true;
                    LOG_F("AFM07: Flujo %.2f L/min, Velocidad %.2f m/s\n", self->_data.flow, self->_data.velocity);
                } else {
                    self->_data.valid = false;
                }
                xSemaphoreGive(self->_dataMutex);
            }
            vTaskDelayUntil(&xLastWakeTime, xFrequency);
        }
    }

public:
    AFM07() {
        _dataMutex = xSemaphoreCreateMutex();
        _data.valid = false;
        _data.flow = 0.0;
        _data.velocity = 0.0;
    }

    ~AFM07() {
        if (_taskHandle) vTaskDelete(_taskHandle);
        if (_dataMutex) vSemaphoreDelete(_dataMutex);
    }

    void begin() {
        RS485::getInstance().begin();
        //xTaskCreatePinnedToCore(taskWorker, "AFM07Task", 4096, this, 1, &_taskHandle, 1);
    }

    bool getData(FlowData& output) {
        bool success = false;
        if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            output = _data;
            success = _data.valid;
            xSemaphoreGive(_dataMutex);
        }
        return success;
    }
};

#endif