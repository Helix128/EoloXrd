#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <Arduino.h>
#include "Wire.h"
#include "../Config/Legacy.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

class I2CBus {
public:
    enum class Result : uint8_t {
        Ok,
        NotReady,
        Busy,
        Nack,
        Timeout,
        ShortRead,
        InvalidArgument,
        BusError
    };

    enum class Command : uint8_t {
        Scan,
        Reset
    };

    struct AddressStats {
        uint8_t address = 0;
        // Evita informar "OK" para una dirección que aún no tuvo ninguna
        // transacción registrada por este wrapper.
        bool observed = false;
        uint8_t consecutiveFailures = 0;
        uint32_t backoffMs = 0;
        uint32_t nextRetryMs = 0;
        uint32_t lastSuccessMs = 0;
        uint32_t lastFailureMs = 0;
        Result lastResult = Result::Ok;
    };

    struct Stats {
        uint32_t transactions = 0;
        uint32_t successes = 0;
        uint32_t failures = 0;
        uint32_t nacks = 0;
        uint32_t timeouts = 0;
        uint32_t shortReads = 0;
        uint32_t recoveries = 0;
        uint32_t lastSuccessMs = 0;
        uint32_t maxTransactionMs = 0;
        uint32_t lastTransactionMs = 0;
        uint8_t lastAddress = 0;
        Result lastResult = Result::Ok;
    };

private:
    bool _ready = false;
    uint8_t _lastScanCount = 0;
    bool _hasScanResult = false;
    bool _lastScanFound[128] = {};
    uint32_t _lastScanCompletedMs = 0;
    SemaphoreHandle_t _mutex = nullptr;
    uint32_t _warmupUntilMs = 0;
    Stats _stats;
    AddressStats _addressStats[128] = {};
    QueueHandle_t _commandQueue = nullptr;
    bool _scanActive = false;
    bool _scanQueued = false;
    uint8_t _scanAddress = 1;
    uint8_t _scanCount = 0;
    bool _scanFound[128] = {};
    uint32_t _scanStartedMs = 0;
    uint32_t _queuedCommandDrops = 0;
    uint32_t _completedCommands = 0;

    static constexpr uint32_t TRANSACTION_TIMEOUT_MS = 20;
    static constexpr uint32_t LOCK_TIMEOUT_MS = 20;

    static Result resultFromTransmissionError(uint8_t error, uint32_t elapsedMs) {
        if (error == 0)
            return Result::Ok;
        if (error == 5 || elapsedMs >= TRANSACTION_TIMEOUT_MS)
            return Result::Timeout;
        if (error == 2 || error == 3)
            return Result::Nack;
        return Result::BusError;
    }

    void clearScanWorkingSet() {
        _scanCount = 0;
        for (uint8_t address = 0; address < 128; ++address)
            _scanFound[address] = false;
    }

    void completeScan() {
        _lastScanCount = _scanCount;
        _hasScanResult = true;
        _lastScanCompletedMs = millis();
        for (uint8_t address = 0; address < 128; ++address)
            _lastScanFound[address] = _scanFound[address];
    }

    void printScanResult(Print& out) const {
        out.printf("[I2C scan] terminado: %u dispositivo(s)",
                   (unsigned int)_lastScanCount);
        if (_lastScanCount != 0) {
            out.print(" [");
            bool first = true;
            for (uint8_t address = 1; address < 127; ++address) {
                if (!_lastScanFound[address])
                    continue;
                if (!first)
                    out.print(", ");
                out.printf("0x%02X", address);
                first = false;
            }
            out.print("]");
        }
        out.println();
    }

    I2CBus() {
        _mutex = xSemaphoreCreateRecursiveMutex();
        _commandQueue = xQueueCreate(8, sizeof(Command));
    }

    I2CBus(const I2CBus&) = delete;
    I2CBus& operator=(const I2CBus&) = delete;

public:
    static I2CBus& getInstance() {
        static I2CBus instance;
        return instance;
    }

    // La espera es finita: una tarea que no obtiene el bus debe degradar esa
    // lectura, nunca congelar el equipo.
    bool lock(uint32_t timeoutMs = LOCK_TIMEOUT_MS) {
        if (!_mutex)
            return true;
        return xSemaphoreTakeRecursive(_mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
    }

    void unlock() {
        if (_mutex)
            xSemaphoreGiveRecursive(_mutex);
    }

    class Guard {
        bool _locked = false;

    public:
        explicit Guard(uint32_t timeoutMs = LOCK_TIMEOUT_MS)
            : _locked(I2CBus::getInstance().lock(timeoutMs)) {}

        ~Guard() {
            if (_locked)
                I2CBus::getInstance().unlock();
        }

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
        bool acquired() const { return _locked; }
    };

    bool begin() {
        Guard guard(LOCK_TIMEOUT_MS);
        if (!guard.acquired())
            return false;
        if (_ready)
            return true;

        _ready = Wire.begin(SDA_PIN, SCL_PIN);
        if (_ready) {
            Wire.setClock(I2C_CLOCK);
            Wire.setTimeOut(TRANSACTION_TIMEOUT_MS);
            LOG_F("I2C iniciado: SDA=%d, SCL=%d, clock=%lu Hz, timeout=%lu ms\n",
                  SDA_PIN, SCL_PIN, (unsigned long)I2C_CLOCK,
                  (unsigned long)TRANSACTION_TIMEOUT_MS);
        } else {
            LOG_LN("I2C no pudo inicializarse");
        }
        return _ready;
    }

    // Algunas librerías de sensores llaman internamente a Wire.begin().
    // Después de cada inicialización se restablecen el reloj y el timeout del
    // perfil para que una librería no cambie silenciosamente la configuración
    // compartida del bus.
    bool applyProfile() {
        Guard guard(LOCK_TIMEOUT_MS);
        if (!guard.acquired() || !_ready)
            return false;
        Wire.setClock(I2C_CLOCK);
        Wire.setTimeOut(TRANSACTION_TIMEOUT_MS);
        return true;
    }

    bool isReady() const {
        return _ready;
    }

    static const char* resultName(Result result) {
        switch (result) {
            case Result::Ok:              return "OK";
            case Result::NotReady:        return "NO_LISTO";
            case Result::Busy:            return "BUS_OCUPADO";
            case Result::Nack:            return "NACK";
            case Result::Timeout:         return "TIMEOUT";
            case Result::ShortRead:       return "LECTURA_CORTA";
            case Result::InvalidArgument: return "ARG_INVALIDO";
            case Result::BusError:        return "ERROR_BUS";
        }
        return "DESCONOCIDO";
    }

    void setWarmupFromNow(uint32_t warmupMs = I2C_WARMUP_MS) {
        _warmupUntilMs = millis() + warmupMs;
    }

    bool warmupComplete() const {
        return (int32_t)(millis() - _warmupUntilMs) >= 0;
    }

    uint32_t warmupRemainingMs() const {
        int32_t remaining = (int32_t)(_warmupUntilMs - millis());
        return remaining > 0 ? (uint32_t)remaining : 0UL;
    }

    bool linesStuck() const {
        return digitalRead(SDA_PIN) == LOW || digitalRead(SCL_PIN) == LOW;
    }

    Stats getStats() const {
        return _stats;
    }

    AddressStats getAddressStats(uint8_t addr) const {
        return addr < 128 ? _addressStats[addr] : AddressStats{};
    }

    bool enqueueCommand(Command command) {
        // No se acumulan scans idénticos: uno activo o en cola ya entrega el
        // mismo diagnóstico y mantener uno basta para no saturar la consola.
        if (command == Command::Scan && (_scanActive || _scanQueued))
            return false;
        if (!_commandQueue || xQueueSend(_commandQueue, &command, 0) != pdTRUE) {
            ++_queuedCommandDrops;
            return false;
        }
        if (command == Command::Scan)
            _scanQueued = true;
        return true;
    }

    uint32_t queuedCommandDrops() const { return _queuedCommandDrops; }
    uint32_t completedCommands() const { return _completedCommands; }
    uint32_t queuedCommands() const {
        return _commandQueue ? (uint32_t)uxQueueMessagesWaiting(_commandQueue) : 0UL;
    }
    bool scanInProgress() const { return _scanActive; }
    bool scanQueued() const { return _scanQueued; }
    uint8_t scanProgressAddress() const { return _scanAddress; }
    bool hasScanResult() const { return _hasScanResult; }
    uint32_t lastScanCompletedMs() const { return _lastScanCompletedMs; }
    bool lastScanFound(uint8_t address) const {
        return address < 128 && _hasScanResult && _lastScanFound[address];
    }

    uint8_t recentFailureCount(uint32_t windowMs = 1000UL) const {
        uint32_t now = millis();
        uint8_t count = 0;
        bool bmePairCounted = false;
        for (uint8_t addr = 1; addr < 127; ++addr) {
            const AddressStats &entry = _addressStats[addr];
            // 0x76/0x77 son dos direcciones alternativas del mismo BME280;
            // no deben contar como dos dispositivos caídos.
            if ((addr == 0x76 || addr == 0x77) && bmePairCounted)
                continue;
            if (entry.consecutiveFailures != 0 &&
                now - entry.lastFailureMs <= windowMs) {
                ++count;
                if (addr == 0x76 || addr == 0x77)
                    bmePairCounted = true;
            }
        }
        return count;
    }

    // Procesa como máximo una operación pequeña por llamada. El worker lo
    // invoca en cada vuelta; así un scan nunca retiene el bus durante cientos
    // de milisegundos ni se ejecuta en el core de la UI.
    void processQueuedCommands() {
        if (!_ready || !warmupComplete())
            return;

        if (_scanActive) {
            processScanStep();
            return;
        }

        Command command;
        if (!_commandQueue || xQueueReceive(_commandQueue, &command, 0) != pdTRUE)
            return;

        if (command == Command::Reset) {
            bool ok = recoverBus();
            ++_completedCommands;
            LOG_F("I2C reset encolado: %s\n", ok ? "OK" : "fallo");
            return;
        }

        _scanQueued = false;
        _scanActive = true;
        _scanAddress = 1;
        clearScanWorkingSet();
        _scanStartedMs = millis();
        Serial.println("[I2C scan] iniciado");
        processScanStep();
    }

    void recordResult(uint8_t addr, Result result, uint32_t elapsedMs,
                      size_t expected = 0, size_t received = 0) {
        _stats.transactions++;
        _stats.lastAddress = addr;
        _stats.lastResult = result;
        _stats.lastTransactionMs = elapsedMs;
        if (elapsedMs > _stats.maxTransactionMs)
            _stats.maxTransactionMs = elapsedMs;

        if (result == Result::Ok) {
            _stats.successes++;
            _stats.lastSuccessMs = millis();
            if (addr < 128) {
                AddressStats &address = _addressStats[addr];
                address.address = addr;
                address.observed = true;
                address.consecutiveFailures = 0;
                address.backoffMs = 0;
                address.nextRetryMs = 0;
                address.lastSuccessMs = millis();
                address.lastResult = Result::Ok;
            }
        } else {
            _stats.failures++;
            if (result == Result::Nack)
                _stats.nacks++;
            if (result == Result::Timeout)
                _stats.timeouts++;
            if (result == Result::ShortRead ||
                (expected != 0 && received != expected))
                _stats.shortReads++;
            if (addr < 128) {
                AddressStats &address = _addressStats[addr];
                address.address = addr;
                address.observed = true;
                if (address.consecutiveFailures < 255)
                    ++address.consecutiveFailures;
                static constexpr uint32_t backoffs[] =
                    {250UL, 500UL, 1000UL, 2000UL, 5000UL};
                uint8_t index = address.consecutiveFailures == 0
                                    ? 0
                                    : (uint8_t)(address.consecutiveFailures - 1);
                if (index >= 5) index = 4;
                address.backoffMs = backoffs[index];
                address.nextRetryMs = millis() + address.backoffMs;
                address.lastFailureMs = millis();
                address.lastResult = result;
            }
        }
    }

    // Recuperación conservadora del periférico y, si hace falta, de un
    // esclavo que dejó SDA retenida. Debe invocarse desde el worker I2C.
    bool recoverBus() {
        Guard guard(LOCK_TIMEOUT_MS);
        if (!guard.acquired())
            return false;

        Wire.end();
        _ready = false;

        pinMode(SDA_PIN, INPUT_PULLUP);
        pinMode(SCL_PIN, INPUT_PULLUP);

        if (digitalRead(SDA_PIN) == LOW) {
            pinMode(SCL_PIN, OUTPUT_OPEN_DRAIN);
            digitalWrite(SCL_PIN, HIGH);
            for (uint8_t i = 0; i < 9 && digitalRead(SDA_PIN) == LOW; ++i) {
                digitalWrite(SCL_PIN, LOW);
                delayMicroseconds(5);
                digitalWrite(SCL_PIN, HIGH);
                delayMicroseconds(5);
            }
        }

        // STOP: SDA baja mientras SCL está alto, después liberar SDA.
        pinMode(SDA_PIN, OUTPUT_OPEN_DRAIN);
        digitalWrite(SDA_PIN, LOW);
        pinMode(SCL_PIN, OUTPUT_OPEN_DRAIN);
        digitalWrite(SCL_PIN, HIGH);
        delayMicroseconds(5);
        digitalWrite(SDA_PIN, HIGH);
        delayMicroseconds(5);
        pinMode(SDA_PIN, INPUT_PULLUP);
        pinMode(SCL_PIN, INPUT_PULLUP);

        bool ok = Wire.begin(SDA_PIN, SCL_PIN);
        if (ok) {
            Wire.setClock(I2C_CLOCK);
            Wire.setTimeOut(TRANSACTION_TIMEOUT_MS);
            _ready = true;
            _stats.recoveries++;
        }
        return ok;
    }

    // Sonda una dirección sin escribir datos. Se usa antes de delegar una
    // inicialización a librerías que acceden Wire directamente, para que el
    // resultado quede contabilizado y un NACK no provoque reintentos opacos.
    Result probe(uint8_t addr, bool logError = false) {
        if (addr == 0 || addr >= 127)
            return Result::InvalidArgument;
        if (!begin())
            return Result::NotReady;
        Guard guard(LOCK_TIMEOUT_MS);
        if (!guard.acquired())
            return Result::Busy;

        uint32_t startMs = millis();
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        uint32_t elapsedMs = millis() - startMs;
        Result result = resultFromTransmissionError(error, elapsedMs);
        recordResult(addr, result, elapsedMs);
        if (result != Result::Ok && logError) {
            LOG_F("I2C probe fallo: addr=0x%02X, error=%u, resultado=%s\n",
                  addr, error, resultName(result));
        }
        return result;
    }

    uint8_t scan() {
        if (!begin())
            return 0;
        Guard guard(LOCK_TIMEOUT_MS);
        if (!guard.acquired())
            return 0;

        clearScanWorkingSet();
        Serial.println("[I2C scan] iniciado");
        uint32_t scanStartMs = millis();
        for (uint8_t addr = 1; addr < 127; addr++) {
            if (millis() - scanStartMs >= 250UL) {
                LOG_LN("Scan I2C abortado por limite de 250 ms");
                break;
            }
            Wire.beginTransmission(addr);
            uint8_t error = Wire.endTransmission();
            if (error == 0) {
                Serial.printf("[I2C scan] encontrado: 0x%02X\n", addr);
                _scanFound[addr] = true;
                _scanCount++;
            } else if (error == 4) {
                Serial.printf("[I2C scan] error desconocido: 0x%02X\n", addr);
            }
        }

        completeScan();
        printScanResult(Serial);
        return _lastScanCount;
    }

private:
    void processScanStep() {
        if (!_scanActive)
            return;

        if ((millis() - _scanStartedMs) >= 250UL || _scanAddress >= 127) {
            if (_scanAddress >= 127) {
                // La última dirección válida es 126; el límite 127 queda
                // excluido por diseño, como en el scanner convencional.
            }
            completeScan();
            _scanActive = false;
            ++_completedCommands;
            printScanResult(Serial);
            return;
        }

        Guard guard(LOCK_TIMEOUT_MS);
        if (!guard.acquired())
            return;
        uint8_t addr = _scanAddress++;
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        if (error == 0) {
            ++_scanCount;
            _scanFound[addr] = true;
            Serial.printf("[I2C scan] encontrado: 0x%02X\n", addr);
        } else if (error == 4) {
            Serial.printf("[I2C scan] error desconocido: 0x%02X\n", addr);
        }
    }

public:

    uint8_t getLastScanCount() const {
        return _lastScanCount;
    }

    bool reset() {
        return recoverBus();
    }

    bool writeCommand(uint8_t addr, uint8_t command, bool logError = true) {
        return writeBytes(addr, &command, 1, logError);
    }

    bool writeBytes(uint8_t addr, const uint8_t* data, size_t len, bool logError = true) {
        if (data == nullptr && len != 0)
            return false;
        if (!begin())
            return false;
        Guard guard(LOCK_TIMEOUT_MS);
        if (!guard.acquired())
            return false;

        uint32_t startMs = millis();
        Wire.beginTransmission(addr);
        for (size_t i = 0; i < len; i++)
            Wire.write(data[i]);

        uint8_t error = Wire.endTransmission();
        uint32_t elapsedMs = millis() - startMs;
        if (error != 0) {
            Result result = resultFromTransmissionError(error, elapsedMs);
            recordResult(addr, result, elapsedMs);
            if (logError) {
                LOG_F("I2C write fallo: addr=0x%02X, len=%u, error=%u\n",
                      addr, (unsigned int)len, error);
            }
            return false;
        }

        recordResult(addr, Result::Ok, elapsedMs);
        return true;
    }

    bool readBytes(uint8_t addr, uint8_t* buffer, size_t len, bool logError = true) {
        return readBytesResult(addr, buffer, len, logError) == Result::Ok;
    }

    Result readBytesResult(uint8_t addr, uint8_t* buffer, size_t len, bool logError = true) {
        if (buffer == nullptr && len != 0)
            return Result::InvalidArgument;
        if (!begin())
            return Result::NotReady;
        Guard guard(LOCK_TIMEOUT_MS);
        if (!guard.acquired())
            return Result::Busy;

        uint32_t startMs = millis();
        size_t readCount = Wire.requestFrom(addr, len);
        uint32_t elapsedMs = millis() - startMs;
        if (readCount != len) {
            Result result = (elapsedMs >= TRANSACTION_TIMEOUT_MS && readCount == 0)
                                ? Result::Timeout
                                : (readCount == 0 ? Result::Nack : Result::ShortRead);
            recordResult(addr, result, elapsedMs, len, readCount);
            if (logError) {
                LOG_F("I2C read corto: addr=0x%02X, esperado=%u, recibido=%u\n",
                      addr, (unsigned int)len, (unsigned int)readCount);
            }
            while (Wire.available())
                Wire.read();
            return result;
        }

        for (size_t i = 0; i < len; i++)
            buffer[i] = Wire.read();

        recordResult(addr, Result::Ok, elapsedMs, len, readCount);
        return Result::Ok;
    }

    bool writeThenRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                       uint8_t* rx, size_t rxLen, bool logError = true) {
        if ((tx == nullptr && txLen != 0) || (rx == nullptr && rxLen != 0))
            return false;
        if (!begin())
            return false;
        Guard guard(LOCK_TIMEOUT_MS);
        if (!guard.acquired())
            return false;

        uint32_t startMs = millis();
        Wire.beginTransmission(addr);
        for (size_t i = 0; i < txLen; i++)
            Wire.write(tx[i]);

        uint8_t error = Wire.endTransmission(false);
        if (error != 0) {
            uint32_t elapsedMs = millis() - startMs;
            Result result = resultFromTransmissionError(error, elapsedMs);
            recordResult(addr, result, elapsedMs);
            if (logError) {
                LOG_F("I2C writeThenRead write fallo: addr=0x%02X, txLen=%u, error=%u\n",
                      addr, (unsigned int)txLen, error);
            }
            return false;
        }

        size_t readCount = Wire.requestFrom(addr, rxLen);
        uint32_t elapsedMs = millis() - startMs;
        if (readCount != rxLen) {
            Result result = (elapsedMs >= TRANSACTION_TIMEOUT_MS && readCount == 0)
                                ? Result::Timeout
                                : (readCount == 0 ? Result::Nack : Result::ShortRead);
            recordResult(addr, result, elapsedMs, rxLen, readCount);
            if (logError) {
                LOG_F("I2C writeThenRead read corto: addr=0x%02X, esperado=%u, recibido=%u\n",
                      addr, (unsigned int)rxLen, (unsigned int)readCount);
            }
            while (Wire.available())
                Wire.read();
            return false;
        }
        for (size_t i = 0; i < rxLen; ++i)
            rx[i] = Wire.read();
        recordResult(addr, Result::Ok, elapsedMs, rxLen, readCount);
        return true;
    }
};

#endif
