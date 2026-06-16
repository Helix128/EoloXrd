#ifndef EOLO_LOG_SERVICE_H
#define EOLO_LOG_SERVICE_H

#include <Arduino.h>
#include <SD.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <Eolo/Types/LogTypes.h>

struct Context;

class LogService
{
    struct LogJob
    {
        uint32_t queuedAt;
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
    bool uploadPending = false;
    volatile bool logActive = false;

    bool initSD(Context &ctx);
    void markSdFailed();
    void startLogTask(Context &ctx);
    void enqueueLogData(Context &ctx);
    bool logsIdle() const;
    void processCaptureSample(Context &ctx);
    bool logData(Context &ctx);
};

#endif // EOLO_LOG_SERVICE_H


#if defined(CONTEXT_CLASS_DEFINED) && !defined(EOLO_LOG_SERVICE_IMPL_DONE)
#define EOLO_LOG_SERVICE_IMPL_DONE

#include "../../Config.h"
#include "../Context.h"
#include "LogSchema.h"

inline void LogService::logTaskWorker(void *arg)
{
    Context *ctx = static_cast<Context *>(arg);
    LogJob job;
    while (true)
    {
        if (xQueueReceive(ctx->logging.logQueue, &job, portMAX_DELAY) == pdTRUE)
        {
            (void)job;
            ctx->logging.logActive = true;
            ctx->logging.processCaptureSample(*ctx);
            ctx->logging.logActive = false;
            ctx->logging.uploadPending = ctx->logging.logQueue != nullptr && uxQueueMessagesWaiting(ctx->logging.logQueue) > 0;
            ctx->markUiDirty();
        }
    }
}

inline bool LogService::initSD(Context &ctx)
{
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
    if (logQueue == nullptr)
        logQueue = xQueueCreate(4, sizeof(LogJob));
    if (logTaskHandle == nullptr && logQueue != nullptr)
        xTaskCreatePinnedToCore(logTaskWorker, "EoloLogTask", 8192, &ctx, 1, &logTaskHandle, 0);
}

inline void LogService::enqueueLogData(Context &ctx)
{
    if (logQueue == nullptr)
        startLogTask(ctx);

    LogJob job = {millis()};
    if (logQueue != nullptr && xQueueSend(logQueue, &job, 0) == pdTRUE)
    {
        uploadPending = true;
        ctx.markUiDirty();
        return;
    }

    LOG_LN("Cola de log llena o no disponible; se omite log para no bloquear UI");
}

inline bool LogService::logsIdle() const
{
    return !logActive && (logQueue == nullptr || uxQueueMessagesWaiting(logQueue) == 0);
}

inline void LogService::processCaptureSample(Context &ctx)
{
    ctx.saveSession();
    logData(ctx);
#ifdef FEATURE_MODEM
    ctx.uploadData();
#endif
}

inline bool LogService::logData(Context &ctx)
{
    PROFILE_SCOPE("context.log");

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

    String dateStr = ctx.session.startDate.timestamp();
    dateStr.replace(":", "_");
    dateStr.replace("-", "_");

    String header = LogSchema::header(ctx);
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

    LogSchema::writeRow(file, ctx);

    if (EoloDebug::verboseLogsEnabled())
    {
        LOG_LN("Datos registrados en SD con schema: " + header);
    }
    file.close();

    if (EoloDebug::verboseLogsEnabled())
    {
        LOG_LN("Archivo de log escrito!");
        LOG_LN("Log completado con éxito.");
    }
    sdStatus = SD_OK;
    return true;
}

#endif
