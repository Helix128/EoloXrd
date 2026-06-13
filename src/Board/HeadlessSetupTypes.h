#ifndef HEADLESS_SETUP_TYPES_H
#define HEADLESS_SETUP_TYPES_H

#include <Arduino.h>
#include "../Config.h"
#include <Eolo/Types/HeadlessSetupTypes.h>

namespace HeadlessSetup
{
  inline bool isSafeLogBasename(const String &name)
  {
    return isSafeLogBasename(name.c_str());
  }

  inline bool isSafePresetName(const String &name)
  {
    return isSafePresetName(name.c_str());
  }
}

#endif
