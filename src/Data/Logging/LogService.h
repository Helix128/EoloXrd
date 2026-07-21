#ifndef EOLO_LOG_SERVICE_H
#define EOLO_LOG_SERVICE_H

#include <Arduino.h>
#include <SD.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <Eolo/Types/LogTypes.h>
#include <atomic>
#include "../../Board/SPIBus.h"

struct Context;

class LogService
{
    struct LogJob
    {
        // La muestra se captura en el ciclo de aplicación y viaja completa a
        // la tarea de SD. Así el worker no vuelve a consultar sensores ni
        // necesita conocer Context.
        LogRecord record;
        uint32_t sessionStartUnix = 0;
        bool includeState = false;
        bool includePlantower = false;
        bool includeAnemometer = false;
        bool includeNtc = false;
        bool kickActive = false;
    };

    QueueHandle_t logQueue = nullptr;
    TaskHandle_t logTaskHandle = nullptr;
    bool sdInitAttempted = false;
    bool sdMissingLogReported = false;

    static void logTaskWorker(void *arg);

public:
    const char *eoloDir = "/EOLO";
    const char *logsDir = "/EOLO/logs";
    SDStatus sdStatus = SD_OK;
    bool isSdReady = false;
    // Written by log task (core 0), read by main loop/drone controller (core 1).
    std::atomic_bool uploadPending{false};
    // Written by log task (core 0), read by main loop/drone controller (core 1).
    std::atomic_bool logActive{false};

    bool initSD(Context &ctx);
    void markSdFailed();
    void startLogTask(Context &ctx);
    void enqueueLogData(Context &ctx);
    bool enqueueLogRecord(const LogRecord &record, uint32_t sessionStartUnix,
                          bool includeState, bool includePlantower,
                          bool includeAnemometer, bool includeNtc,
                          bool kickActive = false);
    bool logsIdle() const;
    void processCaptureSample(Context &ctx);
    bool logData(Context &ctx);
    bool logData(Context &ctx, const LogRecord &record);
    bool logData(const LogRecord &record, uint32_t sessionStartUnix,
                 bool includeState, bool includePlantower,
                 bool includeAnemometer, bool includeNtc,
                 const char *stateText = "Capturando");
    bool buildLogRecord(Context &ctx, LogRecord &record);
};

#endif // EOLO_LOG_SERVICE_H


#if defined(CONTEXT_CLASS_DEFINED) && !defined(EOLO_LOG_SERVICE_IMPL_DONE)
#define EOLO_LOG_SERVICE_IMPL_DONE

#include "../../Config/Legacy.h"
#include "../Context.h"
#include "LogSchema.h"

inline void LogService::logTaskWorker(void *arg)
{
    LogService *service = static_cast<LogService *>(arg);
    LogJob job;
    while (true)
    {
        if (xQueueReceive(service->logQueue, &job, portMAX_DELAY) == pdTRUE)
        {
            service->logActive = true;
            service->logData(job.record, job.sessionStartUnix,
                             job.includeState, job.includePlantower,
                             job.includeAnemometer, job.includeNtc,
                             job.kickActive ? "Arrancando" : "Capturando");
            service->logActive = false;
            service->uploadPending = service->logQueue != nullptr &&
                                     uxQueueMessagesWaiting(service->logQueue) > 0;
        }
    }
}

inline bool LogService::initSD(Context &ctx)
{
    (void)ctx;
    SPIBus::Guard spiGuard;
    if (sdInitAttempted)
        return isSdReady;
    sdInitAttempted = true;

    if (isSdReady)
        return true;

    if (!SD.begin(SD_CS_PIN))
    {
        LOG_LN("Fallo al inicializar SD");
        sdStatus = SD_ERROR;
        isSdReady = false;
        return false;
    }

    sdcard_type_t sdType = SD.cardType();
    if (sdType == CARD_NONE)
    {
        LOG_LN("No se detectó tarjeta SD");
        sdStatus = SD_MISSING;
        isSdReady = false;
        return false;
    }
    isSdReady = true;
    sdStatus = SD_OK;
    sdMissingLogReported = false;

    LOG_OUT("Tipo de tarjeta SD: ");
    switch (sdType)
    {
    case CARD_MMC:
        LOG_OUT_LN("MMC");
        break;
    case CARD_SD:
        LOG_OUT_LN("SDSC");
        break;
    case CARD_SDHC:
        LOG_OUT_LN("SDHC");
        break;
    case CARD_UNKNOWN:
    default:
        LOG_OUT_LN("Desconocido");
        break;
    }

    uint64_t sdCardSize = SD.cardSize();
    LOG_OUT("Tamaño de la tarjeta SD: ");
    LOG_OUT(sdCardSize / (1024 * 1024));
    LOG_OUT_LN(" MB");

    uint64_t totalBytes = SD.totalBytes();
    uint64_t usedBytes = SD.usedBytes();
    LOG_OUT("Espacio total: ");
    LOG_OUT(totalBytes / (1024 * 1024));
    LOG_OUT_LN(" MB");
    LOG_OUT("Espacio usado: ");
    LOG_OUT(usedBytes / (1024 * 1024));
    LOG_OUT(" MB (");
    LOG_OUT(totalBytes > 0 ? (usedBytes * 100) / totalBytes : 0);
    LOG_OUT_LN("%)");

    if (!SD.exists(eoloDir))
    {
        sdStatus = SD_WRITING;
        if (!SD.mkdir(eoloDir))
        {
            LOG_LN("No se pudo crear directorio /EOLO en SD");
            markSdFailed();
            return false;
        }
        LOG_LN("Directorio /EOLO creado en SD");
        sdStatus = SD_OK;
    }
    else
    {
        LOG_LN("Directorio /EOLO ya existe en SD");
    }

    if (!SD.exists(logsDir))
    {
        sdStatus = SD_WRITING;
        if (!SD.mkdir(logsDir))
        {
            LOG_LN("No se pudo crear directorio /EOLO/logs en SD");
            markSdFailed();
            return false;
        }
        LOG_LN("Directorio /EOLO/logs creado en SD");
        sdStatus = SD_OK;
    }
    else
    {
        LOG_LN("Directorio /EOLO/logs ya existe en SD");
    }

    LOG_LN("SD inicializada");
    sdStatus = SD_OK;
    return true;
}

inline void LogService::markSdFailed()
{
    sdStatus = SD_ERROR;
    isSdReady = false;
}

inline void LogService::startLogTask(Context &ctx)
{
    (void)ctx;
    if (logQueue == nullptr)
        logQueue = xQueueCreate(4, sizeof(LogJob));
    if (logTaskHandle == nullptr && logQueue != nullptr)
        // Prio 1 (servicio de fondo): escritura SD asincrona; cede CPU a sensores y bus RS485.
        // Stack 8192: operaciones de archivo SD + formateo de log en buffer temporal.
        xTaskCreatePinnedToCore(logTaskWorker, "EoloLogTask", 8192, this, 1, &logTaskHandle, 0);
}

inline void LogService::enqueueLogData(Context &ctx)
{
    if (logQueue == nullptr)
        startLogTask(ctx);

    LogRecord record;
    buildLogRecord(ctx, record);
    ctx.saveSession();

    enqueueLogRecord(record, ctx.session.startUnix,
                     LogSchema::stateColumnEnabled(ctx),
                     LogSchema::plantowerEnabled(ctx),
                     LogSchema::anemometerEnabled(ctx),
                     LogSchema::ntcEnabled(ctx),
#ifdef FEATURE_FLOW_PID
                     ctx.motorCapture.getPidStatus().kickActive
#else
                     false
#endif
    );
}

inline bool LogService::enqueueLogRecord(const LogRecord &record,
                                         uint32_t sessionStartUnix,
                                         bool includeState,
                                         bool includePlantower,
                                         bool includeAnemometer,
                                         bool includeNtc,
                                         bool kickActive)
{
    if (logQueue == nullptr)
    {
        LOG_LN("Cola de log no disponible; se omite log para no bloquear UI");
        return false;
    }

    LogJob job;
    job.record = record;
    job.sessionStartUnix = sessionStartUnix;
    job.includeState = includeState;
    job.includePlantower = includePlantower;
    job.includeAnemometer = includeAnemometer;
    job.includeNtc = includeNtc;
    job.kickActive = kickActive;

    if (xQueueSend(logQueue, &job, 0) == pdTRUE)
    {
        uploadPending = true;
        return true;
    }

    LOG_LN("Cola de log llena; se omite log para no bloquear UI");
    return false;
}

inline bool LogService::logsIdle() const
{
    return !logActive.load() && (logQueue == nullptr || uxQueueMessagesWaiting(logQueue) == 0);
}

inline void LogService::processCaptureSample(Context &ctx)
{
    enqueueLogData(ctx);
#ifdef FEATURE_MODEM
    ctx.uploadData();
#endif
}

inline bool LogService::logData(Context &ctx)
{
    LogRecord record;
    buildLogRecord(ctx, record);
    return logData(ctx, record);
}

inline bool LogService::logData(Context &ctx, const LogRecord &record)
{
    return logData(record, ctx.session.startUnix,
                   LogSchema::stateColumnEnabled(ctx),
                   LogSchema::plantowerEnabled(ctx),
                   LogSchema::anemometerEnabled(ctx),
                   LogSchema::ntcEnabled(ctx),
#ifdef FEATURE_FLOW_PID
                   ctx.motorCapture.getPidStatus().kickActive ? "Arrancando" : "Capturando"
#else
                   "Capturando"
#endif
    );
}

inline bool LogService::logData(const LogRecord &record,
                                uint32_t sessionStartUnix,
                                bool includeState,
                                bool includePlantower,
                                bool includeAnemometer,
                                bool includeNtc,
                                const char *stateText)
{
    PROFILE_SCOPE("sd.log");
    SPIBus::Guard spiGuard;

    if (!isSdReady)
    {
        if (!sdMissingLogReported)
        {
            LOG_LN("SD no disponible; se omite log local para esta sesión");
            sdMissingLogReported = true;
        }
        return false;
    }

    if (EoloDebug::verboseLogsEnabled())
        LOG_LN("Iniciando log de datos en SD...");
    sdStatus = SD_WRITING;

    String dateStr = DateTime(sessionStartUnix).timestamp();
    dateStr.replace(":", "_");
    dateStr.replace("-", "_");

    String header = LogSchema::header(includeState, includePlantower,
                                      includeAnemometer, includeNtc);
    String filename = String(logsDir) + "/log_" + dateStr + ".csv";
    bool fileExists = SD.exists(filename.c_str());

    for (int schemaAttempt = 0; fileExists; ++schemaAttempt)
    {
        File existingFile = SD.open(filename.c_str(), FILE_READ);
        if (!existingFile)
        {
            markSdFailed();
            LOG_LN("No se pudo abrir el archivo para validar header");
            return false;
        }

        String existingHeader = existingFile.readStringUntil('\n');
        existingHeader.trim();
        existingFile.close();
        if (existingHeader == header)
            break;

        if (schemaAttempt >= 98)
        {
            markSdFailed();
            LOG_LN("No se encontro nombre disponible para schema CSV actual");
            return false;
        }

        filename = String(logsDir) + "/log_" + dateStr + "_schema" + String(schemaAttempt + 2) + ".csv";
        fileExists = SD.exists(filename.c_str());
    }

    if (!fileExists)
    {
        File file = SD.open(filename.c_str(), FILE_WRITE);
        if (!file)
        {
            markSdFailed();
            LOG_LN("No se pudo abrir el archivo para escribir/crear");
            return false;
        }

        file.println(header);
        file.close();

        LOG_LN("Archivo de log creado: " + filename);
    }

    LOG_LN(fileExists ? "Archivo de log ya existe: " + filename : "Archivo de log listo: " + filename);

    File file = SD.open(filename.c_str(), FILE_APPEND);
    if (!file)
    {
        markSdFailed();
        LOG_LN("No se pudo abrir el archivo para escribir");
        return false;
    }

    LogSchema::writeRow(file, record,
                        includeState,
                        includePlantower,
                        includeAnemometer,
                        includeNtc,
                        stateText);

    if (EoloDebug::verboseLogsEnabled())
    {
        LOG_LN("Datos registrados en SD con schema: " + header);
    }
    file.close();

    if (EoloDebug::verboseLogsEnabled())
    {
        LOG_LN("Archivo de log escrito!");
    }
    sdStatus = SD_OK;
    return true;
}

inline bool LogService::buildLogRecord(Context &ctx, LogRecord &record)
{
    record = LogRecord();
    record.timestampUnix = ctx.getUnixTime();
    record.elapsedSeconds = ctx.session.elapsedTime;
    record.targetFlow = ctx.session.targetFlow;
    record.capturedVolume = ctx.session.capturedVolume;
    record.sessionActive = ctx.isCapturing;

    BME280Data bmeData;
    (void)ctx.components.bme.getData(bmeData);
    record.environment = bmeData;

    (void)ctx.components.flowSensor.getData(record.flow);
#ifdef FEATURE_PLANTOWER
    (void)ctx.components.plantower.getData(record.plantower);
#endif
#ifdef FEATURE_ANEMOMETER
    (void)ctx.components.anemometer.getData(record.anemometer);
#endif
#ifdef FEATURE_NTC
    (void)ctx.components.ntc.getData(record.ntc);
#endif
    record.batteryVoltage = ctx.components.battery.getVoltage();
    record.batteryPercent = ctx.components.battery.getPct();
    return record.flow.valid || record.environment.valid || record.plantower.valid || record.anemometer.valid;
}

#endif
