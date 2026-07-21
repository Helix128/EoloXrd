#include <Arduino.h>

/*
  Diagnóstico AFM07 con el mapa RS485 autoritativo de EOLO Express:
    RS485 DI (TX ESP32) = 25
    RS485 RO (RX ESP32) = 27
    DE/RE               = 26
    Serial1, Modbus RTU a mano (sin libreria)

  Objetivo: repetir el diagnóstico sobre el mismo cableado que usa el
  firmware Express. Las constantes vienen de EoloDemoPinout/Pinout.h.

  Prueba DOS configuraciones en secuencia:
    A) ID 0x01 @ 9600 (config original de la demo del profe)
    B) ID 0x02 @ 4800 (config actual del sensor del usuario)
*/

#if !defined(EOLO_MODEL_EXPRESS)
#define EOLO_MODEL_EXPRESS
#endif
#include "../EoloDemoPinout.h"

#define PIN_RS485_DI    RS485_TX_PIN
#define PIN_RS485_RO    RS485_RX_PIN
#define PIN_RS485_DERE  RS485_DE_RE_PIN

HardwareSerial RS485Serial(1);

uint16_t crc16Modbus(const uint8_t* d, uint8_t len) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= d[i];
    for (uint8_t j = 0; j < 8; j++)
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

// Retorna -1 timeout, -2 CRC, o L/min (raw/10)
float afm07_leerFlujo(uint8_t addr) {
  while (RS485Serial.available()) RS485Serial.read();
  uint8_t t[8] = { addr, 0x03, 0x00, 0x00, 0x00, 0x01, 0, 0 };
  uint16_t crc = crc16Modbus(t, 6);
  t[6] = crc & 0xFF; t[7] = crc >> 8;

  digitalWrite(PIN_RS485_DERE, HIGH);
  delayMicroseconds(500);
  RS485Serial.write(t, 8);
  RS485Serial.flush();
  delayMicroseconds(500);
  digitalWrite(PIN_RS485_DERE, LOW);

  uint32_t t0 = millis();
  while (RS485Serial.available() < 7 && millis() - t0 < 300) delay(1);
  if (RS485Serial.available() < 7) return -1.0F;
  uint8_t r[7];
  for (int i = 0; i < 7; i++) r[i] = RS485Serial.read();
  if (crc16Modbus(r, 5) != (r[5] | (uint16_t)r[6] << 8)) return -2.0F;
  return ((uint16_t)(r[3] << 8) | r[4]) / 10.0F;
}

void probar(const char* etiqueta, uint32_t baud, uint8_t addr) {
  Serial.printf("\n== %s (ID=0x%02X @ %lu 8N1) pines DI=%d RO=%d DE/RE=%d ==\n",
                etiqueta, addr, (unsigned long)baud,
                PIN_RS485_DI, PIN_RS485_RO, PIN_RS485_DERE);
  RS485Serial.begin(baud, SERIAL_8N1, PIN_RS485_RO, PIN_RS485_DI);
  delay(50);
  for (int i = 1; i <= 5; i++) {
    float f = afm07_leerFlujo(addr);
    Serial.printf("  Lectura #%d: ", i);
    if      (f == -1.0F) Serial.println("TIMEOUT");
    else if (f == -2.0F) Serial.println("ERROR CRC");
    else                 Serial.printf(">>> %.1f L/min <<<\n", f);
    delay(400);
  }
  RS485Serial.end();
}

void setup() {
  Serial.begin(115200);
  delay(800);
  pinMode(PIN_RS485_DERE, OUTPUT);
  digitalWrite(PIN_RS485_DERE, LOW);

  Serial.printf("\n### TEST AFM07 RS485 Express (%d/%d/%d) ###\n",
                PIN_RS485_DI, PIN_RS485_RO, PIN_RS485_DERE);
  probar("A) config profe", 9600, 0x01);
  probar("B) config usuario", 4800, 0x02);
  Serial.println("\nFin del diagnóstico AFM07.");
}

void loop() {}
