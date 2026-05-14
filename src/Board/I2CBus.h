#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <Arduino.h>
#include "Wire.h"
#include "../Config.h"

class I2CBus {
private:
    bool _ready = false;
    uint8_t _lastScanCount = 0;

    I2CBus() {}

    I2CBus(const I2CBus&) = delete;
    I2CBus& operator=(const I2CBus&) = delete;

public:
    static I2CBus& getInstance() {
        static I2CBus instance;
        return instance;
    }

    bool begin() {
        if (_ready) {
            return true;
        }

        Wire.begin(SDA_PIN, SCL_PIN);
        Wire.setClock(I2C_CLOCK);
        _ready = true;

        LOG_F("I2C iniciado: SDA=%d, SCL=%d, clock=%lu Hz\n",
              SDA_PIN, SCL_PIN, (unsigned long)I2C_CLOCK);
        return true;
    }

    bool isReady() const {
        return _ready;
    }

    uint8_t scan() {
        begin();
        _lastScanCount = 0;

        LOG_LN("Escaneando bus I2C...");
        for (uint8_t addr = 1; addr < 127; addr++) {
            Wire.beginTransmission(addr);
            uint8_t error = Wire.endTransmission();

            if (error == 0) {
                LOG_F("Dispositivo I2C encontrado en 0x%02X (%d)\n", addr, addr);
                _lastScanCount++;
            } else if (error == 4) {
                LOG_F("Error desconocido en direccion I2C 0x%02X\n", addr);
            }
        }

        if (_lastScanCount == 0) {
            LOG_LN("No se encontraron dispositivos I2C");
        } else {
            LOG_F("Total I2C: %d dispositivo(s) encontrados\n", _lastScanCount);
        }

        return _lastScanCount;
    }

    uint8_t getLastScanCount() const {
        return _lastScanCount;
    }

    bool reset() {
        _ready = false;
        Wire.end();
        return begin();
    }

    bool writeCommand(uint8_t addr, uint8_t command, bool logError = true) {
        return writeBytes(addr, &command, 1, logError);
    }

    bool writeBytes(uint8_t addr, const uint8_t* data, size_t len, bool logError = true) {
        begin();

        Wire.beginTransmission(addr);
        for (size_t i = 0; i < len; i++) {
            Wire.write(data[i]);
        }

        uint8_t error = Wire.endTransmission();
        if (error != 0) {
            if (logError) {
                LOG_F("I2C write fallo: addr=0x%02X, len=%u, error=%u\n",
                      addr, (unsigned int)len, error);
            }
            return false;
        }

        return true;
    }

    bool readBytes(uint8_t addr, uint8_t* buffer, size_t len, bool logError = true) {
        begin();

        size_t readCount = Wire.requestFrom(addr, len);
        if (readCount != len) {
            if (logError) {
                LOG_F("I2C read corto: addr=0x%02X, esperado=%u, recibido=%u\n",
                      addr, (unsigned int)len, (unsigned int)readCount);
            }
            while (Wire.available()) {
                Wire.read();
            }
            return false;
        }

        for (size_t i = 0; i < len; i++) {
            buffer[i] = Wire.read();
        }

        return true;
    }

    bool writeThenRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                       uint8_t* rx, size_t rxLen, bool logError = true) {
        begin();

        Wire.beginTransmission(addr);
        for (size_t i = 0; i < txLen; i++) {
            Wire.write(tx[i]);
        }

        uint8_t error = Wire.endTransmission(false);
        if (error != 0) {
            if (logError) {
                LOG_F("I2C writeThenRead write fallo: addr=0x%02X, txLen=%u, error=%u\n",
                      addr, (unsigned int)txLen, error);
            }
            return false;
        }

        return readBytes(addr, rx, rxLen, logError);
    }
};

#endif
