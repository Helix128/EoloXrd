#include <Arduino.h>

#if !defined(EOLO_MODEL_DRON) && !defined(EOLO_MODEL_STANDARD) && \
    !defined(EOLO_MODEL_EXPRESS) && !defined(EOLO_MODEL_EXPRESS_LEGACY)
#define EOLO_MODEL_STANDARD
#endif
#include "../EoloDemoPinout.h"

#if PT_RX_PIN < 0 || PT_TX_PIN < 0
  #error "El perfil seleccionado no tiene pines Plantower configurados"
#endif

/*
  Demo independiente Plantower PMS por UART.

  No depende de archivos de src.
  Abrir esta carpeta/sketch en Arduino IDE y seleccionar una placa ESP32.

  Conexiones usadas segun EOLO_MODEL_* en ../EoloDemoPinout.h.
*/

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t PLANTOWER_BAUD = 9600;

HardwareSerial plantowerSerial(2);

struct PlantowerData {
  uint16_t pm1_0;
  uint16_t pm2_5;
  uint16_t pm10_0;
};

class PlantowerParser {
public:
  bool processByte(uint8_t ch, PlantowerData &out) {
    switch (state) {
      case HEAD1:
        if (ch == 0x42) {
          checksum = ch;
          state = HEAD2;
        }
        break;

      case HEAD2:
        if (ch == 0x4D) {
          checksum += ch;
          state = LENGTH_H;
        } else {
          state = HEAD1;
        }
        break;

      case LENGTH_H:
        packetLen = ch << 8;
        checksum += ch;
        state = LENGTH_L;
        break;

      case LENGTH_L:
        packetLen |= ch;
        checksum += ch;
        dataIndex = 0;
        state = (packetLen == 28) ? DATA : HEAD1;
        break;

      case DATA:
        buffer[dataIndex++] = ch;
        checksum += ch;
        if (dataIndex >= packetLen - 2) {
          dataIndex = 0;
          receivedChecksum = 0;
          state = CHECKSUM_H;
        }
        break;

      case CHECKSUM_H:
        receivedChecksum = ch << 8;
        state = CHECKSUM_L;
        break;

      case CHECKSUM_L:
        receivedChecksum |= ch;
        state = HEAD1;
        if (checksum == receivedChecksum) {
          out.pm1_0 = word(buffer[6], buffer[7]);
          out.pm2_5 = word(buffer[8], buffer[9]);
          out.pm10_0 = word(buffer[10], buffer[11]);
          return true;
        }
        Serial.println("Checksum Plantower invalido");
        break;
    }

    return false;
  }

private:
  enum ParserState { HEAD1, HEAD2, LENGTH_H, LENGTH_L, DATA, CHECKSUM_H, CHECKSUM_L };

  ParserState state = HEAD1;
  uint8_t buffer[32] = {0};
  uint8_t dataIndex = 0;
  uint16_t checksum = 0;
  uint16_t packetLen = 0;
  uint16_t receivedChecksum = 0;
};

PlantowerParser parser;
uint32_t lastFrameMs = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  plantowerSerial.begin(PLANTOWER_BAUD, SERIAL_8N1, PT_RX_PIN, PT_TX_PIN);

  Serial.println();
  Serial.println("Demo Plantower independiente");
  Serial.println("Esperando tramas PMS por Serial2...");
}

void loop() {
  PlantowerData data;

  while (plantowerSerial.available()) {
    uint8_t ch = plantowerSerial.read();
    if (parser.processByte(ch, data)) {
      lastFrameMs = millis();
      Serial.printf("PLANTOWER OK | PM1.0=%u ug/m3 | PM2.5=%u ug/m3 | PM10=%u ug/m3\n",
                    data.pm1_0, data.pm2_5, data.pm10_0);
    }
  }

  static uint32_t lastStatusMs = 0;
  if (millis() - lastStatusMs >= 3000) {
    lastStatusMs = millis();
    if (lastFrameMs == 0) {
      Serial.println("Sin tramas aun. Revisar TX/RX, energia y GND comun.");
    } else {
      Serial.printf("Ultima trama valida hace %lu ms\n", millis() - lastFrameMs);
    }
  }
}
