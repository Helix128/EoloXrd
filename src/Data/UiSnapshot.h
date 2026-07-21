#ifndef UI_SNAPSHOT_H
#define UI_SNAPSHOT_H

#include <Arduino.h>
#include <RTClib.h>
#include "../Config/Legacy.h"

struct FlowSnapshot
{
    bool enabled = false;
    bool valid = false;
    float flow = -1.0f;
    float velocity = 0.0f;
    float targetFlow = 0.0f;
    float capturedVolume = 0.0f;
};

struct EnvironmentSnapshot
{
    bool enabled = true;
    bool valid = false;
    float temperature = -1.0f;
    float humidity = -1.0f;
    float pressure = -1.0f;
    bool ntcValid = false;
    float ntcTemperature = -99.0f;
    bool motorOverheat = false;
    bool motorThermalSensorValid = false;
    float motorThermalTemperature = -99.0f;
};

struct AirQualitySnapshot
{
    bool enabled = false;
    bool valid = false;
    float pm1 = 0.0f;
    float pm25 = 0.0f;
    float pm10 = 0.0f;
};

struct PowerSnapshot
{
    bool dualBattery = false;
    bool valid = false;
    bool stale = false;
    uint32_t ageMs = 0;
    bool poweredByDc = false;
    uint8_t activeBattery = 0;
    float batteryPct = -1.0f;
    float batteryPct0 = -1.0f;
    float batteryPct1 = -1.0f;
    float batteryVoltage = -1.0f;
    float batteryVoltage0 = -1.0f;
    float batteryVoltage1 = -1.0f;
    float dcVoltage = -1.0f;
};

struct WindSnapshot
{
    bool enabled = false;
    bool valid = false;
    float speed = 0.0f;
    float windKph = 0.0f;
    int direction = 0;
};

struct SystemStatusSnapshot
{
    bool sdReady = false;
    int sdStatus = 0;
    bool modemEnabled = false;
    bool modemPowered = false;
    bool modemActive = false;
    bool modemError = false;
    bool modemSignalKnown = false;
    uint8_t modemSignalBars = 0;
    uint8_t modemSignalCsq = 99;
    bool uploadPending = false;
    bool uploadActive = false;
    bool displayOn = true;
    uint32_t unixTime = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
};

struct UiSnapshot
{
    FlowSnapshot flow;
    EnvironmentSnapshot environment;
    AirQualitySnapshot airQuality;
    PowerSnapshot power;
    WindSnapshot wind;
    SystemStatusSnapshot status;
};

#endif
