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
#include "../../Config/Legacy.h"
#include "LogSchema.h"
#include "LogIndexService.h"

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
        bool finalRecord = false;
    };

    QueueHandle_t logQueue = nullptr;
    TaskHandle_t logTaskHandle = nullptr;
    bool sdInitAttempted = false;
    bool sdMissingLogReported = false;
    LogIndexService logIndex;

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

    bool initSD();
    void markSdFailed();
    void startLogTask();
    bool hasLogQueue() const { return logQueue != nullptr; }
    bool enqueueLogRecord(const LogRecord &record, uint32_t sessionStartUnix,
                          bool includeState, bool includePlantower,
                          bool includeAnemometer, bool includeNtc,
                          bool kickActive = false, bool finalRecord = false);
    bool logsIdle() const;
    LogIndexService::ReconcileSummary indexSummary() const { return logIndex.reconciliationSummary(); }
    bool logData(const LogRecord &record, uint32_t sessionStartUnix,
                 bool includeState, bool includePlantower,
                 bool includeAnemometer, bool includeNtc,
                 const char *stateText = "Capturando",
                 String *writtenBasename = nullptr);
    bool removeLogAndIndex(const char *logFile);
};

inline void LogService::logTaskWorker(void *arg)
{
    LogService *service = static_cast<LogService *>(arg);
    LogJob job;
    while (true)
    {
        if (xQueueReceive(service->logQueue, &job, portMAX_DELAY) == pdTRUE)
        {
            service->logActive = true;
            String writtenBasename;
            const bool wroteLog = service->logData(
                job.record, job.sessionStartUnix,
                job.includeState, job.includePlantower,
                job.includeAnemometer, job.includeNtc,
                job.finalRecord ? "Finalizado" : (job.kickActive ? "Arrancando" : "Capturando"),
                &writtenBasename);
            if (wroteLog && job.finalRecord)
            {
                SPIBus::Guard spiGuard;
                LogIndexService::Entry entry;
                if (!service->logIndex.makeEntryFromCapture(
                        writtenBasename.c_str(), job.sessionStartUnix,
                        job.record.timestampUnix, job.record.capturedVolume, entry) ||
                    !service->logIndex.upsert(entry))
                {
                    LOG_LN("No se pudo actualizar el indice maestro de capturas");
                    service->markSdFailed();
                }
            }
            service->logActive = false;
            service->uploadPending = service->logQueue != nullptr &&
                                     uxQueueMessagesWaiting(service->logQueue) > 0;
        }
    }
}

inline bool LogService::initSD()
{
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

    sdStatus = SD_WRITING;
    if (!logIndex.reconcile(logsDir))
    {
        LOG_LN("No se pudo reconciliar el indice maestro de capturas");
        markSdFailed();
        return false;
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

inline void LogService::startLogTask()
{
    if (logQueue == nullptr)
        logQueue = xQueueCreate(4, sizeof(LogJob));
    if (logTaskHandle == nullptr && logQueue != nullptr)
        // Prio 1 (servicio de fondo): escritura SD asincrona; cede CPU a sensores y bus RS485.
        // Stack 8192: operaciones de archivo SD + formateo de log en buffer temporal.
        xTaskCreatePinnedToCore(logTaskWorker, "EoloLogTask", 8192, this, 1, &logTaskHandle, 0);
}

inline bool LogService::enqueueLogRecord(const LogRecord &record,
                                         uint32_t sessionStartUnix,
                                         bool includeState,
                                         bool includePlantower,
                                         bool includeAnemometer,
                                         bool includeNtc,
                                         bool kickActive,
                                         bool finalRecord)
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
    job.finalRecord = finalRecord;

    // Set this before publishing the job.  It closes the cross-core window in
    // which the worker can dequeue immediately but has not set logActive yet.
    uploadPending = true;

    // A closing sample is mandatory: wait for one queue slot instead of
    // silently losing the capture terminus when several periodic samples are
    // still being flushed.
    if (xQueueSend(logQueue, &job, finalRecord ? portMAX_DELAY : 0) == pdTRUE)
    {
        return true;
    }

    uploadPending = uxQueueMessagesWaiting(logQueue) > 0 || logActive.load();
    LOG_LN("Cola de log llena; se omite log para no bloquear UI");
    return false;
}

inline bool LogService::logsIdle() const
{
    return !uploadPending.load() && !logActive.load() &&
           (logQueue == nullptr || uxQueueMessagesWaiting(logQueue) == 0);
}

inline bool LogService::logData(const LogRecord &record,
                                uint32_t sessionStartUnix,
                                bool includeState,
                                bool includePlantower,
                                bool includeAnemometer,
                                bool includeNtc,
                                const char *stateText,
                                String *writtenBasename)
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

    if (writtenBasename != nullptr)
    {
        int slash = filename.lastIndexOf('/');
        *writtenBasename = slash >= 0 ? filename.substring(slash + 1) : filename;
    }

    if (EoloDebug::verboseLogsEnabled())
    {
        LOG_LN("Archivo de log escrito!");
    }
    sdStatus = SD_OK;
    return true;
}

inline bool LogService::removeLogAndIndex(const char *logFile)
{
    if (!isSdReady || logFile == nullptr)
        return false;
    String name(logFile);
    if (!name.startsWith("log_") || !name.endsWith(".csv") ||
        name.indexOf('/') >= 0 || name.indexOf('\\') >= 0)
        return false;

    SPIBus::Guard spiGuard;
    String path = String(logsDir) + "/" + name;
    if (!SD.exists(path.c_str()) || !SD.remove(path.c_str()))
        return false;
    if (!logIndex.remove(name.c_str()))
    {
        LOG_LN("Log eliminado, pero no se pudo regenerar el indice maestro: " + name);
        markSdFailed();
        return false;
    }
    return true;
}

#endif // EOLO_LOG_SERVICE_H
