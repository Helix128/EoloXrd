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
#include "../Board/Modem.h"
#include "../Sensors/Anemometer.h"
#include "../Utility/SensorAPI.h"
#include "Profiler.h" 

typedef struct Components{
  Input input;        // Manejo entradas (encoder/botón)
  MotorManager motor; // Control de motor
  #ifdef EOLO_GRANDE
    Anemometer anemometer; // Anemómetro ultrasónico
    Modem modem;           // Módem celular    
    SensorAPI api = SensorAPI(&modem, 20); // API de sensores
  #endif
  AFM07 flowSensor;    // Sensor de flujo de aire
  Plantower plantower; // Sensor Plantower
  BME280 bme;          // Sensor BME280
  Battery battery;     // Monitoreo del nivel de batería
  RTCManager rtc;      // Manejo RTC

  public: 
  void begin(){
    LOG_LN("Iniciando componentes de hardware...");
    digitalWrite(PPH_PWR_PIN, HIGH);

    input.begin();
    //motor.begin();
   // motor.setPowerPct(100); // ENCENDER MOTORES AL MAXIMO
    flowSensor.begin();
   // motor.setPowerPct(0); // APAGAR MOTORES
    bme.begin();
    rtc.begin();
    plantower.begin();
    
    #ifdef EOLO_GRANDE
      anemometer.begin();
    #endif

    battery.begin();
    int batteryLevel = battery.getRawLevel();
    float batteryPct = battery.getPct();
    Serial.print("Nivel de batería: ");
    Serial.print(batteryLevel);
    Serial.print(" (");
    Serial.print(batteryPct);
    Serial.println("%)");
    LOG_LN("Inicialización de componentes completa");
    
  }

  void poll() {
#ifdef EOLO_GRANDE

    Profiler p("Components poll");
    //api.update();
    static unsigned long lastBatPoll = 0;
    if (millis() - lastBatPoll > 1000) {
      battery.pollFromI2C();
      lastBatPoll = millis();
    }
#endif
  }
};

#endif