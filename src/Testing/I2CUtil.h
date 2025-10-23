#include <Arduino.h>
#include <Wire.h>
#ifndef I2C_UTIL_H
#define I2C_UTIL_H
class I2CUtility {
private:
  uint8_t count;

public:
  I2CUtility() : count(0) {}

  void begin() {
    Serial.begin(115200);
    while (!Serial) {}
    Wire.begin();
    Serial.println("I2C Scanner iniciado");
  }

  void scan() {
    count = 0;
    Serial.println("Escaneando bus I2C...");
    
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      uint8_t error = Wire.endTransmission();
      
      if (error == 0) {
        Serial.print("Dispositivo I2C en 0x");
        if (addr < 16) Serial.print("0");
        Serial.print(addr, HEX);
        Serial.print(" (");
        Serial.print(addr);
        Serial.println(")");
        count++;
      } else if (error == 4) {
        Serial.print("Error desconocido en 0x");
        if (addr < 16) Serial.print("0");
        Serial.println(addr, HEX);
      }
    }

    if (count == 0) {
      Serial.println("No se encontraron dispositivos I2C.");
    } else {
      Serial.print("Total: ");
      Serial.print(count);
      Serial.println(" dispositivo(s) encontrados.");
    }
    Serial.println();
  }
};
#endif  // I2C_UTIL_H