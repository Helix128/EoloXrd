#ifndef RS485_MONITOR_HPP
#define RS485_MONITOR_HPP

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "../Config/Legacy.h"
#include "Profiler.h"

// Estadisticas de monitoreo del bus RS485
struct RS485Stats {
    uint32_t totalRequests = 0;
    uint32_t successfulReads = 0;
    uint32_t failedReads = 0;
    uint32_t timeoutErrors = 0;
    uint32_t crcErrors = 0;
    uint32_t slaveTimeouts[256] = {0};
    
    uint32_t maxQueueDepth = 0;
    uint32_t totalQueueOverflows = 0; // Veces que la cola se lleno
    uint32_t queueDepthWarnings = 0;

    uint32_t lastTransactionMs = 0;
    uint32_t maxTransactionMs = 0;
    
    unsigned long lastReportTime = 0;
    unsigned long lastViolationTime = 0;
    unsigned long lastLoopFrameStartMs = 0;
    
    // Para medir jitter del loop
    uint32_t loopFrameTimes[60] = {0}; // Buffer circular de 60 frames
    uint32_t loopFrameGaps[60] = {0};  // Tiempo entre inicios de frame
    uint8_t loopFrameIndex = 0;
    uint32_t loopMinTime = UINT32_MAX;
    uint32_t loopMaxTime = 0;
    uint32_t loopAvgTime = 0;
    uint32_t loopGapMinTime = UINT32_MAX;
    uint32_t loopGapMaxTime = 0;
    uint32_t loopGapAvgTime = 0;
};

// Monitor centralizado (singleton)
class RS485Monitor {
private:
    RS485Stats _stats;
    SemaphoreHandle_t _statsMutex;
    bool _verboseAlerts = false;
    const uint32_t QUEUE_WARNING_THRESHOLD = 3;
    const uint32_t LOOP_JITTER_THRESHOLD_MS = 5; // Alerta si jitter > 5ms
    const uint32_t ALERT_INTERVAL_MS = 10000;

    RS485Monitor() {
        _statsMutex = xSemaphoreCreateMutex();
        _stats.lastReportTime = millis();
        _verboseAlerts = EoloDebug::verboseLogsEnabled();
    }

public:
    static RS485Monitor& getInstance() {
        static RS485Monitor instance;
        return instance;
    }

    // Registrar una solicitud completada
    void recordRequestCompleted(bool success, uint8_t errorCode, uint8_t slaveId, uint32_t transactionMs) {
        if (xSemaphoreTake(_statsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            _stats.totalRequests++;
            _stats.lastTransactionMs = transactionMs;
            PROFILE_MARK("rs485.tx", transactionMs * 1000UL);
            if (transactionMs > _stats.maxTransactionMs) {
                _stats.maxTransactionMs = transactionMs;
            }
            if (success) {
                _stats.successfulReads++;
            } else {
                _stats.failedReads++;
                if (errorCode == 0xE2) { // ku8MBResponseTimedOut
                    _stats.timeoutErrors++;
                    _stats.slaveTimeouts[slaveId]++;
                } else if (errorCode == 0xE3) { // ku8MBInvalidCRC
                    _stats.crcErrors++;
                }
            }
            xSemaphoreGive(_statsMutex);
        }
    }

    void recordQueueOverflow() {
        if (xSemaphoreTake(_statsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            _stats.totalQueueOverflows++;
            xSemaphoreGive(_statsMutex);
        }
    }

    // Registrar profundidad de la cola
    void recordQueueDepth(uint32_t depth) {
        if (xSemaphoreTake(_statsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (depth > _stats.maxQueueDepth) {
                _stats.maxQueueDepth = depth;
            }
            if (depth >= QUEUE_WARNING_THRESHOLD) {
                _stats.queueDepthWarnings++;
                if (_verboseAlerts && millis() - _stats.lastViolationTime > ALERT_INTERVAL_MS) {
                    LOG_F("RS485 Monitor: Cola en profundidad %d (umbral: %d)\n", 
                          depth, QUEUE_WARNING_THRESHOLD);
                    _stats.lastViolationTime = millis();
                }
            }
            xSemaphoreGive(_statsMutex);
        }
    }

    // Registrar tiempo de frame del loop principal
    void recordLoopFrameTime(uint32_t frameTimeMs) {
        if (xSemaphoreTake(_statsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            unsigned long now = millis();
            uint32_t frameGapMs = 0;
            if (_stats.lastLoopFrameStartMs > 0) {
                frameGapMs = now - _stats.lastLoopFrameStartMs;
            }
            _stats.lastLoopFrameStartMs = now;

            uint8_t index = _stats.loopFrameIndex;
            _stats.loopFrameTimes[index] = frameTimeMs;
            _stats.loopFrameGaps[index] = frameGapMs;
            _stats.loopFrameIndex = (_stats.loopFrameIndex + 1) % 60;

            // Calcular estadisticas
            uint32_t frameSum = 0;
            uint32_t gapSum = 0;
            uint8_t frameSamples = 0;
            uint8_t gapSamples = 0;
            uint32_t minVal = UINT32_MAX;
            uint32_t maxVal = 0;
            uint32_t minGap = UINT32_MAX;
            uint32_t maxGap = 0;
            for (int i = 0; i < 60; i++) {
                if (_stats.loopFrameTimes[i] > 0) {
                    frameSum += _stats.loopFrameTimes[i];
                    frameSamples++;
                    if (_stats.loopFrameTimes[i] < minVal) minVal = _stats.loopFrameTimes[i];
                    if (_stats.loopFrameTimes[i] > maxVal) maxVal = _stats.loopFrameTimes[i];
                }
                if (_stats.loopFrameGaps[i] > 0) {
                    gapSum += _stats.loopFrameGaps[i];
                    gapSamples++;
                    if (_stats.loopFrameGaps[i] < minGap) minGap = _stats.loopFrameGaps[i];
                    if (_stats.loopFrameGaps[i] > maxGap) maxGap = _stats.loopFrameGaps[i];
                }
            }
            _stats.loopAvgTime = frameSamples > 0 ? (frameSum / frameSamples) : 0;
            _stats.loopMinTime = frameSamples > 0 ? minVal : 0;
            _stats.loopMaxTime = maxVal;
            _stats.loopGapAvgTime = gapSamples > 0 ? (gapSum / gapSamples) : 0;
            _stats.loopGapMinTime = gapSamples > 0 ? minGap : 0;
            _stats.loopGapMaxTime = maxGap;

            xSemaphoreGive(_statsMutex);
        }
    }

    // Obtener resumen de estadisticas
    RS485Stats getStats() {
        RS485Stats copy;
        if (xSemaphoreTake(_statsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            copy = _stats;
            xSemaphoreGive(_statsMutex);
        }
        return copy;
    }

    // Reportar si hay alertas pendientes
    void checkAndReportViolations() {
        if (xSemaphoreTake(_statsMutex, pdMS_TO_TICKS(5)) != pdTRUE) return;
        if (!_verboseAlerts || millis() - _stats.lastReportTime < ALERT_INTERVAL_MS) {
            xSemaphoreGive(_statsMutex);
            return;
        }

        uint32_t jitter = _stats.loopMaxTime - _stats.loopMinTime;
        if (jitter > LOOP_JITTER_THRESHOLD_MS) {
            LOG_F("RS485 loop jitter %ums exceeds threshold %ums\n", jitter, LOOP_JITTER_THRESHOLD_MS);
            _stats.lastReportTime = millis();
        }
        xSemaphoreGive(_statsMutex);
    }

    void setVerboseAlerts(bool enabled) {
        if (xSemaphoreTake(_statsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            _verboseAlerts = enabled;
            xSemaphoreGive(_statsMutex);
        }
    }

    // Reset de estadisticas
    void reset() {
        if (xSemaphoreTake(_statsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            _stats = RS485Stats();
            _stats.lastReportTime = millis();
            xSemaphoreGive(_statsMutex);
        }
    }
};

#endif
