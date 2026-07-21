#ifndef NTC_SENSOR_H
#define NTC_SENSOR_H

#include <Arduino.h>
#include "../Config/Legacy.h"
#include <Eolo/Core/Sensors/NtcThermistor.h>
#include <Eolo/Types/NTCData.h>

class NTC
{
public:
  static constexpr float DefaultFixedResistance = NtcThermistor::DefaultFixedResistance;
  static constexpr float DefaultR25 = NtcThermistor::DefaultR25;
  static constexpr float DefaultBeta = NtcThermistor::DefaultBeta;
  static constexpr float T25Kelvin = NtcThermistor::T25Kelvin;
  static constexpr float AdcVref = NtcThermistor::AdcVref;
  static constexpr int AdcMax = NtcThermistor::AdcMax;

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
    return NtcThermistor::rawToVoltage(raw);
  }

  static bool computeTemperature(int raw, float &temperature, float &resistance,
                                 float fixedResistance = DefaultFixedResistance,
                                 float r25 = DefaultR25,
                                 float beta = DefaultBeta)
  {
    return NtcThermistor::computeTemperature(raw, temperature, resistance, fixedResistance, r25, beta);
  }

  static bool motorOverheatLatched(bool current,
                                   bool valid,
                                   float temperature,
                                   float highThreshold = NTC_MOTOR_OVERHEAT_HIGH_C,
                                   float lowThreshold = NTC_MOTOR_OVERHEAT_LOW_C)
  {
    return NtcThermistor::motorOverheatLatched(current, valid, temperature, highThreshold, lowThreshold);
  }

private:
  int _pin = NTC_PIN;
  NTCData _data;
};

#endif
