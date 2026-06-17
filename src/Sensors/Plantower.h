#ifndef PLANTOWER_H
#define PLANTOWER_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "../Config.h"
#include <Eolo/Types/PlantowerData.h>

class PlantowerParser {
public:
    enum State { HEAD1, HEAD2, LENGTH_H, LENGTH_L, DATA, CHECKSUM };
    static constexpr uint16_t ExpectedLen = 28;

    PlantowerParser() : _state(HEAD1) {}

    bool processByte(uint8_t ch) {
        switch (_state) {
            case HEAD1:
                if (ch == 0x42) {
                    _state = HEAD2;
                    _calcChecksum = 0x42;
                }
                break;
            case HEAD2:
                if (ch == 0x4d) {
                    _state = LENGTH_H;
                    _calcChecksum += 0x4d;
                } else {
                    _state = HEAD1;
                }
                break;
            case LENGTH_H:
                _packetLen = (ch << 8);
                _calcChecksum += ch;
                _state = LENGTH_L;
                break;
            case LENGTH_L:
                _packetLen |= ch;
                _calcChecksum += ch;
                if (_packetLen != ExpectedLen) {
                    _state = HEAD1; 
                    _bufIdx = 0;
                } else { 
                    _state = DATA; 
                    _bufIdx = 0; 
                }
                break;
            case DATA:
                _buffer[_bufIdx++] = ch;
                if (_bufIdx < _packetLen - 2) {
                    _calcChecksum += ch;
                } else {
                    _state = CHECKSUM;
                    _bufIdx = 0;
                }
                break;
            case CHECKSUM:
                if (_bufIdx == 0) {
                    _recvChecksum = (ch << 8);
                    _bufIdx++;
                } else {
                    _recvChecksum |= ch;
                    bool isValid = (_calcChecksum == _recvChecksum);
                    if (isValid) {
                        _data.pm1_0  = (_buffer[6] << 8) | _buffer[7];
                        _data.pm2_5  = (_buffer[8] << 8) | _buffer[9];
                        _data.pm10_0 = (_buffer[10] << 8) | _buffer[11];
                        _data.valid = true;
                    }
                    _state = HEAD1;
                    return isValid;
                }
                break;
        }
        return false;
    }

    PlantowerData getData() const { return _data; }

private:
    State _state;
    uint8_t _buffer[32];
    uint8_t _bufIdx = 0;
    uint16_t _calcChecksum = 0;
    uint16_t _packetLen = 0;
    uint16_t _recvChecksum = 0;
    PlantowerData _data = {0, 0, 0, false};
};

class Plantower {
private:
    HardwareSerial* _serial;
    int _rxPin, _txPin;
    TaskHandle_t _taskHandle = nullptr;
    SemaphoreHandle_t _mutex;
    PlantowerData _data;
    PlantowerParser _parser;
    
    static void taskWorker(void* arg) {
        Plantower* self = (Plantower*)arg;
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

    void begin() {
        _serial = &Serial2;
        _rxPin = PT_RX;
        _txPin = PT_TX;
        // Prio 1: sensor de fondo, sin requisito de tiempo real duro; nucleo 1 para liberar nucleo 0 al bus RS485.
        // Stack 2048: lectura UART directa sin buffers adicionales; medir con uxTaskGetStackHighWaterMark en DEBUG.
        static const UBaseType_t kTaskPriority = 1;
        static const uint32_t kTaskStack = 2048;
        xTaskCreatePinnedToCore(taskWorker, "PlantowerTask", kTaskStack, this, kTaskPriority, &_taskHandle, 1);
    }   

    bool getData(PlantowerData& output) {
        bool success = false;
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            output = _data;
            success = _data.valid;
            xSemaphoreGive(_mutex);
        }
        else{
            if (EoloDebug::verboseLogsEnabled()) {
                LOG_F("Error: no se pudo obtener el mutex para leer datos del Plantower\n");
            }
        }
        return success;
    }
};

#endif
