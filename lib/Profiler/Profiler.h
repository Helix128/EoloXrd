#ifndef PROFILER_H
#define PROFILER_H

#include <Arduino.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "Config.h"

#ifndef PROFILE_ENABLED
#define PROFILE_ENABLED 1
#endif

#ifndef PROFILE_VERBOSE
#define PROFILE_VERBOSE 0
#endif

#ifndef PROFILE_MAX_SECTIONS
#define PROFILE_MAX_SECTIONS 32
#endif

#ifndef PROFILE_CRITICAL_US
#define PROFILE_CRITICAL_US 100000ULL
#endif

#ifndef PROFILE_ALERT_INTERVAL_MS
#define PROFILE_ALERT_INTERVAL_MS 10000UL
#endif

#if PROFILE_ENABLED

struct ProfilerStats {
    const char* name = nullptr;
    uint32_t count = 0;
    uint64_t totalUs = 0;
    uint64_t lastUs = 0;
    uint64_t maxUs = 0;
    uint64_t minUs = UINT64_MAX;
    uint32_t overThresholdCount = 0;
    unsigned long lastReportMs = 0;
};

class ProfilerRegistry {
private:
    ProfilerStats _stats[PROFILE_MAX_SECTIONS];
    SemaphoreHandle_t _mutex = nullptr;
    bool _verboseAlerts = PROFILE_VERBOSE;

    ProfilerRegistry() {
        _mutex = xSemaphoreCreateMutex();
    }

    bool lock(TickType_t waitTicks = pdMS_TO_TICKS(5)) {
        return _mutex == nullptr || xSemaphoreTake(_mutex, waitTicks) == pdTRUE;
    }

    void unlock() {
        if (_mutex != nullptr) {
            xSemaphoreGive(_mutex);
        }
    }

    int findOrCreate(const char* name) {
        int emptyIndex = -1;
        for (int i = 0; i < PROFILE_MAX_SECTIONS; ++i) {
            if (_stats[i].name == name || (_stats[i].name != nullptr && strcmp(_stats[i].name, name) == 0)) {
                return i;
            }
            if (_stats[i].name == nullptr && emptyIndex < 0) {
                emptyIndex = i;
            }
        }

        if (emptyIndex >= 0) {
            _stats[emptyIndex].name = name;
            _stats[emptyIndex].minUs = UINT64_MAX;
        }
        return emptyIndex;
    }

public:
    static ProfilerRegistry& getInstance() {
        static ProfilerRegistry instance;
        return instance;
    }

    void record(const char* name, uint64_t durationUs, uint64_t thresholdUs = PROFILE_CRITICAL_US) {
        if (name == nullptr || !lock()) {
            return;
        }

        int index = findOrCreate(name);
        if (index < 0) {
            unlock();
            return;
        }

        ProfilerStats& s = _stats[index];
        s.count++;
        s.totalUs += durationUs;
        s.lastUs = durationUs;
        if (durationUs > s.maxUs) s.maxUs = durationUs;
        if (durationUs < s.minUs) s.minUs = durationUs;

        bool shouldAlert = false;
        unsigned long nowMs = millis();
        if (durationUs >= thresholdUs) {
            s.overThresholdCount++;
            if (_verboseAlerts && (nowMs - s.lastReportMs >= PROFILE_ALERT_INTERVAL_MS)) {
                s.lastReportMs = nowMs;
                shouldAlert = true;
            }
        }

        unlock();

        if (shouldAlert) {
            LOG_OUT_F("[prof] %s %.3f ms (threshold %.3f ms)\n",
                             name,
                             durationUs / 1000.0,
                             thresholdUs / 1000.0);
        }
    }

    void reset() {
        if (!lock(pdMS_TO_TICKS(50))) {
            return;
        }
        for (int i = 0; i < PROFILE_MAX_SECTIONS; ++i) {
            _stats[i] = ProfilerStats();
        }
        unlock();
    }

    void setVerboseAlerts(bool enabled) {
        if (!lock(pdMS_TO_TICKS(50))) {
            return;
        }
        _verboseAlerts = enabled;
        unlock();
    }

    bool verboseAlerts() {
        bool enabled = false;
        if (lock()) {
            enabled = _verboseAlerts;
            unlock();
        }
        return enabled;
    }

    uint8_t snapshot(ProfilerStats* out, uint8_t maxCount) {
        if (out == nullptr || maxCount == 0 || !lock(pdMS_TO_TICKS(50))) {
            return 0;
        }

        uint8_t count = 0;
        for (int i = 0; i < PROFILE_MAX_SECTIONS && count < maxCount; ++i) {
            if (_stats[i].name != nullptr && _stats[i].count > 0) {
                out[count++] = _stats[i];
            }
        }
        unlock();
        return count;
    }

    void printSummary(Print& out, bool sortByTotal = false) {
        ProfilerStats copy[PROFILE_MAX_SECTIONS];
        uint8_t count = snapshot(copy, PROFILE_MAX_SECTIONS);

        for (uint8_t i = 0; i < count; ++i) {
            for (uint8_t j = i + 1; j < count; ++j) {
                uint64_t left = sortByTotal ? copy[i].totalUs : copy[i].maxUs;
                uint64_t right = sortByTotal ? copy[j].totalUs : copy[j].maxUs;
                if (right > left) {
                    ProfilerStats tmp = copy[i];
                    copy[i] = copy[j];
                    copy[j] = tmp;
                }
            }
        }

        out.println("name count avg_ms max_ms last_ms over");
        for (uint8_t i = 0; i < count; ++i) {
            const ProfilerStats& s = copy[i];
            double avgMs = s.count > 0 ? ((double)s.totalUs / (double)s.count) / 1000.0 : 0.0;
            out.printf("%s %lu %.3f %.3f %.3f %lu\n",
                       s.name,
                       (unsigned long)s.count,
                       avgMs,
                       s.maxUs / 1000.0,
                       s.lastUs / 1000.0,
                       (unsigned long)s.overThresholdCount);
        }
    }

    void printSummary(bool sortByTotal = false) {
        printSummary(stdOut, sortByTotal);
    }
};

class Profiler {
private:
    const char* _name;
    int64_t _startUs;
    bool _done = false;

public:
    explicit Profiler(const char* name)
        : _name(name), _startUs(esp_timer_get_time()) {}

    ~Profiler() {
        end();
    }

    void end() {
        if (_done) {
            return;
        }
        _done = true;
        int64_t duration = esp_timer_get_time() - _startUs;
        if (duration >= 0) {
            ProfilerRegistry::getInstance().record(_name, (uint64_t)duration);
        }
    }
};

#define PROFILE_CONCAT_INNER(a, b) a##b
#define PROFILE_CONCAT(a, b) PROFILE_CONCAT_INNER(a, b)
#define PROFILE_SCOPE(name) Profiler PROFILE_CONCAT(_profilerScope_, __COUNTER__)(name)
#define PROFILE_MARK(name, durationUs) ProfilerRegistry::getInstance().record((name), (uint64_t)(durationUs))

#else

struct ProfilerStats {};

class ProfilerRegistry {
public:
    static ProfilerRegistry& getInstance() {
        static ProfilerRegistry instance;
        return instance;
    }
    void record(const char*, uint64_t, uint64_t = 0) {}
    void reset() {}
    void setVerboseAlerts(bool) {}
    bool verboseAlerts() { return false; }
    uint8_t snapshot(ProfilerStats*, uint8_t) { return 0; }
    void printSummary(Print& out, bool = false) {
        out.println("profiling disabled");
    }
    void printSummary(bool = false) {
        printSummary(stdOut);
    }
};

class Profiler {
public:
    explicit Profiler(const char*) {}
    void end() {}
};

#define PROFILE_SCOPE(name) do {} while (0)
#define PROFILE_MARK(name, durationUs) do { (void)(durationUs); } while (0)

#endif

#endif
