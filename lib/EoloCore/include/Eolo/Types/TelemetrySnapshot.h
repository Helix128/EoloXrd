#ifndef EOLO_TYPES_TELEMETRY_SNAPSHOT_H
#define EOLO_TYPES_TELEMETRY_SNAPSHOT_H

#include <stdint.h>

#include <Eolo/Types/AnemometerData.h>
#include <Eolo/Types/BME280Data.h>
#include <Eolo/Types/FlowData.h>
#include <Eolo/Types/PlantowerData.h>

// Datos de una publicación remota. Los valores faltantes conservan el
// contrato histórico del payload (por ejemplo -1 para sensores ausentes),
// mientras que los DTO de sensores mantienen su bandera valid.
struct TelemetrySnapshot
{
    uint32_t timestampUnix = 0;
    float targetFlow = 0.0f;
    float capturedVolume = 0.0f;
    FlowData flow;
    BME280Data environment;
    PlantowerData plantower;
    AnemometerData anemometer;
    float batteryVoltage = -1.0f;

    // GPS todavía no es parte del hardware EOLO actual. Se dejan explícitos
    // para que el serializador no tenga que inferir valores faltantes.
    float latitude = -1.0f;
    float longitude = -1.0f;
    float signalStrength = -1.0f;
    float gpsSpeed = -1.0f;
    float satellites = -1.0f;
};

#endif
