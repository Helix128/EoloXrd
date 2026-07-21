#ifndef EOLO_CONFIG_PROFILE_DRON_H
#define EOLO_CONFIG_PROFILE_DRON_H

#include "../Types.h"

#define EOLO_DISPLAY_MODEL U8G2_SSD1309_128X64_NONAME2_F_4W_SW_SPI

namespace EoloConfig::Profile
{
inline constexpr BoardConfig kBoard = {
    150000UL, 8000000UL, 8, true, 11, 16, 8UL, ModemPowerMode::AlwaysOn,
    -1, -1, -1, -1, -1, 300, 0UL, false
};
inline constexpr SafetyConfig kSafety = {70.0f, 60.0f, 5000UL};
inline constexpr ServiceDefaults kServices = {"gigsky-02", "http://time.cmasccp.cl/"};
inline constexpr SensorConfig kSensors = {100.0f, 5.0f};
inline constexpr FlowPidConfig kFlowPid = {
    800UL, 0.15f, 28.0f, 0.4f, 6.0f, 8, 0.35f, 0.30f,
    5.0f, 2800UL, 2800UL, 1950, 500UL, 0.15f, 10000UL, 2000UL
};
}

#endif
