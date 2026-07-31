#ifndef RS485_BUS_HPP
#define RS485_BUS_HPP

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "../Config/Legacy.h"
#include "../Utility/RS485Monitor.h"

#define RS485_BAUD_RATE 4800
#define RS485_BUS_GUARD_TIME_MS 20
#define RS485_INTER_FRAME_TIME_US 100

// Códigos compatibles con ModbusMaster para que el monitor y las herramientas
// existentes puedan distinguir timeout y CRC sin depender de esa biblioteca.
enum RS485ErrorCode : uint8_t {
    RS485_OK = 0x00,
    RS485_INVALID_SLAVE = 0xE0,
    RS485_INVALID_FUNCTION = 0xE1,
    RS485_TIMEOUT = 0xE2,
    RS485_INVALID_CRC = 0xE3,
    RS485_MALFORMED = 0xE4
};

enum class RS485EndpointState : uint8_t { Online, Degraded, Offline };

struct RS485SlaveStats {
    uint32_t successes = 0;
    uint32_t failures = 0;
    uint32_t consecutiveFailures = 0;
    uint32_t lastSuccessMs = 0;
    uint32_t lastFailureMs = 0;
    uint32_t nextProbeMs = 0;
    uint32_t lastLatencyMs = 0;
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

    SoftwareSerial _serial;
    SemaphoreHandle_t _initMutex;
    std::atomic<bool> _initialized;
    TaskHandle_t _busTaskHandle = nullptr;
    Endpoint _endpoints[2] = {
        {0x01, 0x0000, 2, 1100, 5000, false, false, 0, nullptr, nullptr},
        {0x02, 0x0000, 1, 800, 800, true, false, 0, nullptr, nullptr}
    };
    RS485SlaveStats _slaveStats[2];
    portMUX_TYPE _statsMux = portMUX_INITIALIZER_UNLOCKED;
    uint8_t _corruptFrameStreak = 0;
    bool _rxStuck = false;

    static constexpr uint32_t TRANSACTION_TIMEOUT_MS = 250;
    static constexpr uint8_t MAX_READ_REGISTERS = 64;
    static constexpr uint8_t RECOVER_AFTER_CORRUPT_FRAMES = 3;
    static constexpr int BUS_TASK_STACK_SIZE = 4096;
    static constexpr UBaseType_t BUS_TASK_PRIORITY = 2;
    static constexpr BaseType_t BUS_TASK_CORE = 0;

    RS485Bus() : _serial(RS485_RX_PIN, RS485_TX_PIN), _initialized(false) {
        _initMutex = xSemaphoreCreateMutex();
    }
    RS485Bus(const RS485Bus&) = delete;
    RS485Bus& operator=(const RS485Bus&) = delete;

    static int trackedSlaveIndex(uint8_t slaveId) {
        return slaveId == 0x01 ? 0 : (slaveId == 0x02 ? 1 : -1);
    }
    static bool due(uint32_t now, uint32_t when) { return (int32_t)(now - when) >= 0; }

    static uint16_t crc16(const uint8_t* data, size_t length) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < length; ++i) {
            crc ^= data[i];
            for (uint8_t bit = 0; bit < 8; ++bit)
                crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
        }
        return crc;
    }

    void setReceiveMode() { digitalWrite(RS485_DE_RE_PIN, LOW); }
    // true indica que RX siguió llegando durante toda la ventana: evidencia de
    // un receptor/bus atascado, distinta de que un esclavo no responda.
    bool drainRx(uint32_t maxMs = 20) {
        const uint32_t started = millis();
        while ((uint32_t)(millis() - started) < maxMs) {
            bool readAny = false;
            while (_serial.available() > 0) { (void)_serial.read(); readAny = true; }
            if (!readAny) return false;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        return _serial.available() > 0;
    }

    void recoverTransport() {
        setReceiveMode();
        drainRx(40);
        _serial.end();
        vTaskDelay(pdMS_TO_TICKS(10));
        _serial.begin(RS485_BAUD_RATE, SWSERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
        drainRx(40);
        _corruptFrameStreak = 0;
        LOG_LN("RS485: transporte reiniciado por tramas corruptas consecutivas");
    }

    bool transact(const Endpoint& endpoint, uint16_t* result, uint8_t& error) {
        uint8_t request[8] = {endpoint.slaveId, 0x03,
                              (uint8_t)(endpoint.startReg >> 8), (uint8_t)endpoint.startReg,
                              0x00, endpoint.count, 0, 0};
        const uint16_t requestCrc = crc16(request, 6);
        request[6] = requestCrc & 0xFF;
        request[7] = requestCrc >> 8;

        // El presupuesto incluye guarda, transmisión y recepción: ningún
        // esclavo ausente puede ocupar el propietario del bus más de 250 ms.
        const uint32_t deadline = millis() + TRANSACTION_TIMEOUT_MS;
        _rxStuck = drainRx(5);
        if (_rxStuck) {
            error = RS485_MALFORMED;
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(RS485_BUS_GUARD_TIME_MS));
        digitalWrite(RS485_DE_RE_PIN, HIGH);
        _serial.write(request, sizeof(request));
        _serial.flush();
        delayMicroseconds(RS485_INTER_FRAME_TIME_US);
        setReceiveMode();

        const uint8_t expectedLength = 5 + endpoint.count * 2;
        uint8_t frame[5 + MAX_READ_REGISTERS * 2];
        uint8_t received = 0;
        while (!due(millis(), deadline)) {
            while (_serial.available() > 0 && received < expectedLength)
                frame[received++] = (uint8_t)_serial.read();
            if (received == expectedLength) break;
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        setReceiveMode();
        if (received == 0) { error = RS485_TIMEOUT; return false; }
        if (received != expectedLength || frame[0] != endpoint.slaveId || frame[1] != 0x03 ||
            frame[2] != endpoint.count * 2) {
            error = RS485_MALFORMED;
            drainRx();
            return false;
        }
        const uint16_t receivedCrc = (uint16_t)frame[expectedLength - 2] |
                                     ((uint16_t)frame[expectedLength - 1] << 8);
        if (crc16(frame, expectedLength - 2) != receivedCrc) {
            error = RS485_INVALID_CRC;
            drainRx();
            return false;
        }
        for (uint8_t i = 0; i < endpoint.count; ++i)
            result[i] = ((uint16_t)frame[3 + i * 2] << 8) | frame[4 + i * 2];
        error = RS485_OK;
        return true;
    }

    void recordResult(const Endpoint& endpoint, bool success, uint8_t error, uint32_t latencyMs) {
        const int index = trackedSlaveIndex(endpoint.slaveId);
        const uint32_t now = millis();
        RS485EndpointState oldState;
        RS485EndpointState newState;
        portENTER_CRITICAL(&_statsMux);
        RS485SlaveStats& stats = _slaveStats[index];
        oldState = stats.state;
        stats.lastLatencyMs = latencyMs;
        if (success) {
            ++stats.successes; stats.consecutiveFailures = 0; stats.lastSuccessMs = now;
            stats.lastErrorCode = RS485_OK; stats.state = RS485EndpointState::Online;
        } else {
            ++stats.failures; ++stats.consecutiveFailures; stats.lastFailureMs = now;
            stats.lastErrorCode = error;
            stats.state = stats.lastSuccessMs == 0 || stats.consecutiveFailures >= 2
                ? RS485EndpointState::Offline : RS485EndpointState::Degraded;
        }
        newState = stats.state;
        portEXIT_CRITICAL(&_statsMux);
        if (oldState != newState) {
            LOG_F("RS485 %s %s\n", endpoint.slaveId == 0x01 ? "Anemometro" : "AFM07",
                  newState == RS485EndpointState::Online ? "online" :
                  (newState == RS485EndpointState::Degraded ? "degradado" : "offline"));
        }
    }

    Endpoint* selectDueEndpoint() {
        const uint32_t now = millis();
        Endpoint* critical = &_endpoints[1];
        Endpoint* optional = &_endpoints[0];
        if (critical->registered && due(now, critical->nextDueMs)) return critical;
        if (optional->registered && due(now, optional->nextDueMs)) return optional;
        return nullptr;
    }

    void pollEndpoint(Endpoint& endpoint) {
        uint16_t registers[MAX_READ_REGISTERS];
        uint8_t error = RS485_TIMEOUT;
        const uint32_t started = millis();
        const bool success = transact(endpoint, registers, error);
        const uint32_t latency = millis() - started;
        recordResult(endpoint, success, error, latency);
        RS485Monitor::getInstance().recordRequestCompleted(success, error, endpoint.slaveId, latency);
        if (success) _corruptFrameStreak = 0;
        else if (error == RS485_INVALID_CRC || error == RS485_MALFORMED) {
            if (++_corruptFrameStreak >= RECOVER_AFTER_CORRUPT_FRAMES) recoverTransport();
        } else _corruptFrameStreak = 0; // un esclavo ausente no es un fallo físico del bus

        endpoint.nextDueMs = millis() + ((!success && !endpoint.critical) ? endpoint.offlineIntervalMs : endpoint.intervalMs);
        portENTER_CRITICAL(&_statsMux);
        _slaveStats[trackedSlaveIndex(endpoint.slaveId)].nextProbeMs = endpoint.nextDueMs;
        portEXIT_CRITICAL(&_statsMux);
        if (endpoint.callback) endpoint.callback(endpoint.context, success, success ? registers : nullptr,
                                                 endpoint.count, error);
        // La recuperación es una operación de mantenimiento posterior, no una
        // ampliación del deadline de la transacción recién terminada.
        if (_rxStuck) {
            _rxStuck = false;
            recoverTransport();
        }
    }

    static void busTask(void* arg) {
        RS485Bus* self = static_cast<RS485Bus*>(arg);
        for (;;) {
            Endpoint* endpoint = self->selectDueEndpoint();
            if (endpoint) self->pollEndpoint(*endpoint);
            else vTaskDelay(pdMS_TO_TICKS(5));
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
            case RS485_OK: return "Exito"; case RS485_TIMEOUT: return "Timeout";
            case RS485_INVALID_CRC: return "CRC invalido"; case RS485_MALFORMED: return "Trama invalida";
            case RS485_INVALID_FUNCTION: return "Funcion invalida"; default: return "Error";
        }
    }

public:
    static RS485Bus& getInstance() { static RS485Bus instance; return instance; }

    void begin() {
        if (xSemaphoreTake(_initMutex, pdMS_TO_TICKS(1000)) != pdTRUE) return;
        if (!_initialized) {
            pinMode(RS485_DE_RE_PIN, OUTPUT); setReceiveMode();
            _serial.begin(RS485_BAUD_RATE, SWSERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
            drainRx(40);
            _initialized = true;
            xTaskCreatePinnedToCore(busTask, "RS485Scheduler", BUS_TASK_STACK_SIZE, this,
                                    BUS_TASK_PRIORITY, &_busTaskHandle, BUS_TASK_CORE);
        }
        xSemaphoreGive(_initMutex);
    }

    // Sólo los dos endpoints conocidos pueden registrarse. Esto evita que una
    // petición ad-hoc vuelva a introducir una cola FIFO y bloquee el AFM07.
    bool registerEndpoint(uint8_t slaveId, uint16_t startReg, uint8_t count,
                          RS485ReadCallback callback, void* context) {
        const int index = trackedSlaveIndex(slaveId);
        if (index < 0 || count == 0 || count > MAX_READ_REGISTERS || callback == nullptr) return false;
        Endpoint& endpoint = _endpoints[index];
        if (endpoint.startReg != startReg || endpoint.count != count) return false;
        endpoint.callback = callback; endpoint.context = context; endpoint.registered = true;
        endpoint.nextDueMs = millis();
        begin();
        return true;
    }

    // La API de cola anterior se conserva como rechazo explícito: los datos se
    // entregan por callback y nunca se aceptan punteros de tareas que expiran.
    bool readRegistersAsync(uint8_t, uint16_t, uint8_t, uint16_t*, uint32_t = TRANSACTION_TIMEOUT_MS) { return false; }
    bool readRegisters(uint8_t, uint16_t, uint8_t, uint16_t*) { return false; }
    uint32_t getPendingRequests() const { return 0; }

    RS485SlaveStats getSlaveStats(uint8_t slaveId) {
        RS485SlaveStats copy; const int index = trackedSlaveIndex(slaveId); if (index < 0) return copy;
        portENTER_CRITICAL(&_statsMux); copy = _slaveStats[index]; portEXIT_CRITICAL(&_statsMux); return copy;
    }
    void resetSlaveStats() {
        portENTER_CRITICAL(&_statsMux); _slaveStats[0] = RS485SlaveStats(); _slaveStats[1] = RS485SlaveStats(); portEXIT_CRITICAL(&_statsMux);
    }
    void printStatus(Print& out) {
        const uint32_t now = millis();
        out.println("RS485: planificador fijo (AFM07 prioritario; anemometro opcional)");
        for (uint8_t id : {uint8_t(0x01), uint8_t(0x02)}) {
            const RS485SlaveStats s = getSlaveStats(id);
            out.printf("  ID 0x%02X (%s): %s ok=%lu fallo=%lu consecutivos=%lu, err=%s, lat=%lums, ultimo ok=",
                       id, id == 1 ? "Anemometro" : "AFM07", stateName(s.state), (unsigned long)s.successes,
                       (unsigned long)s.failures, (unsigned long)s.consecutiveFailures, errorName(s.lastErrorCode),
                       (unsigned long)s.lastLatencyMs);
            if (s.lastSuccessMs) out.printf("hace %lus", (unsigned long)((now - s.lastSuccessMs) / 1000)); else out.print("nunca");
            out.printf(", proxima prueba en %ldms\n", (long)(s.nextProbeMs - now));
        }
    }
};

#endif
