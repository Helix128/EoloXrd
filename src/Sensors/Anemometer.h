#ifndef ANEMOMETER_H
#define ANEMOMETER_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "./Board/RS485.h"

#define ANEM_ID 1

struct AnemometerData {
    float speed;
    float windKph;
    int direction;
    bool valid;
};

class Anemometer {
private:
    TaskHandle_t _taskHandle = nullptr;
    SemaphoreHandle_t _dataMutex;
    AnemometerData _data;

    static const uint16_t REG_START = 0x0000;
    static const uint8_t REG_COUNT = 2;

    static void taskWorker(void* arg) {
        Anemometer* self = (Anemometer*)arg;
        
        TickType_t xLastWakeTime = xTaskGetTickCount();
        const TickType_t xFrequency = pdMS_TO_TICKS(600);

        uint16_t buffer[2];

        while (true) {
            bool success = RS485::getInstance().readRegisters(ANEM_ID, REG_START, REG_COUNT, buffer);
            //LOG_F("Anemómetro: Lectura RS485 %s\n", success ? "exitosa" : "fallida");
            if (xSemaphoreTake(self->_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (success) {
                    int rawSpeed = (int)buffer[0];
                    self->_data.speed = rawSpeed / 100.0f;
                    self->_data.windKph = self->_data.speed * 3.6f;
                    self->_data.direction = (int)buffer[1];
                    self->_data.valid = true;
                    //LOG_F("Anemómetro: Velocidad %.2f m/s, Dirección %d°\n", self->_data.speed, self->_data.direction);
                } else {
                    self->_data.valid = false;
                }
                xSemaphoreGive(self->_dataMutex);
            }
            
            vTaskDelayUntil(&xLastWakeTime, xFrequency);
        }
    }

public:
    Anemometer() {
        _dataMutex = xSemaphoreCreateMutex();
        _data.valid = false;
        _data.speed = 0.0f;
        _data.windKph = 0.0f;
        _data.direction = 0;
    }

    ~Anemometer() {
        if (_taskHandle) vTaskDelete(_taskHandle);
        if (_dataMutex) vSemaphoreDelete(_dataMutex);
    }

    void begin() {
        RS485::getInstance().begin();
        xTaskCreatePinnedToCore(taskWorker, "AnemTask", 4096, this, 1, &_taskHandle, 1);
    }

    bool getData(AnemometerData& output) {
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