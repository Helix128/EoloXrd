#ifndef EOLO_UTILITY_LOG_H
#define EOLO_UTILITY_LOG_H

#include <string.h>
#include "SerialOutput.h"

#ifndef FILENAME
  #define FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define LOG_OUT_F(fmt, ...) do { stdOut.printf(fmt, ##__VA_ARGS__); } while(0)
#define LOG_OUT(...) do { stdOut.print(__VA_ARGS__); } while(0)
#define LOG_OUT_LN(...) do { stdOut.println(__VA_ARGS__); } while(0)

#define LOG_F(fmt, ...) do { \
    LOG_OUT("["); LOG_OUT(FILENAME); LOG_OUT(":"); LOG_OUT(__LINE__); \
    LOG_OUT("]["); LOG_OUT(__func__); LOG_OUT("] "); LOG_OUT_F(fmt, ##__VA_ARGS__); \
} while(0)

#define LOG_LN(msg) do { \
    LOG_OUT("["); LOG_OUT(FILENAME); LOG_OUT(":"); LOG_OUT(__LINE__); \
    LOG_OUT("]["); LOG_OUT(__func__); LOG_OUT("] "); LOG_OUT_LN(msg); \
} while(0)

#define LOG_P(msg) do { \
    LOG_OUT("["); LOG_OUT(FILENAME); LOG_OUT(":"); LOG_OUT(__LINE__); \
    LOG_OUT("]["); LOG_OUT(__func__); LOG_OUT("] "); LOG_OUT(msg); \
} while(0)

#endif
