#include <Arduino.h>

#if !defined(EOLO_MODEL_DRON) && !defined(EOLO_MODEL_STANDARD) && \
    !defined(EOLO_MODEL_EXPRESS) && !defined(EOLO_MODEL_EXPRESS_LEGACY)
#define EOLO_MODEL_STANDARD
#endif
#include "../EoloDemoPinout.h"

/*
  Demo independiente de anemometro RS485/Modbus RTU.

  No depende de archivos de src ni del RS485Bus del proyecto.
  Abrir esta carpeta/sketch en Arduino IDE y seleccionar una placa ESP32.

  Conexiones usadas segun EOLO_MODEL_* en ../EoloDemoPinout.h.

  Parametros esperados:
    Slave ID: 1
    Baud: 4800 8N1
    Registros desde 0x0000, count 2
    Velocidad: registro 0 / 100 = m/s
    Direccion: registro 1 = grados
*/

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t RS485_BAUD = 4800;
static const uint8_t ANEMOMETER_SLAVE_ID = 1;
static const uint16_t ANEMOMETER_START_REG = 0x0000;
static const uint16_t MODBUS_TIMEOUT_MS = 1000;

HardwareSerial rs485(2);

uint16_t modbusCrc(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x0001) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
    }
  }
  return crc;
}

bool readHoldingRegisters(uint8_t slaveId, uint16_t startReg, uint16_t count, uint16_t *out) {
  uint8_t request[8] = {
    slaveId,
    0x03,
    highByte(startReg),
    lowByte(startReg),
    highByte(count),
    lowByte(count),
    0,
    0
  };

  uint16_t crc = modbusCrc(request, 6);
  request[6] = lowByte(crc);
  request[7] = highByte(crc);

  while (rs485.available()) {
    rs485.read();
  }

  digitalWrite(RS485_DE_RE_PIN, HIGH);
  delayMicroseconds(200);
  rs485.write(request, sizeof(request));
  rs485.flush();
  delayMicroseconds(200);
  digitalWrite(RS485_DE_RE_PIN, LOW);

  const uint8_t expectedLen = 5 + (count * 2);
  uint8_t response[64];
  uint8_t idx = 0;
  uint32_t startMs = millis();

  while ((millis() - startMs) < MODBUS_TIMEOUT_MS && idx < expectedLen) {
    if (rs485.available()) {
      response[idx++] = rs485.read();
    }
  }

  if (idx != expectedLen) {
    Serial.printf("Timeout o trama incompleta: %u/%u bytes\n", idx, expectedLen);
    return false;
  }

  uint16_t receivedCrc = response[idx - 2] | (response[idx - 1] << 8);
  uint16_t calculatedCrc = modbusCrc(response, idx - 2);
  if (receivedCrc != calculatedCrc) {
    Serial.printf("CRC invalido: recibido 0x%04X calculado 0x%04X\n", receivedCrc, calculatedCrc);
    return false;
  }

  if (response[0] != slaveId || response[1] != 0x03 || response[2] != count * 2) {
    Serial.println("Respuesta Modbus inesperada");
    return false;
  }

  for (uint16_t i = 0; i < count; i++) {
    uint8_t offset = 3 + (i * 2);
    out[i] = (response[offset] << 8) | response[offset + 1];
  }

  return true;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  enableDemoPeripheralPower();

  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);

  rs485.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

  Serial.println();
  Serial.println("Demo anemometro independiente");
  Serial.println("Leyendo ID 1, registros 0x0000-0x0001 cada 1 segundo...");
}

void loop() {
  uint16_t regs[2] = {0, 0};
  if (readHoldingRegisters(ANEMOMETER_SLAVE_ID, ANEMOMETER_START_REG, 2, regs)) {
    float speedMs = regs[0] / 100.0f;
    float speedKph = speedMs * 3.6f;
    uint16_t directionDeg = regs[1];
    Serial.printf("ANEM OK | rawSpeed=%u | %.2f m/s | %.2f km/h | dir=%u deg\n",
                  regs[0], speedMs, speedKph, directionDeg);
  } else {
    Serial.println("Anemometro sin lectura valida. Revisar energia, A/B, GND comun, ID y baudrate.");
  }

  delay(1000);
}
