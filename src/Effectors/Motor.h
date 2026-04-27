#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>
#include "../Config.h"

#define PWM_RESOLUTION 11
#define MAX_PWM ((1 << PWM_RESOLUTION) - 1)

class MotorManager
{
public:
  static constexpr int motors[2] = {25, 27};
  static constexpr int ledcChannels[2] = {0, 1};
  static const int motorCount = sizeof(motors) / sizeof(motors[0]);
  static const int freq = 20000;
  static const int resolution = 11;
  int pwmValues[sizeof(motors) / sizeof(motors[0])];
  bool isReady = false;

private:
  static int constrainPwm(int pwm)
  {
    if (pwm < 0)
      return 0;
    if (pwm > MAX_PWM)
      return MAX_PWM;
    return pwm;
  }

  void writeMotorPwm(int motorIdx, int pwm)
  {
    if (motorIdx < 0 || motorIdx >= motorCount)
      return;

    pwm = constrainPwm(pwm);
    if (pwmValues[motorIdx] != pwm)
    {
      LOG_LN("Motor " + String(motorIdx) + " a " + String(pwm));
    }
    pwmValues[motorIdx] = pwm;
    ledcWrite(ledcChannels[motorIdx], pwmValues[motorIdx]);
  }

public:
  MotorManager()
  {
    for (int i = 0; i < motorCount; i++)
      pwmValues[i] = 0;
  }

  ~MotorManager() {}

  void begin()
  {
    if (isReady)
    {
      LOG_LN("Motores ya inicializados, skipping...");
      return;
    }

    for (int i = 0; i < motorCount; i++)
    {
      pinMode(motors[i], OUTPUT);
      digitalWrite(motors[i], LOW);
      ledcSetup(ledcChannels[i], freq, resolution);
      ledcAttachPin(motors[i], ledcChannels[i]);
      ledcWrite(ledcChannels[i], 0);
    }
#if CHECK_SENSORS
    testMotors();
#endif

    isReady = true;
  }

  void testMotors()
  {
    for (int i = 0; i <= 100; i += 10)
    {
      Serial.print("Prueba de motores: ");
      Serial.print(i);
      LOG_LN("%");
      setPowerPct(i);
      delay(500);
    }
    setPowerPct(0);
  }

  // Establecer PWM de un motor específico (0 a MAX_PWM)
  void setMotorPwm(int motorIdx, int pwm)
  {
    writeMotorPwm(motorIdx, pwm);
  }

  // Establecer TODOS los motores al mismo PWM
  void setPwm(int pwm)
  {
    for (int i = 0; i < motorCount; i++)
      writeMotorPwm(i, pwm);
  }

  // Establecer TODOS los motores al mismo porcentaje (0-100%)
  void setPowerPct(float powerPct)
  {
    if (powerPct > 100.0f)
      powerPct = 100.0f;
    else if (powerPct < 0.0f)
      powerPct = 0.0f;
    int pwm = static_cast<int>((MAX_PWM * powerPct) / 100.0f);
    setPwm(pwm);
  }

  void setPowerPct(int powerPct)
  {
    setPowerPct(static_cast<float>(powerPct));
  }

  // Porcentaje promedio de potencia de todos los motores
  int getPowerPct()
  {
    int totalPWM = 0;
    for (int i = 0; i < motorCount; i++)
      totalPWM += pwmValues[i];
    return (totalPWM * 100) / (motorCount * MAX_PWM);
  }
};

constexpr int MotorManager::motors[2];
constexpr int MotorManager::ledcChannels[2];

#endif
