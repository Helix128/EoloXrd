#ifndef PLANTOWER_H
#define PLANTOWER_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "../Config/Legacy.h"
#include <Eolo/Core/Sensors/PlantowerParser.h>
#include <Eolo/Types/PlantowerData.h>

class Plantower {
private:
    HardwareSerial* _serial = nullptr;
    int _rxPin = EOLO_PIN_UNUSED;
    int _txPin = EOLO_PIN_UNUSED;
    TaskHandle_t _taskHandle = nullptr;
    SemaphoreHandle_t _mutex = nullptr;
    PlantowerData _data = {};
    PlantowerParser _parser;

    static void taskWorker(void* arg) {
        Plantower* self = (Plantower*)arg;

        // Inicialización dinámica del bus inyectado
        self->_serial->begin(9600, SERIAL_8N1, self->_rxPin, self->_txPin);

        while (true) {
            if (self->_serial->available()) {
                uint8_t ch = self->_serial->read();
                if (self->_parser.processByte(ch)) {
                    if (xSemaphoreTake(self->_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        self->_data = self->_parser.getData();
                        xSemaphoreGive(self->_mutex);
                    }
                }
            } else {
                // Si la cola UART está vacía, cedemos el paso de forma limpia
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
    }

public:
    Plantower() {
        _mutex = xSemaphoreCreateMutex();
        _data.valid = false;
    }

    ~Plantower() {
        if (_taskHandle) vTaskDelete(_taskHandle);
        if (_mutex) vSemaphoreDelete(_mutex);
    }

    // Firma flexible con valores por defecto para permitir desacoplamiento y testing de hardware
    bool begin(HardwareSerial& serialInterface = Serial2, int rx = PT_RX, int tx = PT_TX) {
        if (_taskHandle != nullptr)
            return true;
        if (_mutex == nullptr || rx < 0 || tx < 0)
            return false;
        _serial = &serialInterface;
        _rxPin = rx;
        _txPin = tx;

        static const UBaseType_t kTaskPriority = 1;
        static const uint32_t kTaskStack = 2048;

        // Se ejecuta en el Core 1 para no interferir con las tareas de comunicaciones críticas (como RS485 en Core 0)
        BaseType_t created = xTaskCreatePinnedToCore(taskWorker, "PlantowerTask", kTaskStack, this, kTaskPriority, &_taskHandle, 1);
        if (created != pdPASS)
        {
            _taskHandle = nullptr;
            return false;
        }
        return true;
    }

    bool getData(PlantowerData& output) {
        bool success = false;
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            output = _data;
            success = _data.valid;
            xSemaphoreGive(_mutex);
        } else {
            if (EoloDebug::verboseLogsEnabled()) {
                LOG_F("Error: Tiempo de espera agotado al intentar tomar el mutex del Plantower\n");
            }
        }
        return success;
    }
};

#endif
