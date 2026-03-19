#ifndef RS485_MANAGER_HPP
#define RS485_MANAGER_HPP

#include <Arduino.h>
#include <ModbusMaster.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "./Config.h"

#define RS485_RX_PIN 35
#define RS485_TX_PIN 33
#define RS485_DE_RE_PIN 26
#define RS485_BAUD_RATE 4800

#define DEBUG false

class RS485 {
private:
    HardwareSerial& _serial;
    ModbusMaster _node;
    SemaphoreHandle_t _mutex;
    bool _initialized;

    RS485() : _serial(Serial1), _initialized(false) {
        _mutex = xSemaphoreCreateMutex();
    }

    RS485(const RS485&) = delete;
    RS485& operator=(const RS485&) = delete;

    static void _preTransmission() {
        digitalWrite(RS485_DE_RE_PIN, HIGH);
    }

    static void _postTransmission() {
        Serial1.flush();
        digitalWrite(RS485_DE_RE_PIN, LOW);
    }

public:
    static RS485& getInstance() {
        static RS485 instance;
        return instance;
    }

    void begin() {
        if (_initialized) return;

        pinMode(RS485_DE_RE_PIN, OUTPUT);
        digitalWrite(RS485_DE_RE_PIN, LOW);

        _serial.begin(RS485_BAUD_RATE, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

        _node.preTransmission(_preTransmission);
        _node.postTransmission(_postTransmission);

        _initialized = true;
    }

    bool readRegisters(uint8_t slaveId, uint16_t startReg, uint8_t count, uint16_t* dataBuffer) {
        if (!_initialized) return false;

        bool success = false;
        
        vTaskDelay(pdMS_TO_TICKS(10)); 

        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            
            vTaskDelay(pdMS_TO_TICKS(10)); 

            while(_serial.available()) {
                _serial.read();
            }

            _node.begin(slaveId, _serial);
            uint8_t result = _node.readHoldingRegisters(startReg, count);

            if (result == _node.ku8MBSuccess) {
                for (uint8_t i = 0; i < count; i++) {
                    dataBuffer[i] = _node.getResponseBuffer(i);
                }
                success = true;
                #if DEBUG
                    LOG_F("Modbus read success: Slave ID %d, Start Reg %d, Count %d\n", slaveId, startReg, count);
                    for (uint8_t i = 0; i < count; i++) {
                        Serial.printf("  Reg %d: 0x%04X (%d)\n", startReg + i, dataBuffer[i], dataBuffer[i]);
                    }
                #endif
            }
            else{
                LOG_F("Modbus read failed: Slave ID %d, Start Reg %d, Count %d, Error Code: %02X \n", slaveId, startReg, count, result);
            }
            
            xSemaphoreGive(_mutex);
        }
        return success;
    }
};

#endif