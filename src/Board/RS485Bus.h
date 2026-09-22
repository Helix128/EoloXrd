#ifndef RS485_BUS_HPP
#define RS485_BUS_HPP

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <atomic>
#include "../Utility/SystemDiagnostics.h"
#include <Eolo/Core/Communication/RS485Protocol.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "../Config/Legacy.h"
#include "../Utility/RS485Monitor.h"

#define RS485_BAUD_RATE EoloCore::ModbusRtuProtocol::Afm07BaudRate

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

enum class RS485DiagnosticState : uint8_t { Idle, Pending, Running, Complete, Failed };

struct RS485DiagnosticStatus {
    RS485DiagnosticState state = RS485DiagnosticState::Idle;
    uint16_t register0004 = 0;
    bool valueValid = false;
    // Un bloqueo por register0004 se mantiene hasta una lectura explícita de
    // 0x0004 == 0. Los fallos de transporte transitorios pueden limpiarse con
    // una lectura de flujo válida posterior.
    bool rateTuningBlocked = false;
    uint8_t errorCode = RS485_OK;
    uint8_t exceptionCode = 0;
    uint32_t requestedMs = 0;
    uint32_t completedMs = 0;
    uint32_t sequence = 0;
};

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
};

typedef void (*RS485ReadCallback)(void* context, bool success, const uint16_t* registers,
                                  uint8_t count, uint8_t errorCode);

class RS485Bus {
private:
    struct Endpoint {
        uint8_t slaveId;
        uint16_t startReg;
        uint8_t count;
        uint32_t intervalMs;
        uint32_t offlineIntervalMs;
        bool critical;
        bool registered;
        uint32_t nextDueMs;
        RS485ReadCallback callback;
        void* context;
    };

    static constexpr int ANEMOMETER_INDEX = 0;
    static constexpr int AFM07_INDEX = 1;
    static constexpr int BUS_TASK_STACK_SIZE = 4096;
    static constexpr UBaseType_t BUS_TASK_PRIORITY = 3;
    static constexpr BaseType_t BUS_TASK_CORE = 0;
    static constexpr uint32_t BUS_STUCK_MAX_MS = EoloCore::RS485TimingModel::kBusQuietTimeoutMs;
    static constexpr uint8_t RECOVER_AFTER_CORRUPT_FRAMES = 3;
    // Un fallo aislado del AFM07 puede ser ruido, arranque del transceptor o
    // una respuesta que llegó fuera de ventana. Sólo una racha persistente
    // debe bloquear la captura.
    static constexpr uint8_t AFM_SAFETY_FAILURE_ATTEMPTS =
        EoloCore::RS485TimingModel::kAfmSafetyFailureAttempts;

    SoftwareSerial _serial;
    SemaphoreHandle_t _initMutex;
    SemaphoreHandle_t _endpointMutex;
    std::atomic<bool> _initialized;
    TaskHandle_t _busTaskHandle = nullptr;
    Endpoint _endpoints[2] = {
        {0x01, 0x0000, 2, EoloCore::RS485TimingModel::kAnemometerIntervalMs,
         EoloCore::RS485TimingModel::kAnemometerOfflineIntervalMs, false, false, 0, nullptr, nullptr},
        {EoloCore::ModbusRtuProtocol::Afm07SlaveId,
         EoloCore::ModbusRtuProtocol::Afm07FlowRegister,
         EoloCore::ModbusRtuProtocol::Afm07FlowCount,
         EoloCore::RS485TimingModel::kAfmPollGapMs,
         EoloCore::RS485TimingModel::kAfmPollGapMs, true, false, 0, nullptr, nullptr}
    };
    RS485SlaveStats _slaveStats[2];
    portMUX_TYPE _statsMux = portMUX_INITIALIZER_UNLOCKED;
    RS485DiagnosticStatus _afmDiagnostic;
    mutable portMUX_TYPE _diagnosticMux = portMUX_INITIALIZER_UNLOCKED;
    std::atomic<bool> _afmSafetyBlocked{false};
    // Un register0004 distinto de cero es un diagnóstico interno confirmado;
    // no se limpia sólo porque vuelva una lectura de flujo.
    std::atomic<bool> _afmDiagnosticRegisterBlocked{false};

    uint8_t _corruptFrameStreak = 0;
    // Estas métricas se leen desde la consola/UI mientras el scheduler corre
    // en el core 0. Mantenerlas atómicas evita diagnósticos inconsistentes.
    std::atomic<uint32_t> _busBusyDeferrals{0};
    std::atomic<uint32_t> _busRecoveries{0};
    std::atomic<uint32_t> _lateBytes{0};
    std::atomic<uint32_t> _unexpectedFrames{0};

    RS485Bus()
        : _serial(RS485_RX_PIN, RS485_TX_PIN),
          _initMutex(xSemaphoreCreateMutex()),
          _endpointMutex(xSemaphoreCreateMutex()),
          _initialized(false) {}

    RS485Bus(const RS485Bus&) = delete;
    RS485Bus& operator=(const RS485Bus&) = delete;

    static int trackedSlaveIndex(uint8_t slaveId) {
        return slaveId == 0x01 ? ANEMOMETER_INDEX : (slaveId == 0x02 ? AFM07_INDEX : -1);
    }

    static const char* endpointName(uint8_t slaveId) {
        return slaveId == 0x01 ? "Anemometro" : "AFM07";
    }

    static bool due(uint32_t nowMs, uint32_t deadlineMs) {
        return EoloCore::RS485TimingModel::due(nowMs, deadlineMs);
    }

    static uint8_t errorForProtocol(EoloCore::ModbusReadStatus status) {
        switch (status) {
            case EoloCore::ModbusReadStatus::InvalidCrc: return RS485_INVALID_CRC;
            case EoloCore::ModbusReadStatus::UnexpectedSlave: return RS485_INVALID_SLAVE;
            case EoloCore::ModbusReadStatus::UnexpectedFunction: return RS485_INVALID_FUNCTION;
            case EoloCore::ModbusReadStatus::Exception: return RS485_EXCEPTION;
            case EoloCore::ModbusReadStatus::Incomplete: return RS485_INCOMPLETE_FRAME;
            case EoloCore::ModbusReadStatus::InvalidByteCount: return RS485_MALFORMED;
            case EoloCore::ModbusReadStatus::Ok: return RS485_OK;
            default: return RS485_MALFORMED;
        }
    }

    static const char* stateName(RS485EndpointState state) {
        switch (state) {
            case RS485EndpointState::Online: return "Online";
            case RS485EndpointState::Degraded: return "Degraded";
            default: return "Offline";
        }
    }

    static const char* errorName(uint8_t error) {
        switch (error) {
            case RS485_OK: return "Exito";
            case RS485_TIMEOUT: return "Timeout";
            case RS485_INVALID_CRC: return "CRC invalido";
            case RS485_MALFORMED: return "Trama invalida";
            case RS485_BUS_BUSY: return "Bus ocupado";
            case RS485_EXCEPTION: return "Excepcion Modbus";
            case RS485_INVALID_SLAVE: return "Esclavo inesperado";
            case RS485_INVALID_FUNCTION: return "Funcion inesperada";
            case RS485_INCOMPLETE_FRAME: return "Trama incompleta";
            case RS485_UNEXPECTED_FRAME: return "Trama inesperada";
            default: return "Error";
        }
    }

    static const char* diagnosticStateName(RS485DiagnosticState state) {
        switch (state) {
            case RS485DiagnosticState::Idle: return "idle";
            case RS485DiagnosticState::Pending: return "pending";
            case RS485DiagnosticState::Running: return "running";
            case RS485DiagnosticState::Complete: return "complete";
            case RS485DiagnosticState::Failed: return "failed";
            default: return "unknown";
        }
    }

    void setReceiveMode() { digitalWrite(RS485_DE_RE_PIN, LOW); }

    // Espera silencio continuo, no sólo una lectura vacía puntual. El polling
    // de 1 ms es suficiente para detectar los bytes de una trama a 4800 baud
    // (cada carácter tarda unos 2.08 ms).
    bool waitForQuiet(uint32_t quietUs, uint32_t maxMs, uint32_t* drained = nullptr) {
        const uint32_t startedMs = millis();
        uint32_t quietSinceUs = micros();
        uint32_t drainedBytes = 0;

        while (static_cast<uint32_t>(millis() - startedMs) < maxMs) {
            while (_serial.available() > 0) {
                (void)_serial.read();
                ++drainedBytes;
                quietSinceUs = micros();
            }
            if (static_cast<uint32_t>(micros() - quietSinceUs) >= quietUs) {
                if (drained) *drained = drainedBytes;
                return true;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        if (drained) *drained = drainedBytes;
        return false;
    }

    void recoverTransport() {
        setReceiveMode();
        uint32_t drained = 0;
        (void)waitForQuiet(EoloCore::RS485TimingModel::kBusQuietUs, 40, &drained);
        _serial.end();
        vTaskDelay(pdMS_TO_TICKS(10));
        _serial.begin(RS485_BAUD_RATE, SWSERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
        _serial.setTransmitEnablePin(RS485_DE_RE_PIN);
        setReceiveMode();
        (void)waitForQuiet(EoloCore::RS485TimingModel::kBusQuietUs, 40, &drained);
        _corruptFrameStreak = 0;
        _busRecoveries.fetch_add(1, std::memory_order_relaxed);
        LOG_LN("RS485: transporte reiniciado tras ruido o bus ocupado");
    }

    bool transact(const Endpoint& endpoint, uint16_t* result, uint8_t& error,
                  uint8_t& exceptionCode, uint32_t& protocolLateBytes,
                  uint32_t& protocolUnexpectedFrames) {
        exceptionCode = 0;
        uint8_t request[8] = {};
        if (!EoloCore::ModbusRtuProtocol::buildReadHolding(endpoint.slaveId, endpoint.startReg,
                                                            endpoint.count, request, sizeof(request))) {
            error = RS485_MALFORMED;
            return false;
        }

        uint32_t drained = 0;
        if (!waitForQuiet(EoloCore::RS485TimingModel::kBusQuietUs, BUS_STUCK_MAX_MS, &drained)) {
            _busBusyDeferrals.fetch_add(1, std::memory_order_relaxed);
            error = RS485_BUS_BUSY;
            return false;
        }
        if (drained > 0) {
            protocolLateBytes += drained;
            _lateBytes.fetch_add(drained, std::memory_order_relaxed);
        }

        // EspSoftwareSerial write() es síncrono y setTransmitEnablePin() baja
        // DE/RE después del último bit de parada; no se usa flush(), que vacía RX.
        _serial.setTransmitEnablePin(RS485_DE_RE_PIN);
        const size_t written = _serial.write(request, sizeof(request));
        setReceiveMode();
        if (written != sizeof(request)) {
            error = RS485_MALFORMED;
            return false;
        }

        const uint32_t responseDeadlineMs = millis() +
            EoloCore::RS485TimingModel::kResponseStartTimeoutMs;
        const uint8_t maxFrameLength = static_cast<uint8_t>(5U +
            EoloCore::ModbusRtuProtocol::MaxReadRegisters * 2U);
        uint8_t frame[5 + EoloCore::ModbusRtuProtocol::MaxReadRegisters * 2] = {};
        uint16_t registers[EoloCore::ModbusRtuProtocol::MaxReadRegisters] = {};
        uint16_t frameLength = 0;
        uint16_t expectedLength = 0;
        uint32_t frameStartedMs = 0;
        uint32_t lastByteUs = micros();

        while (true) {
            while (_serial.available() > 0) {
                const int value = _serial.read();
                if (value < 0) continue;
                if (frameLength == 0) frameStartedMs = millis();
                if (frameLength < maxFrameLength)
                    frame[frameLength++] = static_cast<uint8_t>(value);
                else {
                    error = RS485_MALFORMED;
                    return false;
                }
                lastByteUs = micros();

                if (frameLength >= 3) {
                    if (frame[1] == static_cast<uint8_t>(EoloCore::ModbusRtuProtocol::ReadHoldingRegisters | 0x80U))
                        expectedLength = 5;
                    else if (frame[1] == EoloCore::ModbusRtuProtocol::ReadHoldingRegisters)
                        expectedLength = static_cast<uint16_t>(5U + frame[2]);
                    else
                        expectedLength = 5;
                }

                if (expectedLength > maxFrameLength) {
                    error = RS485_MALFORMED;
                    return false;
                }
                if (expectedLength > 0 && frameLength == expectedLength) {
                    const EoloCore::ModbusReadResult parsed =
                        EoloCore::ModbusRtuProtocol::parseReadResponse(
                            frame, frameLength, endpoint.slaveId, endpoint.count, registers);
                    if (parsed.status == EoloCore::ModbusReadStatus::UnexpectedSlave ||
                        parsed.status == EoloCore::ModbusReadStatus::UnexpectedFunction) {
                        ++protocolUnexpectedFrames;
                        _unexpectedFrames.fetch_add(1, std::memory_order_relaxed);
                        frameLength = 0;
                        expectedLength = 0;
                        frameStartedMs = 0;
                        continue;
                    }
                    if (parsed.status == EoloCore::ModbusReadStatus::Ok) {
                        for (uint8_t i = 0; i < endpoint.count; ++i)
                            result[i] = registers[i];
                        error = RS485_OK;
                        return true;
                    }
                    if (parsed.status == EoloCore::ModbusReadStatus::Exception) {
                        error = RS485_EXCEPTION;
                        exceptionCode = parsed.exceptionCode;
                    }
                    else
                        error = errorForProtocol(parsed.status);
                    return false;
                }
            }

            const uint32_t nowMs = millis();
            if (frameLength == 0) {
                if (due(nowMs, responseDeadlineMs)) {
                    error = protocolUnexpectedFrames > 0 ? RS485_UNEXPECTED_FRAME : RS485_TIMEOUT;
                    return false;
                }
            } else {
                if (static_cast<uint32_t>(micros() - lastByteUs) >= EoloCore::RS485TimingModel::kBusQuietUs ||
                    static_cast<uint32_t>(nowMs - frameStartedMs) >=
                        EoloCore::RS485TimingModel::kFrameCompletionTimeoutMs) {
                    // Hubo una respuesta parcial, pero nunca llegó una trama
                    // completa dentro de la ventana Modbus.
                    error = RS485_INCOMPLETE_FRAME;
                    return false;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    void recordResult(const Endpoint& endpoint, bool success, uint8_t error,
                      uint8_t exceptionCode,
                      uint32_t startedMs, uint32_t latencyMs,
                      uint32_t lateBytes, uint32_t unexpectedFrames) {
        const int index = trackedSlaveIndex(endpoint.slaveId);
        if (index < 0) return;
        const uint32_t now = millis();
        RS485EndpointState oldState;
        RS485EndpointState newState;
        uint32_t afmConsecutiveFailures = 0;
        portENTER_CRITICAL(&_statsMux);
        RS485SlaveStats& stats = _slaveStats[index];
        oldState = stats.state;
        const bool hadPreviousAttempt = (stats.successes + stats.failures) > 0;
        if (hadPreviousAttempt) {
            const uint32_t gap = startedMs - stats.lastAttemptMs;
            if (gap > stats.maxAttemptGapMs) stats.maxAttemptGapMs = gap;
            stats.lastRestMs = startedMs - stats.lastCompletedMs;
            if (stats.lastRestMs < stats.minRestMs) stats.minRestMs = stats.lastRestMs;
        }
        stats.lastAttemptMs = startedMs;
        stats.lastCompletedMs = startedMs + latencyMs;
        stats.lastLatencyMs = latencyMs;
        if (latencyMs > stats.maxLatencyMs) stats.maxLatencyMs = latencyMs;
        stats.lateBytes += lateBytes;
        stats.unexpectedFrames += unexpectedFrames;
        if (success) {
            const bool hadPreviousSuccess = stats.successes > 0;
            ++stats.successes;
            if (hadPreviousSuccess) {
                const uint32_t gap = now - stats.lastSuccessMs;
                if (gap > stats.maxSuccessGapMs) stats.maxSuccessGapMs = gap;
            }
            stats.consecutiveFailures = 0;
            stats.lastSuccessMs = now;
            stats.lastErrorCode = RS485_OK;
            stats.state = RS485EndpointState::Online;
        } else {
            ++stats.failures;
            ++stats.consecutiveFailures;
            stats.lastFailureMs = now;
            stats.lastErrorCode = error;
            switch (error) {
                case RS485_TIMEOUT: ++stats.timeoutErrors; break;
                case RS485_INVALID_CRC: ++stats.crcErrors; break;
                case RS485_INCOMPLETE_FRAME: ++stats.incompleteFrames; break;
                case RS485_MALFORMED: ++stats.malformedFrames; break;
                case RS485_BUS_BUSY: ++stats.busBusyErrors; break;
                case RS485_UNEXPECTED_FRAME:
                case RS485_INVALID_SLAVE:
                case RS485_INVALID_FUNCTION:
                    ++stats.unexpectedFrameErrors;
                    break;
                case RS485_EXCEPTION:
                    // Conservar el último código exacto para diagnóstico aun
                    // si después aparece un timeout u otra clase de fallo.
                    stats.lastExceptionCode = exceptionCode;
                    if (exceptionCode >= 1 && exceptionCode <= 11)
                        ++stats.modbusExceptions[exceptionCode];
                    else
                        ++stats.otherExceptionErrors;
                    break;
                default: break;
            }
            if (stats.successes == 0 || stats.consecutiveFailures >= 6) {
                stats.state = RS485EndpointState::Offline;
            } else if (stats.consecutiveFailures >= 3) {
                stats.state = RS485EndpointState::Degraded;
            }
        }
        afmConsecutiveFailures = stats.consecutiveFailures;
        newState = stats.state;
        portEXIT_CRITICAL(&_statsMux);

        if (endpoint.slaveId == EoloCore::ModbusRtuProtocol::Afm07SlaveId) {
            if (success) {
                // Una lectura válida demuestra que el transporte se recuperó;
                // la siguiente captura no debe heredar fallos transitorios.
                const bool wasBlocked = _afmSafetyBlocked.exchange(false, std::memory_order_acq_rel);
                const bool diagnosticRegisterBlocked =
                    _afmDiagnosticRegisterBlocked.load(std::memory_order_acquire);
                portENTER_CRITICAL(&_diagnosticMux);
                _afmDiagnostic.rateTuningBlocked = diagnosticRegisterBlocked;
                portEXIT_CRITICAL(&_diagnosticMux);
                if (wasBlocked && !diagnosticRegisterBlocked)
                    LOG_LN("RS485 AFM07: lectura recuperada; bloqueo transitorio limpiado");
            } else if (EoloCore::RS485TimingModel::afmSafetyBlockReached(afmConsecutiveFailures)) {
                // Tanto un timeout como una excepción interna deben repetirse
                // antes de detener una captura por una condición de transporte.
                const bool wasBlocked = _afmSafetyBlocked.exchange(true, std::memory_order_acq_rel);
                if (!wasBlocked) {
                    LOG_F("RS485 AFM07: %u fallos consecutivos; bloqueo de seguridad activo (ultimo=0x%02X)\n",
                          AFM_SAFETY_FAILURE_ATTEMPTS, error);
                }
                portENTER_CRITICAL(&_diagnosticMux);
                _afmDiagnostic.rateTuningBlocked = true;
                portEXIT_CRITICAL(&_diagnosticMux);
            }
        }

        if (oldState != newState) {
            LOG_F("RS485 %s %s\n", endpointName(endpoint.slaveId),
                  newState == RS485EndpointState::Online ? "online" :
                  (newState == RS485EndpointState::Degraded ? "degradado" : "offline"));
        }
    }

    void noteDeadlineMiss(int index, uint32_t startedMs, uint32_t dueMs) {
        if (index < 0 || startedMs == dueMs) return;
        if (static_cast<int32_t>(startedMs - dueMs) > 20) {
            portENTER_CRITICAL(&_statsMux);
            ++_slaveStats[index].deadlineMisses;
            portEXIT_CRITICAL(&_statsMux);
        }
    }

    void scheduleNext(int index, const Endpoint& endpoint, uint32_t completedMs,
                      bool success, uint8_t error, uint8_t exceptionCode) {
        if (index < 0) return;
        uint32_t nextDue;
        if (!success && !endpoint.critical) {
            nextDue = completedMs + endpoint.offlineIntervalMs;
        } else if (!success) {
            // El AFM07 siempre se reancla al término de la transacción. Un
            // 0x06 (device busy) añade una cadencia completa; ningún fallo
            // puede dejar vencimientos acumulados para una ráfaga posterior.
            const bool deviceBusy = error == RS485_EXCEPTION && exceptionCode == 0x06;
            nextDue = EoloCore::RS485TimingModel::nextDueAfterCompletion(
                completedMs, endpoint.intervalMs, deviceBusy ? 1U : 0U);
        } else {
            nextDue = EoloCore::RS485TimingModel::nextDueAfterCompletion(
                completedMs, endpoint.intervalMs);
        }
        if (xSemaphoreTake(_endpointMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            _endpoints[index].nextDueMs = nextDue;
            xSemaphoreGive(_endpointMutex);
        }
        portENTER_CRITICAL(&_statsMux);
        _slaveStats[index].nextProbeMs = nextDue;
        portEXIT_CRITICAL(&_statsMux);
    }

    bool claimAfmDiagnostic()
    {
        bool claimed = false;
        portENTER_CRITICAL(&_diagnosticMux);
        if (_afmDiagnostic.state == RS485DiagnosticState::Pending)
        {
            _afmDiagnostic.state = RS485DiagnosticState::Running;
            claimed = true;
        }
        portEXIT_CRITICAL(&_diagnosticMux);
        return claimed;
    }

    bool runPendingAfmDiagnostic()
    {
        if (!claimAfmDiagnostic())
            return false;

        Endpoint diagnosticEndpoint = {
            EoloCore::ModbusRtuProtocol::Afm07SlaveId,
            EoloCore::ModbusRtuProtocol::Afm07DiagnosticRegister,
            EoloCore::ModbusRtuProtocol::Afm07FlowCount,
            EoloCore::RS485TimingModel::kAfmPollGapMs,
            EoloCore::RS485TimingModel::kAfmPollGapMs, true, true, 0, nullptr, nullptr};
        uint16_t registers[EoloCore::ModbusRtuProtocol::MaxReadRegisters] = {};
        uint8_t error = RS485_TIMEOUT;
        uint8_t exceptionCode = 0;
        uint32_t lateBytes = 0;
        uint32_t unexpectedFrames = 0;
        const uint32_t started = millis();
        const bool success = transact(diagnosticEndpoint, registers, error, exceptionCode,
                                      lateBytes, unexpectedFrames);
        const uint32_t completed = millis();
        const uint32_t latency = completed - started;

        // El diagnóstico es una transacción real y queda contabilizado junto
        // con el resto del bus; el barrido puede resetear estadísticas después
        // de confirmar este registro.
        recordResult(diagnosticEndpoint, success, error, exceptionCode, started, latency,
                     lateBytes, unexpectedFrames);
        RS485Monitor::getInstance().recordRequestCompleted(success, error,
                                                           diagnosticEndpoint.slaveId,
                                                           latency, exceptionCode, completed, started);
        scheduleNext(AFM07_INDEX, diagnosticEndpoint, completed, success, error,
                     exceptionCode);

        const uint16_t value = success ? registers[0] : 0;
        if (success && value != 0)
            _afmDiagnosticRegisterBlocked.store(true, std::memory_order_release);
        else if (success && value == 0)
            _afmDiagnosticRegisterBlocked.store(false, std::memory_order_release);
        const bool blocked = isAfmSafetyBlocked();
        portENTER_CRITICAL(&_diagnosticMux);
        _afmDiagnostic.register0004 = value;
        _afmDiagnostic.valueValid = success;
        _afmDiagnostic.rateTuningBlocked = blocked;
        _afmDiagnostic.errorCode = error;
        _afmDiagnostic.exceptionCode = exceptionCode;
        _afmDiagnostic.completedMs = completed;
        _afmDiagnostic.state = success ? RS485DiagnosticState::Complete
                                       : RS485DiagnosticState::Failed;
        portEXIT_CRITICAL(&_diagnosticMux);

        if (!success) {
            LOG_F("RS485 AFM07 diagnóstico 0x0004 falló: error=0x%02X exc=0x%02X\n",
                  error, exceptionCode);
        } else if (value == 0) {
            LOG_LN("RS485 AFM07 diagnóstico 0x0004=0; rate habilitado para calificación");
        } else {
            LOG_F("RS485 AFM07 diagnóstico 0x0004=0x%04X; rate bloqueado\n", value);
        }
        return true;
    }

    bool selectDueEndpoint(Endpoint& selected, int& index) {
        if (_endpointMutex == nullptr ||
            xSemaphoreTake(_endpointMutex, pdMS_TO_TICKS(5)) != pdTRUE)
            return false;

        const uint32_t now = millis();
        index = -1;
        if (_endpoints[AFM07_INDEX].registered &&
            due(now, _endpoints[AFM07_INDEX].nextDueMs)) {
            index = AFM07_INDEX;
        } else if (_endpoints[ANEMOMETER_INDEX].registered &&
                   due(now, _endpoints[ANEMOMETER_INDEX].nextDueMs) &&
                   EoloCore::RS485TimingModel::optionalFitsBeforeCritical(
                       now, _endpoints[AFM07_INDEX].nextDueMs,
                       _endpoints[AFM07_INDEX].registered,
                       EoloCore::RS485TimingModel::kAnemometerSlotBudgetMs)) {
            index = ANEMOMETER_INDEX;
        }

        if (index >= 0) selected = _endpoints[index];
        xSemaphoreGive(_endpointMutex);
        return index >= 0;
    }

    void pollEndpoint(const Endpoint& endpoint, int index) {
        uint16_t registers[EoloCore::ModbusRtuProtocol::MaxReadRegisters] = {};
        uint8_t error = RS485_TIMEOUT;
        uint8_t exceptionCode = 0;
        uint32_t lateBytes = 0;
        uint32_t unexpectedFrames = 0;
        const uint32_t started = millis();
        noteDeadlineMiss(index, started, endpoint.nextDueMs);
        const bool success = transact(endpoint, registers, error, exceptionCode,
                                      lateBytes, unexpectedFrames);
        const uint32_t completed = millis();
        const uint32_t latency = completed - started;
        recordResult(endpoint, success, error, exceptionCode, started, latency,
                     lateBytes, unexpectedFrames);
        RS485Monitor::getInstance().recordRequestCompleted(success, error, endpoint.slaveId,
                                                           latency, exceptionCode, completed, started);

        if (success) _corruptFrameStreak = 0;
        else if (error == RS485_INVALID_CRC || error == RS485_MALFORMED ||
                 error == RS485_INCOMPLETE_FRAME || error == RS485_BUS_BUSY) {
            if (++_corruptFrameStreak >= RECOVER_AFTER_CORRUPT_FRAMES) recoverTransport();
        } else if (error != RS485_TIMEOUT && error != RS485_EXCEPTION) {
            _corruptFrameStreak = 0;
        }

        scheduleNext(index, endpoint, completed, success, error, exceptionCode);
        if (endpoint.callback) {
            endpoint.callback(endpoint.context, success, success ? registers : nullptr,
                               endpoint.count, error);
        }
    }

    static void busTask(void* arg) {
        RS485Bus* self = static_cast<RS485Bus*>(arg);
        for (;;) {
            SystemDiagnostics::instance().rs485Beat();
            Endpoint endpoint{};
            int index = -1;
            if (self->_initialized.load() && self->runPendingAfmDiagnostic())
                continue;
            if (self->_initialized.load() && self->selectDueEndpoint(endpoint, index))
                self->pollEndpoint(endpoint, index);
            else
                vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

public:
    static RS485Bus& getInstance() { static RS485Bus instance; return instance; }

    bool begin() {
        if (_initMutex == nullptr ||
            xSemaphoreTake(_initMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
            return false;
        if (_initialized.load()) {
            xSemaphoreGive(_initMutex);
            return true;
        }

        pinMode(RS485_DE_RE_PIN, OUTPUT);
        setReceiveMode();
        _serial.begin(RS485_BAUD_RATE, SWSERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
        _serial.setTransmitEnablePin(RS485_DE_RE_PIN);
        setReceiveMode();
        uint32_t drained = 0;
        (void)waitForQuiet(EoloCore::RS485TimingModel::kBusQuietUs, 40, &drained);

        TaskHandle_t taskHandle = nullptr;
        const BaseType_t created = xTaskCreatePinnedToCore(
            busTask, "RS485Scheduler", BUS_TASK_STACK_SIZE, this,
            BUS_TASK_PRIORITY, &taskHandle, BUS_TASK_CORE);
        if (created != pdPASS || taskHandle == nullptr) {
            _serial.end();
            _initialized = false;
            xSemaphoreGive(_initMutex);
            LOG_LN("RS485: no se pudo crear RS485Scheduler");
            return false;
        }
        _busTaskHandle = taskHandle;
        _initialized = true;
        xSemaphoreGive(_initMutex);
        return true;
    }

    bool registerEndpoint(uint8_t slaveId, uint16_t startReg, uint8_t count,
                          RS485ReadCallback callback, void* context) {
        const int index = trackedSlaveIndex(slaveId);
        if (index < 0 || callback == nullptr || count == 0 ||
            count > EoloCore::ModbusRtuProtocol::MaxReadRegisters)
            return false;
        if ((index == ANEMOMETER_INDEX && (startReg != 0x0000 || count != 2)) ||
            (index == AFM07_INDEX && (startReg != 0x0000 || count != 1)))
            return false;
        if (!begin() || _endpointMutex == nullptr ||
            xSemaphoreTake(_endpointMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
            return false;

        Endpoint& endpoint = _endpoints[index];
        if (endpoint.registered) {
            const bool sameOwner = endpoint.callback == callback && endpoint.context == context;
            xSemaphoreGive(_endpointMutex);
            return sameOwner;
        }
        endpoint.callback = callback;
        endpoint.context = context;
        endpoint.nextDueMs = millis();
        endpoint.registered = true;
        xSemaphoreGive(_endpointMutex);
        return true;
    }

    // Encola una lectura exclusiva de AFM07 0x0004. El task dueño del UART
    // ejecuta la operación antes de volver al planificador normal, por lo que
    // no hay dos lectores ni una consulta normal intercalada en la trama.
    bool startAfmDiagnostic()
    {
        if (!begin())
            return false;
        bool accepted = false;
        portENTER_CRITICAL(&_diagnosticMux);
        if (_afmDiagnostic.state != RS485DiagnosticState::Pending &&
            _afmDiagnostic.state != RS485DiagnosticState::Running)
        {
            _afmDiagnostic.state = RS485DiagnosticState::Pending;
            _afmDiagnostic.register0004 = 0;
            _afmDiagnostic.valueValid = false;
            _afmDiagnostic.rateTuningBlocked = isAfmSafetyBlocked();
            _afmDiagnostic.errorCode = RS485_OK;
            _afmDiagnostic.exceptionCode = 0;
            _afmDiagnostic.requestedMs = millis();
            _afmDiagnostic.completedMs = 0;
            ++_afmDiagnostic.sequence;
            accepted = true;
        }
        portEXIT_CRITICAL(&_diagnosticMux);
        return accepted;
    }

    RS485DiagnosticStatus getAfmDiagnosticStatus() const
    {
        RS485DiagnosticStatus copy;
        portENTER_CRITICAL(&_diagnosticMux);
        copy = _afmDiagnostic;
        portEXIT_CRITICAL(&_diagnosticMux);
        return copy;
    }

    bool isAfmSafetyBlocked() const
    {
        return _afmSafetyBlocked.load(std::memory_order_acquire) ||
               _afmDiagnosticRegisterBlocked.load(std::memory_order_acquire);
    }

    uint32_t lateBytes() const { return _lateBytes.load(std::memory_order_acquire); }
    uint32_t unexpectedFrames() const {
        return _unexpectedFrames.load(std::memory_order_acquire);
    }
    uint32_t busBusyDeferrals() const {
        return _busBusyDeferrals.load(std::memory_order_acquire);
    }
    uint32_t transportRecoveries() const {
        return _busRecoveries.load(std::memory_order_acquire);
    }

    // APIs heredadas: se rechazan para impedir que una tarea vuelva a tomar
    // propiedad del UART fuera del planificador fijo.
    [[deprecated("use registerEndpoint() y getData()")]]
    bool readRegistersAsync(uint8_t, uint16_t, uint8_t, uint16_t*, uint32_t = 0) { return false; }
    [[deprecated("use registerEndpoint() y getData()")]]
    bool readRegisters(uint8_t, uint16_t, uint8_t, uint16_t*) { return false; }
    uint32_t getPendingRequests() const { return 0; }

    RS485SlaveStats getSlaveStats(uint8_t slaveId) {
        RS485SlaveStats copy;
        const int index = trackedSlaveIndex(slaveId);
        if (index < 0) return copy;
        portENTER_CRITICAL(&_statsMux);
        copy = _slaveStats[index];
        portEXIT_CRITICAL(&_statsMux);
        return copy;
    }

    void resetSlaveStats() {
        portENTER_CRITICAL(&_statsMux);
        _slaveStats[ANEMOMETER_INDEX] = RS485SlaveStats();
        _slaveStats[AFM07_INDEX] = RS485SlaveStats();
        portEXIT_CRITICAL(&_statsMux);
        _busBusyDeferrals.store(0, std::memory_order_relaxed);
        _busRecoveries.store(0, std::memory_order_relaxed);
        _lateBytes.store(0, std::memory_order_relaxed);
        _unexpectedFrames.store(0, std::memory_order_relaxed);
    }

    void printStatus(Print& out) {
        const uint32_t now = millis();
        out.println("RS485: planificador unico (AFM07 reservado; anemometro opcional)");
        out.printf("  afm_poll_gap=%lums bus_busy=%lu recoveries=%lu late_bytes=%lu unexpected=%lu\n",
                   static_cast<unsigned long>(EoloCore::RS485TimingModel::kAfmPollGapMs),
                   static_cast<unsigned long>(_busBusyDeferrals.load(std::memory_order_relaxed)),
                   static_cast<unsigned long>(_busRecoveries.load(std::memory_order_relaxed)),
                   static_cast<unsigned long>(_lateBytes.load(std::memory_order_relaxed)),
                   static_cast<unsigned long>(_unexpectedFrames.load(std::memory_order_relaxed)));
        out.printf("  afm07_contract=id=0x%02X baud=%lu function=0x%02X flow_reg=0x%04X diag_reg=0x%04X count=%u\n",
                   EoloCore::ModbusRtuProtocol::Afm07SlaveId,
                   static_cast<unsigned long>(EoloCore::ModbusRtuProtocol::Afm07BaudRate),
                   EoloCore::ModbusRtuProtocol::ReadHoldingRegisters,
                   EoloCore::ModbusRtuProtocol::Afm07FlowRegister,
                   EoloCore::ModbusRtuProtocol::Afm07DiagnosticRegister,
                   EoloCore::ModbusRtuProtocol::Afm07FlowCount);
        const RS485Stats monitor = RS485Monitor::getInstance().getStats();
        out.printf("  transactions=%lu ok=%lu fail=%lu timeout=%lu crc=%lu malformed=%lu "
                   "incomplete=%lu busy=%lu unexpected=%lu exception=%lu lastexc=0x%02X minrest=%lums\n",
                   static_cast<unsigned long>(monitor.totalRequests),
                   static_cast<unsigned long>(monitor.successfulReads),
                   static_cast<unsigned long>(monitor.failedReads),
                   static_cast<unsigned long>(monitor.timeoutErrors),
                   static_cast<unsigned long>(monitor.crcErrors),
                   static_cast<unsigned long>(monitor.malformedErrors),
                   static_cast<unsigned long>(monitor.incompleteErrors),
                   static_cast<unsigned long>(monitor.busBusyErrors),
                   static_cast<unsigned long>(monitor.unexpectedFrameErrors),
                   static_cast<unsigned long>(monitor.exceptionErrors),
                   monitor.lastExceptionCode,
                   static_cast<unsigned long>(monitor.minRestMs == UINT32_MAX ? 0 : monitor.minRestMs));
        out.print("  modbus_exceptions:");
        for (uint8_t code = 1; code <= 11; ++code) {
            if (monitor.exceptionCodes[code] != 0)
                out.printf(" 0x%02X=%lu", code,
                           static_cast<unsigned long>(monitor.exceptionCodes[code]));
        }
        if (monitor.otherExceptionErrors != 0)
            out.printf(" other=%lu", static_cast<unsigned long>(monitor.otherExceptionErrors));
        out.println();
        const RS485DiagnosticStatus diagnostic = getAfmDiagnosticStatus();
        out.printf("  afm_diag state=%s reg0004=0x%04X valid=%s blocked=%s error=0x%02X exc=0x%02X seq=%lu\n",
                   diagnosticStateName(diagnostic.state), diagnostic.register0004,
                   diagnostic.valueValid ? "si" : "no",
                   (diagnostic.rateTuningBlocked || isAfmSafetyBlocked()) ? "si" : "no",
                   diagnostic.errorCode, diagnostic.exceptionCode,
                   static_cast<unsigned long>(diagnostic.sequence));
        for (uint8_t id : {uint8_t(0x01), uint8_t(0x02)}) {
            const RS485SlaveStats stats = getSlaveStats(id);
            out.printf("  ID 0x%02X (%s): %s ok=%lu fallo=%lu consecutivos=%lu err=%s(0x%02X) exc=0x%02X "
                       "timeout=%lu crc=%lu incompleta=%lu malformada=%lu busy=%lu inesperada=%lu "
                       "lat=%lums maxlat=%lums rest=%lums minrest=%lums maxattempt=%lums maxsuccess=%lums deadline=%lu, "
                       "next=%ldms\n",
                       id, endpointName(id), stateName(stats.state),
                       static_cast<unsigned long>(stats.successes),
                       static_cast<unsigned long>(stats.failures),
                       static_cast<unsigned long>(stats.consecutiveFailures),
                       errorName(stats.lastErrorCode),
                       stats.lastErrorCode,
                       stats.lastExceptionCode,
                       static_cast<unsigned long>(stats.timeoutErrors),
                       static_cast<unsigned long>(stats.crcErrors),
                       static_cast<unsigned long>(stats.incompleteFrames),
                       static_cast<unsigned long>(stats.malformedFrames),
                       static_cast<unsigned long>(stats.busBusyErrors),
                       static_cast<unsigned long>(stats.unexpectedFrameErrors),
                       static_cast<unsigned long>(stats.lastLatencyMs),
                       static_cast<unsigned long>(stats.maxLatencyMs),
                       static_cast<unsigned long>(stats.lastRestMs),
                       static_cast<unsigned long>(stats.minRestMs == UINT32_MAX ? 0 : stats.minRestMs),
                       static_cast<unsigned long>(stats.maxAttemptGapMs),
                       static_cast<unsigned long>(stats.maxSuccessGapMs),
                       static_cast<unsigned long>(stats.deadlineMisses),
                       static_cast<long>(stats.nextProbeMs - now));
            out.print("    excepciones:");
            for (uint8_t code = 1; code <= 11; ++code) {
                if (stats.modbusExceptions[code] != 0)
                    out.printf(" 0x%02X=%lu", code,
                               static_cast<unsigned long>(stats.modbusExceptions[code]));
            }
            if (stats.otherExceptionErrors != 0)
                out.printf(" other=%lu", static_cast<unsigned long>(stats.otherExceptionErrors));
            out.println();
        }
    }
};

#endif
