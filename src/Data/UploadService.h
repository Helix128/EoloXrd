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

#ifndef EOLO_UPLOADSERVICEIMPL_H
#define EOLO_UPLOADSERVICEIMPL_H

inline void UploadService::uploadData(Context &ctx)
{
#ifdef FEATURE_MODEM
    PROFILE_SCOPE("context.upload");
    uploadActive = true;
    ctx.markUiDirty();
    ctx.components.api.begin();
#ifdef FEATURE_ANEMOMETER
    AnemometerData anemoData;
    (void)ctx.components.anemometer.getData(anemoData);
    ctx.components.api.addData(ApiId::WindSensor, ApiId::WindSpeed, anemoData.speed);
    ctx.components.api.addData(ApiId::WindSensor, ApiId::WindDirection, anemoData.direction);
#else
    ctx.components.api.addData(ApiId::WindSensor, ApiId::WindSpeed, -1);
    ctx.components.api.addData(ApiId::WindSensor, ApiId::WindDirection, -1);
#endif
    ctx.components.api.addData(ApiId::PlantowerSensor, ApiId::Temperature, -69);
    ctx.components.api.addData(ApiId::PlantowerSensor, ApiId::Humidity, -420);
#ifdef FEATURE_PLANTOWER
    PlantowerData ptowerData;
    (void)ctx.components.plantower.getData(ptowerData);
    ctx.components.api.addData(ApiId::PlantowerSensor, ApiId::Pm1, ptowerData.pm1_0);
    ctx.components.api.addData(ApiId::PlantowerSensor, ApiId::Pm25, ptowerData.pm2_5);
    ctx.components.api.addData(ApiId::PlantowerSensor, ApiId::Pm10, ptowerData.pm10_0);
#else
    ctx.components.api.addData(ApiId::PlantowerSensor, ApiId::Pm1, -1);
    ctx.components.api.addData(ApiId::PlantowerSensor, ApiId::Pm25, -1);
    ctx.components.api.addData(ApiId::PlantowerSensor, ApiId::Pm10, -1);
#endif
    ctx.components.api.addData(ApiId::BmeSensor, ApiId::Temperature, ctx.components.bme.temperature);
    ctx.components.api.addData(ApiId::BmeSensor, ApiId::Humidity, ctx.components.bme.humidity);
    ctx.components.api.addData(ApiId::BmeSensor, ApiId::Pressure, ctx.components.bme.pressure);
    ctx.components.api.addData(ApiId::FlowSensor, ApiId::TargetFlow, ctx.session.targetFlow);
    ctx.components.api.addData(ApiId::FlowSensor, ApiId::CapturedVolume, ctx.session.capturedVolume);
    FlowData flowData;
    if (!ctx.components.flowSensor.getData(flowData) || !flowData.valid)
        flowData.flow = -1.0;
    ctx.components.api.addData(ApiId::FlowSensor, ApiId::MeasuredFlow, flowData.flow);
    ctx.components.api.addData(ApiId::AmbientTemperatureSensor, ApiId::Temperature, ctx.components.bme.temperature);
    ctx.components.api.addData(ApiId::BatterySensor, ApiId::BatteryVoltage, ctx.components.battery.getVoltage());
    ctx.components.api.addData(ApiId::GpsSensor, ApiId::Latitude, -1);
    ctx.components.api.addData(ApiId::GpsSensor, ApiId::Longitude, -1);
    ctx.components.api.addData(ApiId::GpsSensor, ApiId::SignalStrength, -1);
    ctx.components.api.addData(ApiId::GpsSensor, ApiId::GpsSpeed, -1);
    ctx.components.api.addData(ApiId::GpsSensor, ApiId::Satellites, -1);
    ctx.components.api.send();
    uploadActive = false;
    ctx.markUiDirty();
#else
    (void)ctx;
#endif
}

#endif

#endif
