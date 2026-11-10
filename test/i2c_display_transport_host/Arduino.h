#pragma once
#include <cstdint>

inline uint32_t mockMillis = 0;
inline unsigned yields = 0;
inline uint32_t millis() { return mockMillis; }
inline void vTaskDelay(uint32_t) { ++yields; }
#define LOG_LN(...) ((void)0)
