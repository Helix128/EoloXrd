#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>
#include <math.h>
#include "../Config.h"

#if defined(FEATURE_MOTOR_PWM_8BIT)
  #define PWM_RESOLUTION 8
#else
  #define PWM_RESOLUTION 11
#endif
#define MAX_PWM ((1 << PWM_RESOLUTION) - 1)

class MotorManager
{
public:
  static constexpr int motors[MOTOR_PWM_PIN_COUNT] = {
      MOTOR_PWM_PIN_0
#if MOTOR_PWM_PIN_COUNT > 1
      , MOTOR_PWM_PIN_1
#endif
  };
  static constexpr int ledcChannels[MOTOR_PWM_PIN_COUNT] = {
      0
#if MOTOR_PWM_PIN_COUNT > 1
      , 1
#endif
  };
  static const int motorCount = sizeof(motors) / sizeof(motors[0]);
  static const int freq = 20000;
  static const int resolution = PWM_RESOLUTION;
  int pwmValues[MOTOR_PWM_PIN_COUNT];
  int targetPwmValues[MOTOR_PWM_PIN_COUNT];
  bool isReady = false;

private:
  unsigned long lastRampMs = 0;

#if defined(FEATURE_MOTOR_PWM_POWER_PIN)
  bool motorPowerPinEnabled = false;

  bool anyMotorActive() const
  {
    for (int i = 0; i < motorCount; i++)
    {
      if (pwmValues[i] > 0)
        return true;
    }
    return false;
  }

  void setMotorPowerPin(bool enabled)
  {
#if MOTOR_POWER_PIN >= 0
    if (motorPowerPinEnabled == enabled)
      return;
    digitalWrite(MOTOR_POWER_PIN, enabled ? HIGH : LOW);
    motorPowerPinEnabled = enabled;
    LOG_LN(String("Motor power pin ") + (enabled ? "ON" : "OFF"));
#else
    (void)enabled;
#endif
  }
#endif

  static int constrainPwm(int pwm)
  {
    if (pwm < 0)
      return 0;
    if (pwm > MAX_PWM)
      return MAX_PWM;
    return pwm;
  }

public:
  static int nextRampedPwm(int current, int target, int step)
  {
    current = constrainPwm(current);
    target = constrainPwm(target);
    if (step <= 0 || target <= current)
      return target;
    int next = current + step;
    return next > target ? target : next;
  }

  static int limitPwmStep(int current, int target, int maxStep)
  {
    current = constrainPwm(current);
    target = constrainPwm(target);
    if (maxStep <= 0)
      return target;
    if (target > current + maxStep)
      return current + maxStep;
    if (target < current - maxStep)
      return current - maxStep;
    return target;
  }

  static int nextClosedLoopPwm(int basePwm,
                               int currentPwm,
                               float targetFlow,
                               float measuredFlow,
                               float dtSeconds,
                               float &integral,
                               float deadbandLpm,
                               float kp,
                               float ki,
                               float integralLimit,
                               int maxStepPwm)
  {
    basePwm = constrainPwm(basePwm);
    currentPwm = constrainPwm(currentPwm);
    if (dtSeconds < 0.0f)
      dtSeconds = 0.0f;

    float error = targetFlow - measuredFlow;
    if (fabsf(error) < deadbandLpm)
      error = 0.0f;

    integral += error * dtSeconds;
    if (integralLimit >= 0.0f)
    {
      if (integral > integralLimit)
        integral = integralLimit;
      else if (integral < -integralLimit)
        integral = -integralLimit;
    }

    float correction = kp * error + ki * integral;
    int desiredPwm = constrainPwm(basePwm + static_cast<int>(correction));
    return limitPwmStep(currentPwm, desiredPwm, maxStepPwm);
  }

private:

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
#if defined(FEATURE_MOTOR_PWM_POWER_PIN)
    setMotorPowerPin(anyMotorActive());
#endif
  }

  void setMotorTargetPwm(int motorIdx, int pwm)
  {
    if (motorIdx < 0 || motorIdx >= motorCount)
      return;

    pwm = constrainPwm(pwm);
    targetPwmValues[motorIdx] = pwm;

#if MOTOR_RAMP_STEP > 0
    if (pwm > pwmValues[motorIdx])
      return;
#endif
    writeMotorPwm(motorIdx, pwm);
  }

public:
  MotorManager()
  {
    for (int i = 0; i < motorCount; i++)
    {
      pwmValues[i] = 0;
      targetPwmValues[i] = 0;
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
      pwmValues[i] = 0;
      targetPwmValues[i] = 0;
      ledcWrite(ledcChannels[i], 0);
    }
#if defined(FEATURE_MOTOR_PWM_POWER_PIN)
#if MOTOR_POWER_PIN >= 0
    pinMode(MOTOR_POWER_PIN, OUTPUT);
    digitalWrite(MOTOR_POWER_PIN, LOW);
    motorPowerPinEnabled = false;
    LOG_LN(String("Motor PWM con power pin inicializado en GPIO ") + String(MOTOR_POWER_PIN));
#else
    LOG_LN("Motor PWM con power pin seleccionado, pero MOTOR_POWER_PIN no esta configurado; usando PWM puro temporalmente.");
#endif
#else
    LOG_LN("Motor PWM puro inicializado.");
#endif
#if CHECK_SENSORS
    testMotors();
#endif

    isReady = true;
  }

  void testMotors()
  {
    for (int i = 0; i <= 100; i += 10)
    {
      LOG_OUT("Prueba de motores: ");
      LOG_OUT(i);
      LOG_LN("%");
      setPowerPct(i);
      delay(500);
    }
    setPowerPct(0);
  }

  void updateRamp()
  {
#if MOTOR_RAMP_STEP > 0
    unsigned long now = millis();
    if (now - lastRampMs < MOTOR_RAMP_INTERVAL_MS)
      return;
    lastRampMs = now;

    for (int i = 0; i < motorCount; i++)
    {
      if (pwmValues[i] >= targetPwmValues[i])
        continue;

      int next = nextRampedPwm(pwmValues[i], targetPwmValues[i], MOTOR_RAMP_STEP);
      writeMotorPwm(i, next);
    }
#endif
  }

  int getMotorPwm(int motorIdx) const
  {
    if (motorIdx < 0 || motorIdx >= motorCount)
      return 0;
    return pwmValues[motorIdx];
  }

  int getMotorTargetPwm(int motorIdx) const
  {
    if (motorIdx < 0 || motorIdx >= motorCount)
      return 0;
    return targetPwmValues[motorIdx];
  }

  // Establecer PWM de un motor específico (0 a MAX_PWM)
  void setMotorPwm(int motorIdx, int pwm)
  {
    setMotorTargetPwm(motorIdx, pwm);
  }

  void setMotorPwmImmediate(int motorIdx, int pwm)
  {
    if (motorIdx < 0 || motorIdx >= motorCount)
      return;
    pwm = constrainPwm(pwm);
    targetPwmValues[motorIdx] = pwm;
    writeMotorPwm(motorIdx, pwm);
  }

  // Establecer TODOS los motores al mismo PWM
  void setPwm(int pwm)
  {
    for (int i = 0; i < motorCount; i++)
      setMotorTargetPwm(i, pwm);
  }

  void setPwmImmediate(int pwm)
  {
    for (int i = 0; i < motorCount; i++)
      setMotorPwmImmediate(i, pwm);
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

constexpr int MotorManager::motors[MOTOR_PWM_PIN_COUNT];
constexpr int MotorManager::ledcChannels[MOTOR_PWM_PIN_COUNT];

#endif
