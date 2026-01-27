#ifndef PROFILER_H
#define PROFILER_H  

#include "Arduino.h"

class Profiler {
    const char* name;
    int64_t start;
    bool done = false;
public:
    Profiler(const char* funcName) : name(funcName), start(esp_timer_get_time()) {}
    ~Profiler() {
        if(done) return;
        int64_t duration = esp_timer_get_time() - start;
        if (duration > 1000) {
            if (duration >= 1000000) {
            Serial.printf("%s: %.2f s\n", name, duration / 1000000.0f);
            } else if (duration >= 1000) {
            Serial.printf("%s: %.2f ms\n", name, duration / 1000.0f);
            } else {
            Serial.printf("%s: %lld us\n", name, duration);
            }
        }
    }

    void end() {
        int64_t duration = esp_timer_get_time() - start;
        if (duration > 1000) {
            if (duration >= 1000000) {
            Serial.printf("%s: %.2f s\n", name, duration / 1000000.0f);
            } else if (duration >= 1000) {
            Serial.printf("%s: %.2f ms\n", name, duration / 1000.0f);
            } else {
            Serial.printf("%s: %lld us\n", name, duration);
            }
        }
        done = true;
    }
};

#endif