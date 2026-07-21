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
    TaskHandle_t _taskHandle = nullptr;
    SemaphoreHandle_t _dataMutex;
    AnemometerData _data;
    uint32_t _lastSuccessMs = 0;

    static const uint16_t REG_START = 0x0000;
    static const uint8_t REG_COUNT = 2;
    // 1100 ms: el anemómetro Modbus actualiza a ~0.9 Hz; escalonado 300 ms tras AFM07 (800 ms)
    // para que las peticiones RS485 de ambos sensores no colisionen en el bus compartido.
    static const uint32_t READ_INTERVAL_MS = 1100;
    // 5000 ms: backoff tras error Modbus, consistente con AFM07 para simplificar el diagnóstico de bus.
    static const uint32_t ERROR_BACKOFF_MS = 5000;
    // 15000 ms: sin respuesta en 15 s → dato obsoleto; umbral conservador para alertar fallo de sensor.
    static const uint32_t STALE_DATA_MS = 15000;
    static const BaseType_t TASK_CORE = 0;
    // Prio 1: por debajo del driver RS485 (prio 2); bloquea en mutex durante la lectura Modbus.
    // Stack 4096: suficiente para el frame Modbus + buffer local; medir con uxTaskGetStackHighWaterMark en DEBUG.
    static const UBaseType_t TASK_PRIORITY   = 1;
    static const uint32_t TASK_STACK_BYTES   = 4096;

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
                    AnemometerModel::applyReadSuccess(self->_data, self->_lastSuccessMs, rawSpeed, (int)buffer[1], now);
                    if (EoloDebug::verboseLogsEnabled()) {
                        LOG_F("Anemometro: rawSpeed=%d, speed=%.2f m/s, windKph=%.2f, direction=%d\n",
                              rawSpeed, self->_data.speed, self->_data.windKph, self->_data.direction);
                    }
                } else {
                    AnemometerModel::applyReadFailure(self->_data, self->_lastSuccessMs, now, STALE_DATA_MS);
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

    bool begin() {
        if (_taskHandle != nullptr)
            return true;
        if (_dataMutex == nullptr)
            return false;
        RS485Bus::getInstance().begin();
        BaseType_t created = xTaskCreatePinnedToCore(taskWorker, "AnemTask", TASK_STACK_BYTES, this, TASK_PRIORITY, &_taskHandle, TASK_CORE);
        if (created != pdPASS)
        {
            _taskHandle = nullptr;
            return false;
        }
        return true;
    }

    bool getData(AnemometerData& output) {
        bool success = false;
        if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            AnemometerModel::refreshValidity(_data, _lastSuccessMs, millis(), STALE_DATA_MS);
            output = _data;
            success = _data.valid;
            xSemaphoreGive(_dataMutex);
        }
        return success;
    }
};

#endif
