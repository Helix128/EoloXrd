#ifndef NTC_SENSOR_H
#define NTC_SENSOR_H

#include <Arduino.h>
#include <math.h>
#include "../Config.h"

struct NTCData
{
  int raw = 0;
  float voltage = 0.0f;
  float resistance = 0.0f;
  float temperature = -99.0f;
  bool valid = false;
};

class NTC
{
public:
  static constexpr float DefaultFixedResistance = 10000.0f;
  static constexpr float DefaultR25 = 10000.0f;
  static constexpr float DefaultBeta = 3950.0f;
  static constexpr float T25Kelvin = 298.15f;
  static constexpr float AdcVref = 3.3f;
  static constexpr int AdcMax = 4095;

  void begin(int pin = NTC_PIN)
  {
    _pin = pin;
    if (_pin >= 0)
      pinMode(_pin, INPUT);
  }

  void readData()
  {
    if (_pin < 0)
    {
      _data = NTCData();
      return;
    }

    _data.raw = analogRead(_pin);
    _data.voltage = rawToVoltage(_data.raw);
    _data.valid = computeTemperature(_data.raw, _data.temperature, _data.resistance);
    if (!_data.valid)
      _data.temperature = -99.0f;
  }

  bool getData(NTCData &output)
  {
    readData();
    output = _data;
    return output.valid;
  }

  static float rawToVoltage(int raw)
  {
    return ((float)raw * AdcVref) / (float)AdcMax;
  }

  static bool computeTemperature(int raw, float &temperature, float &resistance,
                                 float fixedResistance = DefaultFixedResistance,
                                 float r25 = DefaultR25,
                                 float beta = DefaultBeta)
  {
    if (raw <= 10 || raw >= 4085)
      return false;

    float voltage = rawToVoltage(raw);
    if (voltage <= 0.0f || voltage >= AdcVref)
      return false;

    resistance = fixedResistance * voltage / (AdcVref - voltage);
    if (resistance <= 0.0f)
      return false;

    float tempK = 1.0f / ((1.0f / T25Kelvin) + (logf(resistance / r25) / beta));
    temperature = tempK - 273.15f;
    return isfinite(temperature);
  }

  static bool motorOverheatLatched(bool current,
                                   bool valid,
                                   float temperature,
                                   float highThreshold = NTC_MOTOR_OVERHEAT_HIGH_C,
                                   float lowThreshold = NTC_MOTOR_OVERHEAT_LOW_C)
  {
    if (!valid || !isfinite(temperature))
      return current;
    if (temperature >= highThreshold)
      return true;
    if (temperature <= lowThreshold)
      return false;
    return current;
  }

private:
  int _pin = NTC_PIN;
  NTCData _data;
};

#endif
