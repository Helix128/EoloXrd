#ifndef EOLO_CONFIG_PROFILE_STANDARD_H
#define EOLO_CONFIG_PROFILE_STANDARD_H

#include "../Types.h"

#define EOLO_DISPLAY_MODEL U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI
#define EOLO_DISPLAY_HW_SPI 1

namespace EoloConfig::Profile
{
inline constexpr BoardConfig kBoard = {
    // La demo probada maneja las bombas con PWM directo: duty 0 = apagado.
    // U8g2 usa 4 MHz por defecto para este SSD1309; se conserva ese valor.
    // El bus principal comparte cableado con ATmega328P; 50 kHz y 5 s de
    // estabilizacion son las condiciones verificadas en Eolo Grande.
    50000UL, 4000000UL, 8, false, 11, 0, 8UL, ModemPowerMode::AlwaysOn,
    27, 15, 2, 18, 23, 300, 5000UL, true
};

inline constexpr SafetyConfig kSafety = {70.0f, 60.0f, 5000UL};
inline constexpr ServiceDefaults kServices = {"gigsky-02", "http://time.cmasccp.cl/"};
inline constexpr SensorConfig kSensors = {100.0f, 5.0f};
inline constexpr FlowPidConfig kFlowPid = {
    100UL, 0.08f, 80.0f, 8.0f, 30.0f, 32, 0.30f, 0.20f,
    6.0f, 1000UL, 1200UL, 1900, 400UL, 0.10f, 8000UL, 1500UL
};
}

#endif
