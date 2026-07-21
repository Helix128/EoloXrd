#include <Arduino.h>
#include <Eolo/Core/Sensors/PlantowerParser.h>
#include <Eolo/Types/PlantowerData.h>

#if !defined(EOLO_MODEL_DRON) && !defined(EOLO_MODEL_STANDARD) && \
    !defined(EOLO_MODEL_EXPRESS) && !defined(EOLO_MODEL_EXPRESS_LEGACY)
#define EOLO_MODEL_STANDARD
#endif
#include "../EoloDemoPinout.h"

#if PT_RX_PIN < 0 || PT_TX_PIN < 0
  #error "El perfil seleccionado no tiene pines Plantower configurados"
#endif

/*
  Demo PlatformIO Plantower PMS por UART usando el parser de EoloCore.

  Conexiones usadas segun EOLO_MODEL_* en ../EoloDemoPinout.h.
*/

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t PLANTOWER_BAUD = 9600;

HardwareSerial plantowerSerial(2);

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
    if (parser.processByte(ch)) {
      data = parser.getData();
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
