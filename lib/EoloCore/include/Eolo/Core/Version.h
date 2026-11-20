#ifndef EOLO_CORE_VERSION_H
#define EOLO_CORE_VERSION_H

#ifndef EOLO_FIRMWARE_VERSION
#define EOLO_FIRMWARE_VERSION "dev"
#endif

namespace EoloCore
{
    inline constexpr const char *kFirmwareVersion = EOLO_FIRMWARE_VERSION;
}

#endif
