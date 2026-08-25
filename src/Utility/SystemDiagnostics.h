#ifndef EOLO_SYSTEM_DIAGNOSTICS_H
#define EOLO_SYSTEM_DIAGNOSTICS_H

#include <Arduino.h>
#include <atomic>
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

struct EoloCrashLedger
{
    uint32_t magic;
    uint32_t checksum;
    uint32_t loopHeartbeat;
    uint32_t i2cHeartbeat;
    uint32_t rs485Heartbeat;
    uint32_t loopBeatMs;
    uint32_t i2cBeatMs;
    uint32_t rs485BeatMs;
    uint32_t savedAtMs;
    char phase[24];
    char request[48];
};

RTC_DATA_ATTR inline EoloCrashLedger gEoloCrashLedger{};

class SystemDiagnostics
{
public:
    struct Snapshot
    {
        uint32_t uptimeMs = 0;
        uint32_t loopHeartbeat = 0;
        uint32_t loopLastDurationMs = 0;
        uint32_t loopMaxPauseMs = 0;
        uint32_t i2cHeartbeat = 0;
        uint32_t i2cStackWords = 0;
        uint32_t rs485Heartbeat = 0;
        uint32_t rs485StackWords = 0;
        uint32_t freeHeap = 0;
        uint32_t minFreeHeap = 0;
        uint32_t httpLastDurationMs = 0;
        uint32_t httpSlow = 0;
        uint32_t httpFailed = 0;
        int httpLastCode = 0;
        uint8_t core = 0;
        char phase[24] = {};
        char lastRequest[48] = {};
    };

    static SystemDiagnostics &instance()
    {
        static SystemDiagnostics value;
        return value;
    }

    static const char *resetReasonName(esp_reset_reason_t reason)
    {
        switch (reason) {
        case ESP_RST_POWERON: return "power_on";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt_watchdog";
        case ESP_RST_TASK_WDT: return "task_watchdog";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_DEEPSLEEP: return "deep_sleep";
        default: return "other";
        }
    }

    void begin()
    {
        const esp_reset_reason_t reason = esp_reset_reason();
        Serial.printf("Reset ESP32: %s (%d)\n", resetReasonName(reason), (int)reason);
        if (ledgerValid() && (reason == ESP_RST_TASK_WDT || reason == ESP_RST_INT_WDT ||
                             reason == ESP_RST_WDT || reason == ESP_RST_PANIC)) {
            Serial.printf("Ledger previo: fase=%s request=%s loop=%lu i2c=%lu rs485=%lu guardado_ms=%lu\n",
                          gEoloCrashLedger.phase, gEoloCrashLedger.request,
                          (unsigned long)gEoloCrashLedger.loopHeartbeat,
                          (unsigned long)gEoloCrashLedger.i2cHeartbeat,
                          (unsigned long)gEoloCrashLedger.rs485Heartbeat,
                          (unsigned long)gEoloCrashLedger.savedAtMs);
            const uint32_t loopAge = gEoloCrashLedger.savedAtMs - gEoloCrashLedger.loopBeatMs;
            const uint32_t i2cAge = gEoloCrashLedger.savedAtMs - gEoloCrashLedger.i2cBeatMs;
            Serial.printf("Ledger previo: edad_loop=%lu ms edad_i2c=%lu ms tarea_probable=%s\n",
                          (unsigned long)loopAge, (unsigned long)i2cAge,
                          loopAge >= 10000UL ? "loop" : (i2cAge >= 10000UL ? "i2c" : "indeterminada"));
        }
        esp_task_wdt_init(15, true);
        esp_task_wdt_add(nullptr);
        setPhase("boot");
        persistLedger(true);
    }

    void registerCurrentTask() { esp_task_wdt_add(nullptr); }
    void feedWatchdog() { esp_task_wdt_reset(); }
#ifdef EOLO_WDT_FAULT_INJECTION
    void requestI2cHang() { _injectI2cHang = true; }
    bool consumeI2cHang() { return _injectI2cHang.exchange(false); }
#endif

    void loopBeat(uint32_t durationMs)
    {
        const uint32_t now = millis();
        const uint32_t previous = _lastLoopMs.exchange(now);
        if (previous != 0) updateMaximum(_loopMaxPauseMs, now - previous);
        _loopLastDurationMs = durationMs;
        ++_loopHeartbeat;
        persistLedger(false);
    }

    void i2cBeat()
    {
        _lastI2cMs = millis();
        ++_i2cHeartbeat;
        _i2cStackWords = uxTaskGetStackHighWaterMark(nullptr);
        persistLedger(false);
    }

    void rs485Beat()
    {
        _lastRs485Ms = millis();
        ++_rs485Heartbeat;
        _rs485StackWords = uxTaskGetStackHighWaterMark(nullptr);
    }

    void setPhase(const char *phase)
    {
        portENTER_CRITICAL(&_textMux);
        strlcpy(_phase, phase ? phase : "", sizeof(_phase));
        portEXIT_CRITICAL(&_textMux);
    }

    void httpBegin(const String &uri)
    {
        _httpStartedMs = millis();
        portENTER_CRITICAL(&_textMux);
        strlcpy(_lastRequest, uri.c_str(), sizeof(_lastRequest));
        portEXIT_CRITICAL(&_textMux);
        setPhase("http");
    }

    void httpEnd(int code)
    {
        const uint32_t duration = millis() - _httpStartedMs.load();
        _httpLastDurationMs = duration;
        _httpLastCode = code;
        if (duration >= 1000UL) ++_httpSlow;
        if (code >= 400) ++_httpFailed;
        setPhase("loop");
        persistLedger(true);
    }

    Snapshot snapshot() const
    {
        Snapshot out;
        out.uptimeMs = millis();
        out.loopHeartbeat = _loopHeartbeat.load();
        out.loopLastDurationMs = _loopLastDurationMs.load();
        out.loopMaxPauseMs = _loopMaxPauseMs.load();
        out.i2cHeartbeat = _i2cHeartbeat.load();
        out.i2cStackWords = _i2cStackWords.load();
        out.rs485Heartbeat = _rs485Heartbeat.load();
        out.rs485StackWords = _rs485StackWords.load();
        out.freeHeap = ESP.getFreeHeap();
        out.minFreeHeap = ESP.getMinFreeHeap();
        out.httpLastDurationMs = _httpLastDurationMs.load();
        out.httpSlow = _httpSlow.load();
        out.httpFailed = _httpFailed.load();
        out.httpLastCode = _httpLastCode.load();
        out.core = xPortGetCoreID();
        portENTER_CRITICAL(&_textMux);
        strlcpy(out.phase, _phase, sizeof(out.phase));
        strlcpy(out.lastRequest, _lastRequest, sizeof(out.lastRequest));
        portEXIT_CRITICAL(&_textMux);
        return out;
    }

    void print(Print &out) const
    {
        Snapshot s = snapshot();
        out.printf("system uptime=%lu heap=%lu min_heap=%lu core=%u fase=%s\n",
                   (unsigned long)s.uptimeMs, (unsigned long)s.freeHeap,
                   (unsigned long)s.minFreeHeap, s.core, s.phase);
        out.printf("loop heartbeat=%lu duracion=%lu ms pausa_max=%lu ms | i2c heartbeat=%lu stack=%lu | rs485 heartbeat=%lu stack=%lu\n",
                   (unsigned long)s.loopHeartbeat, (unsigned long)s.loopLastDurationMs,
                   (unsigned long)s.loopMaxPauseMs, (unsigned long)s.i2cHeartbeat,
                   (unsigned long)s.i2cStackWords, (unsigned long)s.rs485Heartbeat,
                   (unsigned long)s.rs485StackWords);
        out.printf("http request=%s codigo=%d duracion=%lu ms lentas=%lu fallidas=%lu\n",
                   s.lastRequest, s.httpLastCode, (unsigned long)s.httpLastDurationMs,
                   (unsigned long)s.httpSlow, (unsigned long)s.httpFailed);
    }

private:
    static constexpr uint32_t LedgerMagic = 0x454F4C4FUL;
    mutable portMUX_TYPE _textMux = portMUX_INITIALIZER_UNLOCKED;
    portMUX_TYPE _ledgerMux = portMUX_INITIALIZER_UNLOCKED;
    char _phase[24] = "startup";
    char _lastRequest[48] = "";
    std::atomic_uint32_t _loopHeartbeat{0}, _lastLoopMs{0}, _loopLastDurationMs{0}, _loopMaxPauseMs{0};
    std::atomic_uint32_t _i2cHeartbeat{0}, _i2cStackWords{0}, _rs485Heartbeat{0}, _rs485StackWords{0};
    std::atomic_uint32_t _lastI2cMs{0}, _lastRs485Ms{0};
    std::atomic_uint32_t _httpStartedMs{0}, _httpLastDurationMs{0}, _httpSlow{0}, _httpFailed{0};
    std::atomic_int _httpLastCode{0};
    std::atomic_uint32_t _lastLedgerMs{0};
#ifdef EOLO_WDT_FAULT_INJECTION
    std::atomic_bool _injectI2cHang{false};
#endif

    static void updateMaximum(std::atomic_uint32_t &target, uint32_t value)
    {
        uint32_t old = target.load();
        while (value > old && !target.compare_exchange_weak(old, value)) {}
    }

    static uint32_t checksum(const EoloCrashLedger &ledger)
    {
        const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&ledger);
        uint32_t sum = 2166136261UL;
        for (size_t i = offsetof(EoloCrashLedger, loopHeartbeat); i < sizeof(ledger); ++i)
            sum = (sum ^ bytes[i]) * 16777619UL;
        return sum;
    }

    bool ledgerValid() const
    {
        return gEoloCrashLedger.magic == LedgerMagic &&
               gEoloCrashLedger.checksum == checksum(gEoloCrashLedger);
    }

    void persistLedger(bool force)
    {
        const uint32_t now = millis();
        portENTER_CRITICAL(&_ledgerMux);
        uint32_t last = _lastLedgerMs.load();
        if (!force && now - last < 1000UL) {
            portEXIT_CRITICAL(&_ledgerMux);
            return;
        }
        _lastLedgerMs = now;
        portEXIT_CRITICAL(&_ledgerMux);
        EoloCrashLedger next{};
        next.magic = LedgerMagic;
        next.loopHeartbeat = _loopHeartbeat.load();
        next.i2cHeartbeat = _i2cHeartbeat.load();
        next.rs485Heartbeat = _rs485Heartbeat.load();
        next.loopBeatMs = _lastLoopMs.load();
        next.i2cBeatMs = _lastI2cMs.load();
        next.rs485BeatMs = _lastRs485Ms.load();
        next.savedAtMs = now;
        portENTER_CRITICAL(&_textMux);
        strlcpy(next.phase, _phase, sizeof(next.phase));
        strlcpy(next.request, _lastRequest, sizeof(next.request));
        portEXIT_CRITICAL(&_textMux);
        next.checksum = checksum(next);
        portENTER_CRITICAL(&_ledgerMux);
        gEoloCrashLedger = next;
        portEXIT_CRITICAL(&_ledgerMux);
    }
};

#endif
