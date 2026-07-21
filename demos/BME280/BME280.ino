#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>

#if !defined(EOLO_MODEL_DRON) && !defined(EOLO_MODEL_STANDARD) && \
    !defined(EOLO_MODEL_EXPRESS) && !defined(EOLO_MODEL_EXPRESS_LEGACY)
#define EOLO_MODEL_DRON
#endif
#include "../EoloDemoPinout.h"

/*
  Demo independiente BME280 por I2C.

  No depende de archivos de src.
  Dependencias declaradas para el ambiente PlatformIO:
    - Adafruit BME280 Library
    - Adafruit Unified Sensor

  Conexiones usadas segun EOLO_MODEL_* en ../EoloDemoPinout.h.

  Direccion por defecto: 0x76. Si no responde, probar 0x77.
*/

static const uint32_t SERIAL_BAUD = 115200;
static const uint8_t DEMO_BME280_ADDRESS = 0x76;

Adafruit_BME280 bme;
bool bmeReady = false;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  Serial.println();
  Serial.println("Demo BME280 independiente");

  bmeReady = bme.begin(DEMO_BME280_ADDRESS, &Wire);
  if (!bmeReady) {
    Serial.println("No se encontro BME280 en 0x76. Revisar cableado o probar direccion 0x77.");
  } else {
    Serial.println("BME280 inicializado.");
  }
}

void loop() {
  if (!bmeReady) {
    delay(2000);
    return;
  }

  float temperatureC = bme.readTemperature();
  float humidityPct = bme.readHumidity();
  float pressureHpa = bme.readPressure() / 100.0f;

  Serial.printf("BME280 OK | temp=%.2f C | humedad=%.2f %% | presion=%.2f hPa\n",
                temperatureC, humidityPct, pressureHpa);

  delay(1000);
}
