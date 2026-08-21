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

constexpr uint32_t kMinute = 60UL;
constexpr uint32_t kHour = 3600UL;
constexpr uint32_t kDay = 86400UL;
}

#endif
