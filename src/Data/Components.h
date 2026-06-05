#ifndef HARDWARE_H
#define HARDWARE_H

#include "../Board/Input.h"
#include "../Effectors/Motor.h"
#include "../Effectors/StatusLed.h"
#include "../Board/RTCManager.h"
#include "../Sensors/AFM07.h"
#include "../Sensors/FS3000.h"
#include "../Sensors/Plantower.h"
#include "../Sensors/BME280.h"
#include "../Sensors/NTC.h"
#include "../Board/Battery.h"
#ifdef FEATURE_MODEM
#include "../Board/Modem.h"
#include "../Board/ModemService.h"
#include "../Utility/SensorAPI.h"
#endif
#include "../Sensors/Anemometer.h"
#include "Profiler.h" 

typedef struct Components{
#ifndef FEATURE_HEADLESS
  Input input;        // Manejo entradas (encoder/botón)
#endif
  MotorManager motor; // Control de motor
  StatusLed statusLed; // Indicador NeoPixel / no-op si no esta habilitado

  #ifdef FEATURE_ANEMOMETER
    Anemometer anemometer; // Anemómetro ultrasónico
  #endif

  #ifdef FEATURE_MODEM
    Modem modem;           // Módem celular    
    ModemService modemService = ModemService(modem); // Servicio async del módem
    SensorAPI api = SensorAPI(&modemService, 20); // API de sensores
  #endif

  #ifdef FEATURE_FLOW_AFM07
    AFM07 flowSensor;    // Sensor de flujo de aire AFM07
  #elif defined(FEATURE_FLOW_FS3000)
    FS3K flowSensor;     // Sensor de flujo de aire FS3000 (Legacy)
  #endif

#ifdef FEATURE_PLANTOWER
  Plantower plantower; // Sensor Plantower
#endif
  BME280 bme;          // Sensor BME280
#ifdef FEATURE_NTC
  NTC ntc;             // Termistor NTC
#endif
  Battery battery;     // Monitoreo del nivel de batería
  RTCManager rtc;      // Manejo RTC

  public: 
  void begin(){
    LOG_LN("Iniciando componentes de hardware...");

#ifndef FEATURE_HEADLESS
    input.begin();
#endif
    //motor.begin();
   // motor.setPowerPct(100); // ENCENDER MOTORES AL MAXIMO
    statusLed.begin();
    statusLed.setPattern(StatusLedPattern::Boot);
    flowSensor.begin();
   // motor.setPowerPct(0); // APAGAR MOTORES
    bme.begin();
#ifdef FEATURE_NTC
    ntc.begin();
#endif
    rtc.begin();
#ifdef FEATURE_PLANTOWER
    plantower.begin();
#endif
    
    #ifdef FEATURE_ANEMOMETER
      anemometer.begin();
    #endif

    battery.begin();
    int batteryLevel = battery.getRawLevel();
    float batteryPct = battery.getPct();
    LOG_OUT("Nivel de batería: ");
    LOG_OUT(batteryLevel);
    LOG_OUT(" (");
    LOG_OUT(batteryPct);
    LOG_OUT_LN("%)");
    LOG_LN("Inicialización de componentes completa");
    
  }

  void poll() {
    motor.updateRamp();
    statusLed.poll();
    #ifdef FEATURE_DUAL_BATTERY
      //api.update();
      static unsigned long lastBatPoll = 0;
      if ((int32_t)(millis() - lastBatPoll) > 1000) {
        battery.pollFromI2C();
        lastBatPoll = millis();
      }
    #endif
  }
};

#endif
