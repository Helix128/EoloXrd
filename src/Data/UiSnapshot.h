#ifndef UI_SNAPSHOT_H
#define UI_SNAPSHOT_H

#include <Arduino.h>
#include <RTClib.h>
#include "../Config.h"

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
    float temperature = 0.0f;
    float humidity = 0.0f;
    float pressure = 0.0f;
    bool ntcValid = false;
    float ntcTemperature = -99.0f;
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
    bool poweredByDc = false;
    uint8_t activeBattery = 0;
    float batteryPct = 0.0f;
    float batteryPct0 = 0.0f;
    float batteryPct1 = 0.0f;
    float batteryVoltage = 0.0f;
    float batteryVoltage0 = 0.0f;
    float batteryVoltage1 = 0.0f;
    float dcVoltage = 0.0f;
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
