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
        Serial.print("Dispositivo I2C en 0x");
        if (addr < 16) Serial.print("0");
        Serial.print(addr, HEX);
        Serial.print(" (");
        Serial.print(addr);
        LOG_LN(")");
        count++;
      } else if (error == 4) {
        Serial.print("Error desconocido en 0x");
        if (addr < 16) Serial.print("0");
        Serial.println(addr, HEX);
      }
    }

    if (count == 0) {
      LOG_LN("No se encontraron dispositivos I2C.");
    } else {
      Serial.print("Total: ");
      Serial.print(count);
      LOG_LN(" dispositivo(s) encontrados.");
    }
    LOG_LN();
  }
};

uint8_t I2CUtility::count = 0;

#endif  // I2C_UTIL_H