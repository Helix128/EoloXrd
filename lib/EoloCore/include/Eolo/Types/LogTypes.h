#ifndef EOLO_TYPES_LOG_TYPES_H
#define EOLO_TYPES_LOG_TYPES_H

#include <stdint.h>

#include <Eolo/Types/AnemometerData.h>
#include <Eolo/Types/BME280Data.h>
#include <Eolo/Types/FlowData.h>
#include <Eolo/Types/NTCData.h>
#include <Eolo/Types/PlantowerData.h>

enum SDStatus
{
    SD_OK,
    SD_WRITING,
    SD_MISSING,
    SD_ERROR
};

// Snapshot immutable del estado que se escribe en una fila de log.  El tipo
// no conoce SD, Arduino ni Context; el adaptador de aplicación decide cómo
// serializarlo (CSV, consola o una cola de escritura).
struct LogRecord
{
    uint32_t timestampUnix = 0;
    uint32_t elapsedSeconds = 0;
    float targetFlow = 0.0f;
    float capturedVolume = 0.0f;
    FlowData flow;
    BME280Data environment;
    PlantowerData plantower;
    AnemometerData anemometer;
    NTCData ntc;
    float batteryVoltage = -1.0f;
    float batteryPercent = -1.0f;
    bool sessionActive = false;
};

#endif
