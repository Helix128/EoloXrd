#ifndef EOLO_LOG_SERVICE_H
#define EOLO_LOG_SERVICE_H

#include <Arduino.h>
#include <SD.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

struct Context;

enum SDStatus
{
    SD_OK,
    SD_WRITING,
    SD_MISSING,
    SD_ERROR
};

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

#endif
