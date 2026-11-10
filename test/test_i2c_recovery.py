"""Ejercita recuperación y exclusión del I2CBus real con hardware simulado."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]

# Solo las dependencias del ESP32 se simulan; la lógica del bus es la real.
MOCKS = {
    "Arduino.h": r"""
#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#define LOW 0
#define HIGH 1
#define INPUT_PULLUP 0
#define OUTPUT_OPEN_DRAIN 1
#define SDA_PIN 21
#define SCL_PIN 22
#define LOG_F(...) ((void)0)
#define LOG_LN(...) ((void)0)
inline int lineLevel = HIGH;
inline int lockDepth = 0;
inline int digitalRead(int) { assert(lockDepth > 0); return lineLevel; }
inline void digitalWrite(int, int) {}
inline void pinMode(int, int) {}
inline void delayMicroseconds(int) {}
inline uint32_t fakeNow = 10000;
inline uint32_t millis() { return fakeNow; }
struct Print {
    template<class... T> void printf(const char*, T...) {}
    template<class T> void print(T) {}
    void println() {}
    template<class T> void println(T) {}
};
inline Print Serial;
""",
    "Wire.h": r"""
#pragma once
#include <cstddef>
#include <cstdint>
struct WireMock {
    int ended = 0;
    bool begin(int, int) { return true; }
    void end() { ++ended; }
    void setClock(uint32_t) {}
    void setTimeOut(uint32_t) {}
    void beginTransmission(uint8_t) {}
    uint8_t endTransmission(bool = true) { return 0; }
    size_t write(uint8_t) { return 1; }
    size_t requestFrom(uint8_t, size_t n) { return n; }
    int available() { return 0; }
    int read() { return 0; }
};
inline WireMock Wire;
""",
    "freertos/FreeRTOS.h": r"""
#pragma once
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
""",
    "freertos/semphr.h": r"""
#pragma once
using SemaphoreHandle_t = void*;
inline bool createMutex = true;
inline bool lockAvailable = true;
inline unsigned requestedWait = 999;
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() {
    return createMutex ? reinterpret_cast<void*>(1) : nullptr;
}
inline int xSemaphoreTakeRecursive(SemaphoreHandle_t, unsigned wait) {
    requestedWait = wait;
    if (!lockAvailable) return 0;
    ++lockDepth;
    return 1;
}
inline void xSemaphoreGiveRecursive(SemaphoreHandle_t) { --lockDepth; }
""",
    "freertos/queue.h": r"""
#pragma once
using QueueHandle_t = void*;
inline QueueHandle_t xQueueCreate(int, int) { return nullptr; }
inline int xQueueSend(QueueHandle_t, const void*, int) { return 0; }
inline int xQueueReceive(QueueHandle_t, void*, int) { return 0; }
inline int uxQueueMessagesWaiting(QueueHandle_t) { return 0; }
""",
}

SOURCE = r"""
#include <cstring>
#define EOLO_CONFIG_LEGACY_H
namespace EoloConfig {
constexpr unsigned i2cTransactionTimeoutMs = 20, i2cClockHz = 150000, i2cWarmupMs = 0;
}
#include "src/Common/Board/I2CBus.h"
int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "no-mutex") == 0) {
        createMutex = false;
        auto& bus = I2CBus::getInstance();
        assert(!bus.lock());
        assert(!bus.begin());
        assert(!bus.recoverBus());
        assert(Wire.ended == 0);
        return 0;
    }
    auto& bus = I2CBus::getInstance();
    // Otra tarea usa el bus: no leer líneas, no esperar ni reiniciar.
    lineLevel = LOW;
    lockAvailable = false;
    assert(!bus.recoverBus(true));
    assert(requestedWait == 0 && Wire.ended == 0);
    // Tráfico terminó: líneas libres no justifican recuperación.
    lockAvailable = true;
    lineLevel = HIGH;
    assert(!bus.recoverBus(true));
    assert(Wire.ended == 0);
    // Una línea baja con mutex retenido sí justifica recuperación.
    lineLevel = LOW;
    assert(bus.recoverBus(true));
    assert(Wire.ended == 1 && lockDepth == 0);
    // Reset explícito sigue funcionando aun con líneas libres.
    lineLevel = HIGH;
    assert(bus.recoverBus());
    assert(Wire.ended == 2 && requestedWait == 20 && lockDepth == 0);

    // Un éxito posterior no debe borrar la causa del fallo previo; una
    // transferencia lenta conserva su propia dirección y resultado.
    bus.recordResult(0x08, I2CBus::Result::Timeout, 25, 3, 0, I2CBus::Operation::Read);
    bus.recordResult(0x3c, I2CBus::Result::Ok, 1006);
    bus.recordResult(0x28, I2CBus::Result::Ok, 1, 0, 0, I2CBus::Operation::Probe);
    const auto stats = bus.getStats();
    assert(stats.failures == 1 && stats.timeouts == 1 && stats.shortReads == 1);
    assert(stats.lastFailureAddress == 0x08 && stats.lastFailureResult == I2CBus::Result::Timeout);
    assert(stats.lastFailureOperation == I2CBus::Operation::Read && stats.lastFailureDurationMs == 25);
    assert(stats.maxTransactionMs == 1006 && stats.maxTransactionAddress == 0x3c);
    assert(stats.maxTransactionResult == I2CBus::Result::Ok);
    assert(stats.lastAddress == 0x28 && stats.lastOperation == I2CBus::Operation::Probe);
    assert(bus.recentTransportFailure()); // El éxito posterior no oculta el timeout.
    fakeNow += 1001;
    assert(!bus.recentTransportFailure());
    fakeNow = UINT32_MAX - 500;
    bus.recordResult(0x08, I2CBus::Result::Nack, 2);
    fakeNow = 200;
    assert(bus.recentTransportFailure()); // Desbordamiento de millis().
}
"""


@unittest.skipUnless(shutil.which("c++"), "requiere compilador C++")
class I2CRecoveryTest(unittest.TestCase):
    def test_recovery_and_missing_mutex(self):
        with tempfile.TemporaryDirectory() as directory:
            temp = pathlib.Path(directory)
            for name, source in MOCKS.items():
                path = temp / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(source)
            source_path = temp / "test.cpp"
            source_path.write_text(SOURCE)
            executable = temp / "test"
            subprocess.run(
                ["c++", "-std=c++17", "-I", str(temp), "-I", str(ROOT),
                 str(source_path), "-o", str(executable)],
                check=True, capture_output=True, text=True,
            )
            for args in ([], ["no-mutex"]):
                with self.subTest(args=args):
                    subprocess.run([str(executable), *args], check=True,
                                   capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
