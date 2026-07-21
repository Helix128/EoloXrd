#ifndef EOLO_UPLOAD_SERVICE_H
#define EOLO_UPLOAD_SERVICE_H

#include <atomic>
#include <Eolo/Types/TelemetrySnapshot.h>

struct Context;
class SensorAPI;

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
    // Written by upload task/call path, read by main loop/drone controller.
    std::atomic_bool uploadActive{false};
    bool collectSnapshot(Context &ctx, TelemetrySnapshot &snapshot);
    // Publicador desacoplado: recibe el DTO completo y el adaptador de API,
    // sin volver a recorrer Context ni consultar sensores.
    void publishSnapshot(SensorAPI &api, const TelemetrySnapshot &snapshot);
    void uploadData(Context &ctx);
};

#endif // EOLO_UPLOAD_SERVICE_H


#if defined(CONTEXT_CLASS_DEFINED) && !defined(EOLO_UPLOAD_SERVICE_IMPL_DONE)
#define EOLO_UPLOAD_SERVICE_IMPL_DONE

#include "Context.h"
#include "../Config/Legacy.h"

inline void UploadService::uploadData(Context &ctx)
{
#ifdef FEATURE_MODEM
    PROFILE_SCOPE("context.upload");
    uploadActive = true;
    ctx.markUiDirty();

    TelemetrySnapshot snapshot;
    collectSnapshot(ctx, snapshot);
    publishSnapshot(ctx.components.api, snapshot);
    uploadActive = false;
    ctx.markUiDirty();
#else
    (void)ctx;
#endif
}

inline void UploadService::publishSnapshot(SensorAPI &api, const TelemetrySnapshot &snapshot)
{
#ifdef FEATURE_MODEM
    api.begin();
#ifdef FEATURE_ANEMOMETER
    api.addData(ApiId::WindSensor, ApiId::WindSpeed,
                snapshot.anemometer.valid ? snapshot.anemometer.speed : -1);
    api.addData(ApiId::WindSensor, ApiId::WindDirection,
                snapshot.anemometer.valid ? snapshot.anemometer.direction : -1);
#else
    api.addData(ApiId::WindSensor, ApiId::WindSpeed, -1);
    api.addData(ApiId::WindSensor, ApiId::WindDirection, -1);
#endif
    // Estos dos campos históricos no representan datos Plantower y deben
    // conservar sus sentinelas para no cambiar el payload remoto.
    api.addData(ApiId::PlantowerSensor, ApiId::Temperature, -69);
    api.addData(ApiId::PlantowerSensor, ApiId::Humidity, -420);
#ifdef FEATURE_PLANTOWER
    api.addData(ApiId::PlantowerSensor, ApiId::Pm1,
                snapshot.plantower.valid ? snapshot.plantower.pm1_0 : -1);
    api.addData(ApiId::PlantowerSensor, ApiId::Pm25,
                snapshot.plantower.valid ? snapshot.plantower.pm2_5 : -1);
    api.addData(ApiId::PlantowerSensor, ApiId::Pm10,
                snapshot.plantower.valid ? snapshot.plantower.pm10_0 : -1);
#else
    api.addData(ApiId::PlantowerSensor, ApiId::Pm1, -1);
    api.addData(ApiId::PlantowerSensor, ApiId::Pm25, -1);
    api.addData(ApiId::PlantowerSensor, ApiId::Pm10, -1);
#endif
    api.addData(ApiId::BmeSensor, ApiId::Temperature, snapshot.environment.temperature);
    api.addData(ApiId::BmeSensor, ApiId::Humidity, snapshot.environment.humidity);
    api.addData(ApiId::BmeSensor, ApiId::Pressure, snapshot.environment.pressure);
    api.addData(ApiId::FlowSensor, ApiId::TargetFlow, snapshot.targetFlow);
    api.addData(ApiId::FlowSensor, ApiId::CapturedVolume, snapshot.capturedVolume);
    api.addData(ApiId::FlowSensor, ApiId::MeasuredFlow,
                snapshot.flow.valid ? snapshot.flow.flow : -1.0f);
    api.addData(ApiId::AmbientTemperatureSensor, ApiId::Temperature,
                snapshot.environment.temperature);
    api.addData(ApiId::BatterySensor, ApiId::BatteryVoltage, snapshot.batteryVoltage);
    api.addData(ApiId::GpsSensor, ApiId::Latitude, snapshot.latitude);
    api.addData(ApiId::GpsSensor, ApiId::Longitude, snapshot.longitude);
    api.addData(ApiId::GpsSensor, ApiId::SignalStrength, snapshot.signalStrength);
    api.addData(ApiId::GpsSensor, ApiId::GpsSpeed, snapshot.gpsSpeed);
    api.addData(ApiId::GpsSensor, ApiId::Satellites, snapshot.satellites);
    api.send();
#else
    (void)api;
    (void)snapshot;
#endif
}

inline bool UploadService::collectSnapshot(Context &ctx, TelemetrySnapshot &snapshot)
{
    snapshot = TelemetrySnapshot();
    snapshot.timestampUnix = ctx.getUnixTime();
    snapshot.targetFlow = ctx.session.targetFlow;
    snapshot.capturedVolume = ctx.session.capturedVolume;
    BME280Data bmeData;
    bool bmeValid = ctx.components.bme.getData(bmeData);
    snapshot.environment.temperature = bmeValid ? bmeData.temperature : -1.0f;
    snapshot.environment.humidity = bmeValid ? bmeData.humidity : -1.0f;
    snapshot.environment.pressure = bmeValid ? bmeData.pressure : -1.0f;
    snapshot.environment.valid = bmeValid;
    snapshot.batteryVoltage = ctx.components.battery.getVoltage();

#ifdef FEATURE_ANEMOMETER
    (void)ctx.components.anemometer.getData(snapshot.anemometer);
#endif
#ifdef FEATURE_PLANTOWER
    (void)ctx.components.plantower.getData(snapshot.plantower);
#endif
    (void)ctx.components.flowSensor.getData(snapshot.flow);
    return snapshot.flow.valid || snapshot.environment.valid || snapshot.anemometer.valid || snapshot.plantower.valid;
}

#endif
