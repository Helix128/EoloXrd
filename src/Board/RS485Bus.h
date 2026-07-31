#ifndef RS485_BUS_HPP
#define RS485_BUS_HPP

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <atomic>
#include <Eolo/Core/Communication/RS485Protocol.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "../Config/Legacy.h"
#include "../Utility/RS485Monitor.h"

#define RS485_BAUD_RATE 4800

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
    RS485_UNEXPECTED_FRAME = 0xE7
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
    uint32_t deadlineMisses = 0;
    uint32_t lateBytes = 0;
    uint32_t unexpectedFrames = 0;
    uint8_t lastErrorCode = RS485_OK;
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

    SoftwareSerial _serial;
    SemaphoreHandle_t _initMutex;
    SemaphoreHandle_t _endpointMutex;
    std::atomic<bool> _initialized;
    TaskHandle_t _busTaskHandle = nullptr;
    Endpoint _endpoints[2] = {
        {0x01, 0x0000, 2, EoloCore::RS485TimingModel::kAnemometerIntervalMs,
         EoloCore::RS485TimingModel::kAnemometerOfflineIntervalMs, false, false, 0, nullptr, nullptr},
        {0x02, 0x0000, 1, EoloCore::RS485TimingModel::kAfmIntervalMs,
         EoloCore::RS485TimingModel::kAfmIntervalMs, true, false, 0, nullptr, nullptr}
    };
    RS485SlaveStats _slaveStats[2];
    portMUX_TYPE _statsMux = portMUX_INITIALIZER_UNLOCKED;

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
            default: return "Error";
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
                  uint32_t& protocolLateBytes, uint32_t& protocolUnexpectedFrames) {
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
                    if (parsed.status == EoloCore::ModbusReadStatus::Exception)
                        error = RS485_EXCEPTION;
                    else
                        error = errorForProtocol(parsed.status);
                    return false;
                }
            }

            const uint32_t nowMs = millis();
            if (frameLength == 0) {
                if (due(nowMs, responseDeadlineMs)) {
                    error = RS485_TIMEOUT;
                    return false;
                }
            } else {
                if (static_cast<uint32_t>(micros() - lastByteUs) >= EoloCore::RS485TimingModel::kBusQuietUs ||
                    static_cast<uint32_t>(nowMs - frameStartedMs) >=
                        EoloCore::RS485TimingModel::kFrameCompletionTimeoutMs) {
                    error = RS485_MALFORMED;
                    return false;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    void recordResult(const Endpoint& endpoint, bool success, uint8_t error,
                      uint32_t startedMs, uint32_t latencyMs,
                      uint32_t lateBytes, uint32_t unexpectedFrames) {
        const int index = trackedSlaveIndex(endpoint.slaveId);
        if (index < 0) return;
        const uint32_t now = millis();
        RS485EndpointState oldState;
        RS485EndpointState newState;
        portENTER_CRITICAL(&_statsMux);
        RS485SlaveStats& stats = _slaveStats[index];
        oldState = stats.state;
        if (stats.lastAttemptMs != 0) {
            const uint32_t gap = startedMs - stats.lastAttemptMs;
            if (gap > stats.maxAttemptGapMs) stats.maxAttemptGapMs = gap;
        }
        stats.lastAttemptMs = startedMs;
        stats.lastLatencyMs = latencyMs;
        if (latencyMs > stats.maxLatencyMs) stats.maxLatencyMs = latencyMs;
        stats.lateBytes += lateBytes;
        stats.unexpectedFrames += unexpectedFrames;
        if (success) {
            ++stats.successes;
            if (stats.lastSuccessMs != 0) {
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
            stats.state = stats.lastSuccessMs == 0 || stats.consecutiveFailures >= 2
                ? RS485EndpointState::Offline : RS485EndpointState::Degraded;
        }
        newState = stats.state;
        portEXIT_CRITICAL(&_statsMux);

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

    void scheduleNext(int index, const Endpoint& endpoint, uint32_t startedMs, bool success) {
        if (index < 0) return;
        uint32_t nextDue;
        if (!success && !endpoint.critical) {
            nextDue = startedMs + endpoint.offlineIntervalMs;
        } else {
            nextDue = EoloCore::RS485TimingModel::nextPeriodicDue(
                endpoint.nextDueMs, startedMs, endpoint.intervalMs);
        }
        if (xSemaphoreTake(_endpointMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            _endpoints[index].nextDueMs = nextDue;
            xSemaphoreGive(_endpointMutex);
        }
        portENTER_CRITICAL(&_statsMux);
        _slaveStats[index].nextProbeMs = nextDue;
        portEXIT_CRITICAL(&_statsMux);
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
        uint32_t lateBytes = 0;
        uint32_t unexpectedFrames = 0;
        const uint32_t started = millis();
        noteDeadlineMiss(index, started, endpoint.nextDueMs);
        const bool success = transact(endpoint, registers, error, lateBytes, unexpectedFrames);
        const uint32_t latency = millis() - started;
        recordResult(endpoint, success, error, started, latency, lateBytes, unexpectedFrames);
        RS485Monitor::getInstance().recordRequestCompleted(success, error, endpoint.slaveId, latency);

        if (success) _corruptFrameStreak = 0;
        else if (error == RS485_INVALID_CRC || error == RS485_MALFORMED || error == RS485_BUS_BUSY) {
            if (++_corruptFrameStreak >= RECOVER_AFTER_CORRUPT_FRAMES) recoverTransport();
        } else if (error != RS485_TIMEOUT && error != RS485_EXCEPTION) {
            _corruptFrameStreak = 0;
        }

        scheduleNext(index, endpoint, started, success);
        if (endpoint.callback) {
            endpoint.callback(endpoint.context, success, success ? registers : nullptr,
                               endpoint.count, error);
        }
    }

    static void busTask(void* arg) {
        RS485Bus* self = static_cast<RS485Bus*>(arg);
        for (;;) {
            Endpoint endpoint{};
            int index = -1;
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
        out.printf("  bus_busy=%lu recoveries=%lu late_bytes=%lu unexpected=%lu\n",
                   static_cast<unsigned long>(_busBusyDeferrals.load(std::memory_order_relaxed)),
                   static_cast<unsigned long>(_busRecoveries.load(std::memory_order_relaxed)),
                   static_cast<unsigned long>(_lateBytes.load(std::memory_order_relaxed)),
                   static_cast<unsigned long>(_unexpectedFrames.load(std::memory_order_relaxed)));
        const RS485Stats monitor = RS485Monitor::getInstance().getStats();
        out.printf("  transactions=%lu ok=%lu fail=%lu timeout=%lu crc=%lu malformed=%lu "
                   "exception=%lu\n",
                   static_cast<unsigned long>(monitor.totalRequests),
                   static_cast<unsigned long>(monitor.successfulReads),
                   static_cast<unsigned long>(monitor.failedReads),
                   static_cast<unsigned long>(monitor.timeoutErrors),
                   static_cast<unsigned long>(monitor.crcErrors),
                   static_cast<unsigned long>(monitor.malformedErrors),
                   static_cast<unsigned long>(monitor.exceptionErrors));
        for (uint8_t id : {uint8_t(0x01), uint8_t(0x02)}) {
            const RS485SlaveStats stats = getSlaveStats(id);
            out.printf("  ID 0x%02X (%s): %s ok=%lu fallo=%lu consecutivos=%lu err=%s, "
                       "lat=%lums maxlat=%lums gap=%lums maxokgap=%lums deadline=%lu, "
                       "next=%ldms\n",
                       id, endpointName(id), stateName(stats.state),
                       static_cast<unsigned long>(stats.successes),
                       static_cast<unsigned long>(stats.failures),
                       static_cast<unsigned long>(stats.consecutiveFailures),
                       errorName(stats.lastErrorCode),
                       static_cast<unsigned long>(stats.lastLatencyMs),
                       static_cast<unsigned long>(stats.maxLatencyMs),
                       static_cast<unsigned long>(stats.maxAttemptGapMs),
                       static_cast<unsigned long>(stats.maxSuccessGapMs),
                       static_cast<unsigned long>(stats.deadlineMisses),
                       static_cast<long>(stats.nextProbeMs - now));
        }
    }
};

#endif
