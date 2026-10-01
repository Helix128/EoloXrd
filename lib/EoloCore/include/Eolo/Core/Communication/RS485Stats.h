#ifndef EOLO_CORE_COMMUNICATION_RS485_STATS_H
#define EOLO_CORE_COMMUNICATION_RS485_STATS_H

#include <stdint.h>

// Códigos estables para el monitor y los diagnósticos de placa.
enum RS485ErrorCode : uint8_t {
    RS485_OK = 0x00,
    RS485_INVALID_SLAVE = 0xE0,
    RS485_INVALID_FUNCTION = 0xE1,
    RS485_TIMEOUT = 0xE2,
    RS485_INVALID_CRC = 0xE3,
    RS485_MALFORMED = 0xE4,
    RS485_BUS_BUSY = 0xE5,
    RS485_EXCEPTION = 0xE6,
    RS485_UNEXPECTED_FRAME = 0xE7,
    RS485_INCOMPLETE_FRAME = 0xE8,
    // Alias corto para consumidores que imprimen la categoría directamente.
    RS485_INCOMPLETE = RS485_INCOMPLETE_FRAME
};

enum class RS485EndpointState : uint8_t { Online, Degraded, Offline };

struct RS485SlaveStats {
    uint32_t successes = 0;
    uint32_t failures = 0;
    uint32_t consecutiveFailures = 0;
    uint32_t lastSuccessMs = 0;
    uint32_t lastFailureMs = 0;
    uint32_t lastAttemptMs = 0;
    uint32_t nextProbeMs = 0;
    uint32_t lastLatencyMs = 0;
    uint32_t maxLatencyMs = 0;
    uint32_t maxAttemptGapMs = 0;
    uint32_t maxSuccessGapMs = 0;
    uint32_t lastRestMs = 0;
    uint32_t minRestMs = UINT32_MAX;
    uint32_t lastCompletedMs = 0;
    uint32_t deadlineMisses = 0;
    uint32_t lateBytes = 0;
    uint32_t unexpectedFrames = 0;
    uint32_t timeoutErrors = 0;
    uint32_t crcErrors = 0;
    uint32_t incompleteFrames = 0;
    uint32_t malformedFrames = 0;
    uint32_t busBusyErrors = 0;
    uint32_t unexpectedFrameErrors = 0;
    uint32_t modbusExceptions[12] = {0};
    uint32_t otherExceptionErrors = 0;
    uint8_t lastErrorCode = RS485_OK;
    uint8_t lastExceptionCode = 0;
    RS485EndpointState state = RS485EndpointState::Offline;

    // Un intento que empezó más de 20 ms después de su vencimiento cuenta como
    // fallo de plazo.
    static bool isDeadlineMiss(uint32_t startedMs, uint32_t dueMs)
    {
        return startedMs != dueMs && static_cast<int32_t>(startedMs - dueMs) > 20;
    }

    // Registra el resultado de una transacción. `nowMs` es la hora de
    // finalización; `startedMs` la de inicio. Tres fallos seguidos degradan el
    // endpoint y seis (o cualquier fallo antes del primer éxito) lo dan de
    // baja.
    void record(bool success, uint8_t error, uint8_t exceptionCode,
                uint32_t startedMs, uint32_t latencyMs,
                uint32_t lateBytesSeen, uint32_t unexpectedFramesSeen,
                uint32_t nowMs)
    {
        const bool hadPreviousAttempt = (successes + failures) > 0;
        if (hadPreviousAttempt) {
            const uint32_t gap = startedMs - lastAttemptMs;
            if (gap > maxAttemptGapMs) maxAttemptGapMs = gap;
            lastRestMs = startedMs - lastCompletedMs;
            if (lastRestMs < minRestMs) minRestMs = lastRestMs;
        }
        lastAttemptMs = startedMs;
        lastCompletedMs = startedMs + latencyMs;
        lastLatencyMs = latencyMs;
        if (latencyMs > maxLatencyMs) maxLatencyMs = latencyMs;
        lateBytes += lateBytesSeen;
        unexpectedFrames += unexpectedFramesSeen;
        if (success) {
            const bool hadPreviousSuccess = successes > 0;
            ++successes;
            if (hadPreviousSuccess) {
                const uint32_t gap = nowMs - lastSuccessMs;
                if (gap > maxSuccessGapMs) maxSuccessGapMs = gap;
            }
            consecutiveFailures = 0;
            lastSuccessMs = nowMs;
            lastErrorCode = RS485_OK;
            state = RS485EndpointState::Online;
        } else {
            ++failures;
            ++consecutiveFailures;
            lastFailureMs = nowMs;
            lastErrorCode = error;
            switch (error) {
                case RS485_TIMEOUT: ++timeoutErrors; break;
                case RS485_INVALID_CRC: ++crcErrors; break;
                case RS485_INCOMPLETE_FRAME: ++incompleteFrames; break;
                case RS485_MALFORMED: ++malformedFrames; break;
                case RS485_BUS_BUSY: ++busBusyErrors; break;
                case RS485_UNEXPECTED_FRAME:
                case RS485_INVALID_SLAVE:
                case RS485_INVALID_FUNCTION:
                    ++unexpectedFrameErrors;
                    break;
                case RS485_EXCEPTION:
                    // Conservar el último código exacto para diagnóstico aun
                    // si después aparece un timeout u otra clase de fallo.
                    lastExceptionCode = exceptionCode;
                    if (exceptionCode >= 1 && exceptionCode <= 11)
                        ++modbusExceptions[exceptionCode];
                    else
                        ++otherExceptionErrors;
                    break;
                default: break;
            }
            if (successes == 0 || consecutiveFailures >= 6) {
                state = RS485EndpointState::Offline;
            } else if (consecutiveFailures >= 3) {
                state = RS485EndpointState::Degraded;
            }
        }
    }
};

#endif
