#ifndef RS485_MONITOR_HPP
#define RS485_MONITOR_HPP

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Estadisticas de monitoreo del bus RS485
struct RS485Stats {
    uint32_t totalRequests = 0;
    uint32_t successfulReads = 0;
    uint32_t failedReads = 0;
    uint32_t timeoutErrors = 0;
    uint32_t crcErrors = 0;
    
    uint32_t maxQueueDepth = 0;
    uint32_t totalQueueOverflows = 0; // Veces que la cola se lleno
    
    unsigned long lastReportTime = 0;
    unsigned long lastViolationTime = 0;
    
    // Para medir jitter del loop
    uint32_t loopFrameTimes[60] = {0}; // Buffer circular de 60 frames
    uint8_t loopFrameIndex = 0;
    uint32_t loopMinTime = UINT32_MAX;
    uint32_t loopMaxTime = 0;
    uint32_t loopAvgTime = 0;
};

// Monitor centralizado (singleton)
class RS485Monitor {
private:
    RS485Stats _stats;
    SemaphoreHandle_t _statsMutex;
    const uint32_t REPORT_INTERVAL_MS = 10000; // Reportar cada 10 segundos
    const uint32_t QUEUE_WARNING_THRESHOLD = 3;
    const uint32_t LOOP_JITTER_THRESHOLD_MS = 5; // Alerta si jitter > 5ms

    RS485Monitor() {
        _statsMutex = xSemaphoreCreateMutex();
        _stats.lastReportTime = millis();
    }

public:
    static RS485Monitor& getInstance() {
        static RS485Monitor instance;
        return instance;
    }

    // Registrar una solicitud completada
    void recordRequestCompleted(bool success, uint8_t errorCode) {
        if (xSemaphoreTake(_statsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            _stats.totalRequests++;
            if (success) {
                _stats.successfulReads++;
            } else {
                _stats.failedReads++;
                if (errorCode == 0xE2) { // ku8MBResponseTimedOut
                    _stats.timeoutErrors++;
                } else if (errorCode == 0xE4) { // ku8MBInvalidCRC
                    _stats.crcErrors++;
                }
            }
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
                _stats.totalQueueOverflows++;
                if (millis() - _stats.lastViolationTime > 5000) {
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
            _stats.loopFrameTimes[_stats.loopFrameIndex] = frameTimeMs;
            _stats.loopFrameIndex = (_stats.loopFrameIndex + 1) % 60;

            // Calcular estadisticas
            uint32_t sum = 0;
            uint32_t minVal = UINT32_MAX;
            uint32_t maxVal = 0;
            for (int i = 0; i < 60; i++) {
                sum += _stats.loopFrameTimes[i];
                if (_stats.loopFrameTimes[i] > 0) {
                    if (_stats.loopFrameTimes[i] < minVal) minVal = _stats.loopFrameTimes[i];
                    if (_stats.loopFrameTimes[i] > maxVal) maxVal = _stats.loopFrameTimes[i];
                }
            }
            _stats.loopAvgTime = sum / 60;
            _stats.loopMinTime = minVal;
            _stats.loopMaxTime = maxVal;

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
        if (millis() - _stats.lastReportTime < REPORT_INTERVAL_MS) {
            xSemaphoreGive(_statsMutex);
            return;
        }

        LOG_LN("\n=== RS485 Monitor Report ===");
        LOG_F("Total Requests: %d\n", _stats.totalRequests);
        LOG_F("Successful: %d (%.1f%%)\n", _stats.successfulReads,
              _stats.totalRequests > 0 ? (100.0f * _stats.successfulReads / _stats.totalRequests) : 0);
        LOG_F("Failed: %d (Timeouts: %d, CRC: %d)\n",
              _stats.failedReads, _stats.timeoutErrors, _stats.crcErrors);

        LOG_F("Queue Max Depth: %d (Overflows: %d)\n",
              _stats.maxQueueDepth, _stats.totalQueueOverflows);

        LOG_F("Loop Frame Times: Avg %ums, Min %ums, Max %ums\n",
              _stats.loopAvgTime, _stats.loopMinTime, _stats.loopMaxTime);

        uint32_t jitter = _stats.loopMaxTime - _stats.loopMinTime;
        if (jitter > LOOP_JITTER_THRESHOLD_MS) {
            LOG_F("WARNING: Loop jitter %ums exceeds threshold %ums\n",
                  jitter, LOOP_JITTER_THRESHOLD_MS);
        }

        _stats.lastReportTime = millis();
        xSemaphoreGive(_statsMutex);
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
