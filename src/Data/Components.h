#ifndef HARDWARE_H
#define HARDWARE_H

#include "../Board/Input.h"
#include "../Effectors/Motor.h"
#include "../Board/RTCManager.h"
#include "../Sensors/AFM07.h"
#include "../Sensors/FS3000.h"
#include "../Sensors/Plantower.h"
#include "../Sensors/BME280.h"
#include "../Board/Battery.h"

typedef struct Components{
  Input input;        // Manejo entradas (encoder/botón)
  MotorManager motor; // Control de motor
  // AFM07 flowSensor;    // Sensor de flujo
  FS3K flowSensor;     // Sensor de velocidad de aire
  Plantower plantower; // Sensor Plantower
  BME280 bme;          // Sensor BME280
  Battery battery;     // Monitoreo del nivel de batería
  RTCManager rtc;      // Manejo RTC

  public: 
  void begin(){
    input.begin();
    motor.begin();
    motor.setPowerPct(100); // ENCENDER MOTORES AL MAXIMO
    flowSensor.begin();
    motor.setPowerPct(0); // APAGAR MOTORES
    bme.begin();
    plantower.begin();
    rtc.begin();

    int batteryLevel = battery.getLevel();
    float batteryPct = battery.getPct();
    Serial.print("Nivel de batería: ");
    Serial.print(batteryLevel);
    Serial.print(" (");
    Serial.print(batteryPct);
    Serial.println("%)");

    Serial.println("Inicialización de componentes completa");
    
  }
}Components;

#endif