#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_FS3000_Arduino_Library.h>

#if !defined(EOLO_MODEL_DRON) && !defined(EOLO_MODEL_STANDARD) && \
    !defined(EOLO_MODEL_EXPRESS) && !defined(EOLO_MODEL_EXPRESS_LEGACY)
#define EOLO_MODEL_DRON
#endif
#include "../EoloDemoPinout.h"

/*
  Demo independiente FS3000 por I2C.

  No depende de archivos de src.
  Libreria necesaria en Arduino IDE:
    - SparkFun FS3000 Arduino Library

  Conexiones usadas segun EOLO_MODEL_* en ../EoloDemoPinout.h.
*/

static const uint32_t SERIAL_BAUD = 115200;

FS3000 fs3000;
bool fsReady = false;

float flowFromVelocity(float speedMs) {
  if (speedMs <= 0.0f) {
    return 0.0f;
  }

  static const float velocityPoints[] = {0.06f, 0.34f, 0.70f, 1.06f, 1.42f, 1.74f, 2.02f, 2.30f, 2.60f, 2.80f, 3.00f, 3.33f};
  static const float flowPoints[] = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f};
  static const int pointCount = sizeof(velocityPoints) / sizeof(velocityPoints[0]);

  if (speedMs < velocityPoints[0]) {
    return 0.0f;
  }

  for (int i = 0; i < pointCount - 1; i++) {
    if (speedMs < velocityPoints[i + 1]) {
      float slope = (flowPoints[i + 1] - flowPoints[i]) / (velocityPoints[i + 1] - velocityPoints[i]);
      return flowPoints[i] + slope * (speedMs - velocityPoints[i]);
    }
  }

  float slope = (flowPoints[pointCount - 1] - flowPoints[pointCount - 2]) /
                (velocityPoints[pointCount - 1] - velocityPoints[pointCount - 2]);
  return flowPoints[pointCount - 1] + slope * (speedMs - velocityPoints[pointCount - 1]);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  Serial.println();
  Serial.println("Demo FS3000 independiente");

  fsReady = fs3000.begin();
  if (!fsReady || !fs3000.isConnected()) {
    Serial.println("No se encontro FS3000. Revisar I2C, energia y GND comun.");
    fsReady = false;
    return;
  }

  fs3000.setRange(AIRFLOW_RANGE_15_MPS);
  Serial.println("FS3000 inicializado.");
}

void loop() {
  if (!fsReady) {
    delay(2000);
    return;
  }

  float velocityMs = fs3000.readMetersPerSecond();
  float flowLpm = flowFromVelocity(velocityMs);

  Serial.printf("FS3000 OK | velocidad=%.3f m/s | flujo estimado=%.2f L/min\n",
                velocityMs, flowLpm);

  delay(1000);
}
