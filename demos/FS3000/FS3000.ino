#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_FS3000_Arduino_Library.h>
#include <Eolo/Core/Sensors/FS3000FlowModel.h>

#if !defined(EOLO_MODEL_DRON) && !defined(EOLO_MODEL_STANDARD) && \
    !defined(EOLO_MODEL_EXPRESS) && !defined(EOLO_MODEL_EXPRESS_LEGACY)
#define EOLO_MODEL_DRON
#endif
#include "../EoloDemoPinout.h"

/*
  Demo PlatformIO FS3000 por I2C usando la conversión de EoloCore.

  Conexiones usadas segun EOLO_MODEL_* en ../EoloDemoPinout.h.
*/

static const uint32_t SERIAL_BAUD = 115200;

FS3000 fs3000;
bool fsReady = false;

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
  float flowLpm = FS3000FlowModel::flowFromVelocity(velocityMs);

  Serial.printf("FS3000 OK | velocidad=%.3f m/s | flujo estimado=%.2f L/min\n",
                velocityMs, flowLpm);

  delay(1000);
}
