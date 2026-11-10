#ifndef PLANTOWER_H
#define PLANTOWER_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "../../Variants/Legacy.h"
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
    uint32_t _lastValidMs = 0;
    PlantowerParser _parser;

    static void taskWorker(void* arg) {
        Plantower* self = (Plantower*)arg;

        // Inicialización dinámica del bus inyectado
        self->_serial->begin(9600, SERIAL_8N1, self->_rxPin, self->_txPin);

        const uint32_t startedMs = millis();
        uint32_t lastExpiryCheckMs = startedMs;
        bool receivedFrame = false;
        bool unavailableReported = false;
        while (true) {
            const uint32_t nowMs = millis();
            if (static_cast<uint32_t>(nowMs - lastExpiryCheckMs) >= 1000UL) {
                if (xSemaphoreTake(self->_mutex, 0) == pdTRUE) {
                    self->_data.valid = self->_data.freshAt(nowMs, self->_lastValidMs);
                    const uint32_t sinceMs = receivedFrame ? self->_lastValidMs : startedMs;
                    const bool reportUnavailable = !self->_data.valid && !unavailableReported &&
                        static_cast<uint32_t>(nowMs - sinceMs) > PlantowerData::StaleAfterMs;
                    xSemaphoreGive(self->_mutex);
                    if (reportUnavailable) {
                        LOG_LN(receivedFrame ? "Plantower sin trama valida reciente" :
                                               "Plantower no recibio una primera trama valida");
                        unavailableReported = true;
                    }
                }
                lastExpiryCheckMs = nowMs;
            }
            if (self->_serial->available()) {
                uint8_t ch = self->_serial->read();
                if (self->_parser.processByte(ch)) {
                    if (xSemaphoreTake(self->_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        self->_data = self->_parser.getData();
                        self->_lastValidMs = millis();
                        xSemaphoreGive(self->_mutex);
                        if (unavailableReported)
                            LOG_LN("Plantower recuperado; trama valida recibida");
                        receivedFrame = true;
                        unavailableReported = false;
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
        output = {};
        if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            _data.valid = _data.freshAt(millis(), _lastValidMs);
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
