#include <Arduino.h>
#include "Wire.h"
#ifndef I2C_UTIL_H
#define I2C_UTIL_H
class I2CUtility {
private:
  static uint8_t count;

public:
  static void begin() {
    while (!Serial) {}
    Wire.begin();
    LOG_LN("I2C Scanner iniciado");
  }

  static void scan() {
    count = 0;
    LOG_LN("Escaneando bus I2C...");
    
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      uint8_t error = Wire.endTransmission();
      
      if (error == 0) {
        LOG_F("Dispositivo I2C encontrado en 0x%02X (%d)\n", addr, addr);
        count++;
      } else if (error == 4) {
        LOG_F("Error desconocido en dirección 0x%02X\n", addr);
      }
    }

    if (count == 0) {
      LOG_LN("No se encontraron dispositivos I2C.");
    } else {
      LOG_F("Total: %d dispositivo(s) encontrados.\n", count);
    }
    LOG_LN();
  }
};

uint8_t I2CUtility::count = 0;

#endif  // I2C_UTIL_H