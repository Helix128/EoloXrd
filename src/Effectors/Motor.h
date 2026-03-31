#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>
#include "../Config.h"


#define PWM_RESOLUTION 11
#define MAX_PWM ((1 << PWM_RESOLUTION) - 1)

class MotorManager
{
public:
  enum DistributionMode
  {
    LEGACY_GLOBAL = 0,
    AUTO_BY_FLOW,
    SMALL_ONLY,
    BIG_ONLY,
    BOTH_EXTREME
  };

  static constexpr int motors[2] = {25, 27};
  static constexpr int ledcChannels[2] = {0, 1};
  static const int motorCount = sizeof(motors) / sizeof(motors[0]);
  static const int freq = 20000;
  static const int resolution = 11;
  int pwmValues[sizeof(motors) / sizeof(motors[0])];
  bool isReady = false;

private:
  DistributionMode mode = LEGACY_GLOBAL;
  bool manualOverride = false;
  DistributionMode overrideMode = LEGACY_GLOBAL;
  DistributionMode autoResolvedMode = SMALL_ONLY;

  // Umbrales para AUTO_BY_FLOW (L/min)
  float lowFlowThreshold = 2.0f;      // <= bajo -> SMALL_ONLY
  float highFlowThreshold = 5.5f;     // >= alto -> BIG_ONLY
  float extremeFlowThreshold = 7.5f;  // >= extremo -> BOTH_EXTREME
  float hysteresis = 0.3f;
  float lastTargetFlow = 0.0f;

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

  void applyByMode(int globalPwm, DistributionMode effectiveMode)
  {
    const int totalMax = motorCount * MAX_PWM;
    if (globalPwm < 0)
      globalPwm = 0;
    if (globalPwm > totalMax)
      globalPwm = totalMax;

    switch (effectiveMode)
    {
    case SMALL_ONLY:
      writeMotorPwm(0, globalPwm);
      writeMotorPwm(1, 0);
      break;
    case BIG_ONLY:
      writeMotorPwm(0, 0);
      writeMotorPwm(1, globalPwm);
      break;
    case BOTH_EXTREME:
    {
      // Reparto uniforme con saturación por canal.
      int half = globalPwm / 2;
      int rem = globalPwm - half;
      writeMotorPwm(0, half);
      writeMotorPwm(1, rem);
      break;
    }
    case LEGACY_GLOBAL:
    default:
    {
      int pwm = globalPwm;
      for (int i = 0; i < motorCount; i++)
      {
        if (pwm >= MAX_PWM)
        {
          writeMotorPwm(i, MAX_PWM);
          pwm -= MAX_PWM;
        }
        else
        {
          writeMotorPwm(i, pwm);
          pwm = 0;
        }
      }
      break;
    }
    }
  }

  void resolveAutoMode()
  {
    // Histéresis simple para evitar flapping.
    if (autoResolvedMode == BOTH_EXTREME)
    {
      if (lastTargetFlow < (extremeFlowThreshold - hysteresis))
      {
        autoResolvedMode = BIG_ONLY;
      }
      return;
    }

    if (lastTargetFlow >= extremeFlowThreshold)
    {
      autoResolvedMode = BOTH_EXTREME;
      return;
    }

    if (autoResolvedMode == SMALL_ONLY)
    {
      if (lastTargetFlow > (highFlowThreshold + hysteresis))
      {
        autoResolvedMode = BIG_ONLY;
      }
      return;
    }

    if (autoResolvedMode == BIG_ONLY)
    {
      if (lastTargetFlow < (lowFlowThreshold - hysteresis))
      {
        autoResolvedMode = SMALL_ONLY;
      }
      return;
    }

    autoResolvedMode = (lastTargetFlow <= lowFlowThreshold) ? SMALL_ONLY : BIG_ONLY;
  }

  DistributionMode getEffectiveMode()
  {
    if (manualOverride)
      return overrideMode;

    if (mode == AUTO_BY_FLOW)
    {
      resolveAutoMode();
      return autoResolvedMode;
    }

    return mode;
  }

public:

  MotorManager()
  {
    for (int i = 0; i < motorCount; i++)
    {
      pwmValues[i] = 0;
    }
  }

  ~MotorManager() {}

  void setDistributionMode(DistributionMode newMode)
  {
    mode = newMode;
  }

  DistributionMode getDistributionMode() const
  {
    return mode;
  }

  DistributionMode getResolvedMode() const
  {
    if (manualOverride)
      return overrideMode;
    if (mode == AUTO_BY_FLOW)
      return autoResolvedMode;
    return mode;
  }

  void setCalibrationOverrideMode(DistributionMode forcedMode)
  {
    manualOverride = true;
    overrideMode = forcedMode;
  }

  void clearCalibrationOverrideMode()
  {
    manualOverride = false;
  }

  void setAutoThresholds(float low, float high, float extreme, float hysteresisBand)
  {
    lowFlowThreshold = low;
    highFlowThreshold = high;
    extremeFlowThreshold = extreme;
    if (hysteresisBand < 0.0f)
      hysteresisBand = 0.0f;
    hysteresis = hysteresisBand;
  }

  void setTargetFlow(float flowLpm)
  {
    if (flowLpm < 0.0f)
      flowLpm = 0.0f;
    lastTargetFlow = flowLpm;
    if (mode == AUTO_BY_FLOW && !manualOverride)
    {
      resolveAutoMode();
    }
  }

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
    applyByMode(pwm, getEffectiveMode());
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
