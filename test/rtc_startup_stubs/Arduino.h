#ifndef RTC_STARTUP_TEST_ARDUINO_H
#define RTC_STARTUP_TEST_ARDUINO_H
#include <cstdint>
#include <cmath>
#include <string>
using std::isfinite;

class String {
    std::string value;
public:
    String(const char *text = "") : value(text) {}
    const char *c_str() const { return value.c_str(); }
};

inline uint32_t fakeMillis = 0;
inline uint32_t millis() { return fakeMillis; }
inline void delay(unsigned long) {}
#define LOG_LN(...) ((void)0)
#define LOG_F(...) ((void)0)
#endif
