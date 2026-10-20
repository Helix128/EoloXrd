#ifndef EOLO_CONFIG_TYPES_H
#define EOLO_CONFIG_TYPES_H

#include <stdint.h>
namespace EoloConfig
{
enum class ModemPowerMode : uint8_t
{
    AlwaysOn = 1,
    OnDemand = 2
};

// LegacyDivider: divisor 7.8:1 con compensación empírica (BatteryMath).
// Pack2sLiIon: pack 18650 2S con divisor 3.2:1 y curva de descarga propia.
enum class BatteryModel : uint8_t
{
    LegacyDivider = 1,
    Pack2sLiIon = 2
};

constexpr uint32_t kMinute = 60UL;
constexpr uint32_t kHour = 3600UL;
constexpr uint32_t kDay = 86400UL;
// Duración de captura sin fin (Dron).
constexpr uint32_t kDurationInfinite = UINT32_MAX;
}

#endif
