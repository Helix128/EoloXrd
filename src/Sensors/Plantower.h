#ifndef PLANTOWER_H
#define PLANTOWER_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define PT_RX 34
#define PT_TX 32

struct PlantowerData {
    uint16_t pm1_0;
    uint16_t pm2_5;
    uint16_t pm10_0;
    bool valid;
};

class Plantower {
private:
    HardwareSerial* _serial;
    int _rxPin, _txPin;
    TaskHandle_t _taskHandle = nullptr;
    SemaphoreHandle_t _mutex;
    PlantowerData _data;
    
    enum State { HEAD1, HEAD2, LENGTH_H, LENGTH_L, DATA, CHECKSUM };

    static void taskWorker(void* arg) {
        Plantower* self = (Plantower*)arg;
        uint8_t ch;
        State state = HEAD1;
        
        uint8_t buffer[32];
        uint8_t bufIdx = 0;
        uint16_t calcChecksum = 0;
        uint16_t packetLen = 0;
        uint16_t recvChecksum = 0;

        self->_serial->begin(9600, SERIAL_8N1, self->_rxPin, self->_txPin);

        while (true) {
            if (self->_serial->available()) {
                ch = self->_serial->read();

                switch (state) {
                    case HEAD1:
                        if (ch == 0x42) {
                            state = HEAD2;
                            calcChecksum = 0x42;
                        }
                        break;
                    case HEAD2:
                        if (ch == 0x4d) {
                            state = LENGTH_H;
                            calcChecksum += 0x4d;
                        } else {
                            state = HEAD1;
                        }
                        break;
                    case LENGTH_H:
                        packetLen = (ch << 8);
                        calcChecksum += ch;
                        state = LENGTH_L;
                        break;
                    case LENGTH_L:
                        packetLen |= ch;
                        calcChecksum += ch;
                        if (packetLen > 30) { 
                            state = HEAD1; 
                        } else { 
                            state = DATA; 
                            bufIdx = 0; 
                        }
                        break;
                    case DATA:
                        buffer[bufIdx++] = ch;
                        if (bufIdx < packetLen - 2) {
                            calcChecksum += ch;
                        } else {
                            state = CHECKSUM;
                            bufIdx = 0;
                        }
                        break;
                    case CHECKSUM:
                        if (bufIdx == 0) {
                            recvChecksum = (ch << 8);
                            bufIdx++;
                        } else {
                            recvChecksum |= ch;
                            
                            if (calcChecksum == recvChecksum) {
                                if (xSemaphoreTake(self->_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                                    self->_data.pm1_0  = (buffer[6] << 8) | buffer[7];
                                    self->_data.pm2_5  = (buffer[8] << 8) | buffer[9];
                                    self->_data.pm10_0 = (buffer[10] << 8) | buffer[11];
                                    self->_data.valid = true;
                                    xSemaphoreGive(self->_mutex);
                                }
                            }
                            state = HEAD1;
                        }
                        break;
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
        xTaskCreatePinnedToCore(taskWorker, "PlantowerTask", 2048, this, 1, &_taskHandle, 1);
    }

    bool getData(PlantowerData& output) {
        bool success = false;
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            output = _data;
            success = _data.valid;
            xSemaphoreGive(_mutex);
        }
        return success;
    }
};

#endif