#ifndef EOLO_UPLOAD_SERVICE_H
#define EOLO_UPLOAD_SERVICE_H

struct Context;

namespace ApiId
{
    constexpr int WindSensor = 748;
    constexpr int PlantowerSensor = 749;
    constexpr int BmeSensor = 750;
    constexpr int FlowSensor = 751;
    constexpr int AmbientTemperatureSensor = 752;
    constexpr int BatterySensor = 753;
    constexpr int GpsSensor = 754;

    constexpr int Temperature = 3;
    constexpr int BatteryVoltage = 4;
    constexpr int WindSpeed = 5;
    constexpr int Humidity = 6;
    constexpr int Pm1 = 7;
    constexpr int Pm25 = 8;
    constexpr int Pm10 = 9;
    constexpr int Latitude = 11;
    constexpr int Longitude = 12;
    constexpr int Pressure = 13;
    constexpr int SignalStrength = 15;
    constexpr int WindDirection = 17;
    constexpr int GpsSpeed = 45;
    constexpr int Satellites = 46;
    constexpr int TargetFlow = 48;
    constexpr int CapturedVolume = 49;
    constexpr int MeasuredFlow = 50;
}

class UploadService
{
public:
    bool uploadActive = false;
    void uploadData(Context &ctx);
};

#endif
