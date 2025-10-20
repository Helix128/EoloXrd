#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

// Incluye TODOS los módulos
#include "Board/Input.h"
#include "Effectors/Motor.h"
#include "Board/RTCManager.h"
#include "Sensors/AFM07.h"
#include "Sensors/FS3000.h"
#include "Sensors/Plantower.h"
#include "Sensors/BME280.h"
#include "Board/Battery.h"
#include "Board/Logger.h"
#include "Config.h"
#include <U8g2lib.h>

typedef struct AppContext
{
  // --- MÓDULOS DE HARDWARE ---
  DisplayModel &u8g2; // Referencia al display
  Input input;        // Manejo entradas (encoder/botón)
  MotorManager motor; // Control de motor
  // AFM07 flowSensor;    // Sensor de flujo
  FS3K flowSensor;     // Sensor de velocidad de aire
  Plantower plantower; // Sensor Plantower
  BME280 bme;          // Sensor BME280
  Battery battery;     // Monitoreo del nivel de batería
  Logger logger;       // Registro de datos en SD
  RTCManager rtc;      // Manejo RTC

  // --- ESTADO DE LA APLICACIÓN ---
  // Flujo y temporización de la aplicación
  float flujoObjetivo = 5.0;           // Flujo objetivo en L/min
  unsigned long horaInicioCaptura = 0; // Timestamp de inicio
  unsigned long horaFinCaptura = 3600; // Timestamp de fin
  bool usarSensorPM = true;            // Usar sensor plantower (PM)
  bool rtcAvailable = false;           // true si rtc.begin() tuvo éxito

  // Estado de la interfaz (para selectores de opciones)
  int seleccionMenu = 0;

  // Encoder virtual para evitar el límite 0..255 del encoder físico
  int encoderVirtual = 0; // minutos del día (0..1439) durante edición
  // NOTE: prevEncoderRaw fue movido al Input; usar ctx.input.getEncoderValue() cuando sea necesario

  // Estado de la sesión
  bool capturaActiva = false;                // Whether capture is currently active
  unsigned long tiempoTranscurrido = 0;      // Elapsed time in current session
  unsigned long captureIntervalSeconds = 10; // Interval for periodic capture/logging
  unsigned long lastCaptureTime = 0;         // Last timestamp (epoch) a sample was logged
  // Pause/resume support
  bool paused = false;                // true if session is paused
  unsigned long remainingSeconds = 0; // remaining seconds when paused
  // Flag para transición a EndScene
  bool shouldTransitionToEnd = false;

  // Control del motor durante captura
  float motorPowerPct = 0.0;                     // Potencia actual del motor (0-100%)
  float errorIntegral = 0.0;                     // Acumulador de error integral
  unsigned long lastMotorAdjust = 0;             // Última vez que se ajustó el motor
  const float Kp = 1.5;                          // Ganancia proporcional
  const float Ki = 0.35;                         // Ganancia integral
  const float Kd = 0.75;                         // Ganancia derivativa
  float lastError = 0.0;                         // Error previo para cálculo derivativo
  const unsigned long motorAdjustInterval = 100; // Ajustar motor cada 500ms
  const float maxMotorChangePct = 1.5;           // Máximo cambio de potencia por ajuste (±5%)

  // Constructor
  AppContext(DisplayModel &display)
      : u8g2(display)
  {
  }

  void begin()
  {
    Serial.begin(115200);
    while (!Serial)
      delay(100);
    Serial.println("EOLO Boot");

    // Inicializa todos los módulos
    u8g2.setI2CAddress(0x3C * 2);
    u8g2.begin();
    input.begin();
    motor.begin();
    motor.setPowerPct(100);
    flowSensor.begin();
    motor.setPowerPct(0);
    bme.begin();
    plantower.begin();
    rtcAvailable = rtc.begin();

    int battery = getBatteryPercentage();
    Serial.print("Nivel de batería: ");
    Serial.print(battery);
    Serial.println("%");

    // Inicializa el datalogger
    if (logger.begin())
    {
      Serial.println("Logger iniciado correctamente");
    }
    else
    {
      Serial.println("Advertencia: Fallo en la inicialización del logger");
    }

    Serial.println("Inicialización de AppContext completa");
  }

  // Devuelve el timestamp actual en segundos (RTC si está disponible, sino uptime)
  unsigned long nowSeconds()
  {
    if (rtcAvailable)
    {
      return rtc.now().unixtime();
    }
    return millis() / 1000;
  }

  // Devuelve epoch del próximo ocurrir del minuto del día (0..1439)
  unsigned long epochForNextMinuteOfDay(int minuteOfDay)
  {
    unsigned long now = nowSeconds();
    unsigned long today0 = now - (now % 86400UL);
    unsigned long candidate = today0 + (minuteOfDay * 60UL);
    if (candidate <= now)
      candidate += 86400UL;
    return candidate;
  }

  // Método para actualizar el contexto de la app
  void update()
  {
    input.poll();

    // Actualiza tiempo de captura si está activa (preferir RTC epoch)
    unsigned long now = nowSeconds();

    if (capturaActiva)
    {
      if (horaInicioCaptura > 0)
      {
        tiempoTranscurrido = now - horaInicioCaptura;
      }

      // Control PID del motor para mantener flujo objetivo
      flowSensor.readData();
      adjustMotorPower(flowSensor.flow);

      // Termina la captura si se ha alcanzado la hora de fin absoluta
      if (horaFinCaptura > 0 && now >= horaFinCaptura)
      {
        stopCapture();
        shouldTransitionToEnd = true; // Marcar para transición
      }

      // Logging periódico: actualizar lastCaptureTime
      if (lastCaptureTime == 0)
      {
        lastCaptureTime = now;
      }
      else if (now - lastCaptureTime >= captureIntervalSeconds)
      {
        lastCaptureTime = now;
        // Guardar muestra en SD si está disponible
        if (Logger::status == SD_OK)
        {
          logger.capture(
              now,
              flowSensor.flow,
              flujoObjetivo,
              bme.temperature,
              bme.humidity,
              bme.pressure,
              plantower.pm1,
              plantower.pm25,
              plantower.pm10,
              getBatteryPercentage());
        }
      }
    }
  }

  // Ajusta la potencia del motor usando control PID para alcanzar el flujo objetivo
  void adjustMotorPower(float flow)
  {
    unsigned long nowMillis = millis();
    if (lastMotorAdjust == 0 || nowMillis - lastMotorAdjust >= motorAdjustInterval)
    {
      lastMotorAdjust = nowMillis;

      float flujoActual = flow;

      // Calcular error (positivo = necesitamos más flujo, negativo = tenemos demasiado)
      float error = flujoObjetivo - flujoActual;

      // Zona muerta amplia: si estamos cerca del objetivo, no hacer nada
      if (abs(error) < 1.0) // Tolerancia de ±1.0 L/min
      {
        // Mantener potencia actual y resetear integral lentamente
        errorIntegral *= 0.9; // Decaimiento gradual del integral
      }
      else
      {
        // Término proporcional
        float P = Kp * error;

        // Término integral con anti-windup
        errorIntegral += error * (motorAdjustInterval / 1000.0);
        errorIntegral = constrain(errorIntegral, -20.0, 50.0);
        float I = Ki * errorIntegral;

        // Calcular cambio de potencia
        float change = P + I;

        // Si necesitamos más flujo (error > 0): subir suavemente
        // Si tenemos demasiado flujo (error < 0): bajar más rápido pero no bruscamente
        if (error > 0)
        {
          // Subir suavemente
          change = constrain(change, 0.0, maxMotorChangePct);
        }
        else
        {
          // Bajar moderadamente cuando nos pasamos
          change = constrain(change, -maxMotorChangePct * 1.2, 0.0);
        }

        motorPowerPct += change;

        // Asegurar que esté dentro de 0-100%
        motorPowerPct = constrain(motorPowerPct, 0.0, 100.0);

        // Aplicar nuevo valor al motor
        motor.setPowerPct((int)motorPowerPct);
      }

      lastError = error;

      // Debug
      Serial.print("Flujo: ");
      Serial.print(flujoActual, 2);
      Serial.print(" L/min | Objetivo: ");
      Serial.print(flujoObjetivo, 2);
      Serial.print(" | Error: ");
      Serial.print(error, 2);
      Serial.print(" | Motor: ");
      Serial.print(motorPowerPct, 1);
      Serial.println("%");
    }
  }

  // Helpers para operaciones comunes
  void startCapture()
  {
    unsigned long now = nowSeconds();
    if (!rtcAvailable)
      Serial.println("Warning: RTC not available, using uptime seconds as timestamp");

    // Si horaFinCaptura parece ser una duracion relativa (<= 24h), convertir a epoch absoluto
    if (horaFinCaptura > 0 && horaFinCaptura <= 86400UL)
    {
      horaFinCaptura = now + horaFinCaptura;
    }

    horaInicioCaptura = now;
    tiempoTranscurrido = 0;
    capturaActiva = true;
    lastCaptureTime = now;

    // Inicializar variables del control PID
    motorPowerPct = 50.0; // Empezar con 50% de potencia
    errorIntegral = 0.0;
    lastError = 0.0;
    lastMotorAdjust = 0;
    motor.setPowerPct(50);

    Serial.println("Capture started");
  }

  void pauseCapture()
  {
    if (!capturaActiva)
      return;
    unsigned long now = nowSeconds();
    if (horaFinCaptura > now)
      remainingSeconds = horaFinCaptura - now;
    else
      remainingSeconds = 0;
    capturaActiva = false;
    paused = true;

    // Apagar el motor al pausar
    motor.setPowerPct(0);

    Serial.print("Capture paused, remaining secs: ");
    Serial.println(remainingSeconds);
  }

  void resumeCapture()
  {
    if (!paused)
      return;
    unsigned long now = nowSeconds();
    horaInicioCaptura = now;
    horaFinCaptura = now + remainingSeconds;
    tiempoTranscurrido = 0;
    capturaActiva = true;
    paused = false;
    lastCaptureTime = now;

    // Reiniciar control del motor al reanudar
    motorPowerPct = 50.0;
    errorIntegral = 0.0;
    lastError = 0.0;
    lastMotorAdjust = 0;
    motor.setPowerPct(50);

    Serial.println("Capture resumed");
  }

  void stopCapture()
  {
    capturaActiva = false;
    // reset state for next session
    lastCaptureTime = 0;
    paused = false;
    remainingSeconds = 0;

    // Apagar el motor al detener captura
    motor.setPowerPct(0);
    motorPowerPct = 0.0;
    errorIntegral = 0.0;
    lastError = 0.0;

    Serial.println("Capture stopped");
  }

  // Formatea tiempo transcurrido como HH:MM
  void getElapsedTimeString(char *buffer, size_t bufferSize)
  {
    // Formatear tiempo transcurrido (segundos) como HH:MM
    unsigned long secs = tiempoTranscurrido;
    unsigned long hours = secs / 3600UL;
    unsigned long minutes = (secs % 3600UL) / 60UL;
    snprintf(buffer, bufferSize, "%02lu:%02lu", hours, minutes);
  }

  // Helper para obtener % de batería para display
  int getBatteryPercentage()
  {
    int rawLevel = battery.getLevel();
    return map(rawLevel, 0, 255, 0, 100);
  }
} AppContext;

#endif