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

  MotorManager()
  {
    for (int i = 0; i < motorCount; i++)
    {
      pwmValues[i] = 0;
    }
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

  void setPWM(int pwm)
  {
    if (pwm > motorCount * MAX_PWM)
      pwm = motorCount * MAX_PWM;
    else if (pwm < 0)
      pwm = 0;

    for (int i = 0; i < motorCount; i++)
    {
      if (pwm >= MAX_PWM)
      {
        if (pwmValues[i] != MAX_PWM)
          LOG_LN("Motor " + String(i) + " a " + String(MAX_PWM));
        pwmValues[i] = MAX_PWM;
        pwm -= MAX_PWM;
      }
      else
      {
        if (pwmValues[i] != pwm)
          LOG_LN("Motor " + String(i) + " a " + String(pwm));
        pwmValues[i] = pwm;
        pwm = 0;
      }
      ledcWrite(ledcChannels[i], pwmValues[i]);
    }
  }

  void setPowerPct(int powerPct)
  {
    if (powerPct > 100)
      powerPct = 100;
    else if (powerPct < 0)
      powerPct = 0;
    int totalMax = motorCount * MAX_PWM;
    int targetPWM = (totalMax * powerPct) / 100;
    setPWM(targetPWM);
  }

  void setPowerPct(float powerPct)
  {
    if (powerPct > 100.0f)
      powerPct = 100.0f;
    else if (powerPct < 0.0f)
      powerPct = 0.0f;
    int totalMax = motorCount * MAX_PWM;
    int targetPWM = static_cast<int>((totalMax * powerPct) / 100.0f);
    setPWM(targetPWM);
  }

  int getPowerPct()
  {
    int totalPWM = 0;
    for (int i = 0; i < motorCount; i++)
    {
      totalPWM += pwmValues[i];
    }
    int totalMax = motorCount * MAX_PWM;
    int powerPct = (totalPWM * 100) / totalMax;
    return powerPct;
  }
};

constexpr int MotorManager::motors[2];
constexpr int MotorManager::ledcChannels[2];

#endif
