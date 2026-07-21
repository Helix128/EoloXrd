#include <Arduino.h>
#include <ModbusMaster.h>

#if !defined(EOLO_MODEL_DRON) && !defined(EOLO_MODEL_STANDARD) && \
    !defined(EOLO_MODEL_EXPRESS) && !defined(EOLO_MODEL_EXPRESS_LEGACY)
#define EOLO_MODEL_EXPRESS
#endif
#include "../EoloDemoPinout.h"

/*
  Scanner Modbus RTU para AFM07 (o cualquier esclavo RS485).
  Barre baudrates comunes x IDs 1..20 leyendo el registro 0x0000.
  Reporta cualquier combinacion que responda con exito.

  Solo lecturas (no destructivo). Mismo cableado que el firmware Express.
*/

static const uint32_t SERIAL_BAUD = 115200;
static const uint16_t FLOW_REG = 0x0000;
static const uint32_t BAUDS[] = {4800, 9600, 19200, 38400};
static const int NUM_BAUDS = sizeof(BAUDS) / sizeof(BAUDS[0]);
static const uint8_t ID_MIN = 1;
static const uint8_t ID_MAX = 20;

HardwareSerial rs485(2);
ModbusMaster node;

void preTransmission()  { digitalWrite(RS485_DE_RE_PIN, HIGH); delayMicroseconds(200); }
void postTransmission() { delayMicroseconds(200); digitalWrite(RS485_DE_RE_PIN, LOW); }

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  enableDemoPeripheralPower();
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);

  Serial.println();
  Serial.println("### SCANNER MODBUS AFM07 ###");
  Serial.printf("RX=%d TX=%d DE/RE=%d | reg=0x%04X | IDs %u..%u\n",
                RS485_RX_PIN, RS485_TX_PIN, RS485_DE_RE_PIN, FLOW_REG, ID_MIN, ID_MAX);

  int hits = 0;
  for (int b = 0; b < NUM_BAUDS; b++) {
    uint32_t baud = BAUDS[b];
    rs485.begin(baud, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    Serial.printf("\n-- Baud %lu 8N1 --\n", (unsigned long)baud);
    for (uint8_t id = ID_MIN; id <= ID_MAX; id++) {
      node.begin(id, rs485);
      node.preTransmission(preTransmission);
      node.postTransmission(postTransmission);
      uint8_t r = node.readHoldingRegisters(FLOW_REG, 1);
      if (r == ModbusMaster::ku8MBSuccess) {
        uint16_t raw = node.getResponseBuffer(0);
        Serial.printf("  >>> RESPONDE ID=%u @ %lu baud | raw=%u (%.2f /10)\n",
                      id, (unsigned long)baud, raw, raw / 10.0f);
        hits++;
      }
      delay(60);
    }
  }
  Serial.println();
  if (hits == 0)
    Serial.println("Ningun esclavo respondio. Revisar energia, A/B (probar invertir), GND comun.");
  else
    Serial.printf("Scan completo: %d combinacion(es) con respuesta.\n", hits);
}

void loop() {}
