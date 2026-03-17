#ifndef RS485_MANAGER_HPP
#define RS485_MANAGER_HPP

#include <Arduino.h>
#include <ModbusMaster.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define RS485_RX_PIN 35
#define RS485_TX_PIN 33
#define RS485_DE_RE_PIN 26
#define RS485_BAUD_RATE 4800

class RS485 {
private:
    static HardwareSerial& _getSerial() {
        return Serial1;
    }

    static ModbusMaster& _getNode() {
        static ModbusMaster node;
        return node;
    }

    static SemaphoreHandle_t& _getMutex() {
        static SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
        return mutex;
    }

    static bool& _isInitialized() {
        static bool initialized = false;
        return initialized;
    }

    static void _preTransmission() {
        digitalWrite(RS485_DE_RE_PIN, HIGH);
        delayMicroseconds(25);
    }

    static void _postTransmission() {
        delayMicroseconds(25);
        digitalWrite(RS485_DE_RE_PIN, LOW);
    }

public:
    static void begin() {
        if (_isInitialized()) return;

        pinMode(RS485_DE_RE_PIN, OUTPUT);
        digitalWrite(RS485_DE_RE_PIN, LOW);

        HardwareSerial& serial = _getSerial();
        serial.begin(RS485_BAUD_RATE, SERIAL_8N2, RS485_RX_PIN, RS485_TX_PIN);

        ModbusMaster& node = _getNode();
        node.preTransmission(_preTransmission);
        node.postTransmission(_postTransmission);

        _isInitialized() = true;
    }

    static bool readRegisters(uint8_t slaveId, uint16_t startReg, uint8_t count, uint16_t* dataBuffer) {
        bool success = false;
        SemaphoreHandle_t mutex = _getMutex();

        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ModbusMaster& node = _getNode();
            HardwareSerial& serial = _getSerial();

            node.begin(slaveId, serial);

            while(serial.available()) serial.read();

            uint8_t result = node.readHoldingRegisters(startReg, count);

            if (result == node.ku8MBSuccess) {
                for (uint8_t i = 0; i < count; i++) {
                    dataBuffer[i] = node.getResponseBuffer(i);
                }
                success = true;
            }
            
            xSemaphoreGive(mutex);
        }
        return success;
    }
};

#endif