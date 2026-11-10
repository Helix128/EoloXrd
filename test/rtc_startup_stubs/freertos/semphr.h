#ifndef RTC_STARTUP_TEST_SEMPHR_H
#define RTC_STARTUP_TEST_SEMPHR_H
using SemaphoreHandle_t = void *;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return reinterpret_cast<void *>(1); }
inline int xSemaphoreTake(SemaphoreHandle_t, unsigned) { return 1; }
inline void xSemaphoreGive(SemaphoreHandle_t) {}
#endif
