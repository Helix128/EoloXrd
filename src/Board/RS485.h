#ifndef RS485_MANAGER_HPP
#define RS485_MANAGER_HPP

#include <Arduino.h>
#include <ModbusMaster.h>
#include <SoftwareSerial.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// PINES (Pin 26 ELIMINADO porque tu módulo es automático)
#define RS485_RX_PIN 35
#define RS485_TX_PIN 33
#define RS485_BAUD_RATE 4800

class RS485 {
private:
    static SoftwareSerial& _getSerial() {
        static SoftwareSerial swSerial;
        return swSerial;
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

    // --- ELIMINAMOS CONTROL MANUAL DE/RE ---
    // Tu módulo azul hace esto solo, no necesitamos código aquí.
    static void _preTransmission() {
        // Nada que hacer, el hardware es automático
    }

    static void _postTransmission() {
        // Nada que hacer
    }

public:
    static void begin() {
        if (_isInitialized()) return;

        // Iniciamos SoftwareSerial en los pines 35 y 33
        SoftwareSerial& serial = _getSerial();
        serial.begin(RS485_BAUD_RATE, SWSERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
        
        ModbusMaster& node = _getNode();
        
        // Aunque las funciones estén vacías, ModbusMaster las pide.
        node.preTransmission(_preTransmission);
        node.postTransmission(_postTransmission);
        
        _isInitialized() = true;
    }

    static bool readRegisters(uint8_t slaveId, uint16_t startReg, uint8_t count, uint16_t* dataBuffer) {
        bool success = false;
        SemaphoreHandle_t mutex = _getMutex();

        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ModbusMaster& node = _getNode();
            SoftwareSerial& serial = _getSerial();
            
            node.begin(slaveId, serial);
            
            // Limpieza buffer
            while(serial.available()) serial.read();

            uint8_t result = node.readHoldingRegisters(startReg, count);

            if (result == node.ku8MBSuccess) {
                for (uint8_t i = 0; i < count; i++) {
                    dataBuffer[i] = node.getResponseBuffer(i);
                }
                Serial.printf("Modbus ID %d: Lectura exitosa de %d registros desde 0x%04X\n", slaveId, count, startReg);
                success = true;
            }
            // Debug opcional
            else { Serial.printf("Error Modbus ID %d: 0x%02X\n", slaveId, result); }

            xSemaphoreGive(mutex);
        }
        return success;
    }
};

#endif