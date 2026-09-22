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
    uint32_t malformedErrors = 0;
    uint32_t incompleteErrors = 0;
    uint32_t exceptionErrors = 0;
    uint32_t busBusyErrors = 0;
    uint32_t unexpectedFrameErrors = 0;
    uint32_t slaveTimeouts[256] = {0};
    // Índice igual al código Modbus (1..11). El índice 0 queda reservado.
    uint32_t exceptionCodes[12] = {0};
    uint32_t otherExceptionErrors = 0;
    uint8_t lastExceptionCode = 0;

    // Descanso observado entre el término de una transacción y el inicio de
    // la siguiente en el bus completo. UINT32_MAX significa "sin muestra".
    uint32_t lastRestMs = 0;
    uint32_t minRestMs = UINT32_MAX;
    uint32_t lastCompletedMs = 0;

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
    void recordRequestCompleted(bool success, uint8_t errorCode, uint8_t slaveId,
                                uint32_t transactionMs, uint8_t exceptionCode = 0,
                                uint32_t completedMs = UINT32_MAX,
                                uint32_t startedMs = UINT32_MAX) {
        // El task RS485 no puede quedar bloqueado por la consola o por una
        // copia de estadísticas; perder una métrica es preferible a perder
        // una ventana de sondeo del AFM07.
        if (xSemaphoreTake(_statsMutex, 0) == pdTRUE) {
            _stats.totalRequests++;
            _stats.lastTransactionMs = transactionMs;
            const uint32_t completion = completedMs != UINT32_MAX ? completedMs : millis();
            if (_stats.totalRequests > 1) {
                // El descanso es inicio_actual - término_anterior, no el
                // intervalo entre términos (que también contiene la trama
                // actual). Si el llamador legado no entrega inicio, se
                // reconstruye con la duración observada.
                const uint32_t start = startedMs != UINT32_MAX
                    ? startedMs
                    : completion - transactionMs;
                _stats.lastRestMs = start - _stats.lastCompletedMs;
                if (_stats.lastRestMs < _stats.minRestMs)
                    _stats.minRestMs = _stats.lastRestMs;
            }
            _stats.lastCompletedMs = completion;
            PROFILE_MARK("rs485.tx", transactionMs * 1000UL);
            if (transactionMs > _stats.maxTransactionMs) {
                _stats.maxTransactionMs = transactionMs;
            }
            if (success) {
                _stats.successfulReads++;
            } else {
                _stats.failedReads++;
                if (errorCode == 0xE2) { // RS485_TIMEOUT
                    _stats.timeoutErrors++;
                    _stats.slaveTimeouts[slaveId]++;
                } else if (errorCode == 0xE3) { // ku8MBInvalidCRC
                    _stats.crcErrors++;
                } else if (errorCode == 0xE4) {
                    _stats.malformedErrors++;
                } else if (errorCode == 0xE8) {
                    _stats.incompleteErrors++;
                } else if (errorCode == 0xE5) {
                    _stats.busBusyErrors++;
                } else if (errorCode == 0xE6) {
                    _stats.exceptionErrors++;
                    _stats.lastExceptionCode = exceptionCode;
                    if (exceptionCode >= 1 && exceptionCode <= 11)
                        _stats.exceptionCodes[exceptionCode]++;
                    else
                        _stats.otherExceptionErrors++;
                } else if (errorCode == 0xE0 || errorCode == 0xE1 || errorCode == 0xE7) {
                    _stats.unexpectedFrameErrors++;
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
