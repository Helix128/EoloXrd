#ifndef EOLO_BOARD_PINOUT_H
#define EOLO_BOARD_PINOUT_H

#define EOLO_PIN_UNUSED -1

#define EOLO_INPUT_ONLY_PIN(pin) ((pin) >= 34 && (pin) <= 39)
#define EOLO_VALID_PIN(pin) ((pin) >= 0)
#define EOLO_SAME_USED_PIN(a, b) (EOLO_VALID_PIN(a) && EOLO_VALID_PIN(b) && ((a) == (b)))

#define EXPRESS_REV1_ALT 1

#if (defined(EOLO_TARGET_DRON) + defined(EOLO_TARGET_DRON_LEGACY) + \
     defined(EOLO_TARGET_STANDARD) + \
     defined(EOLO_TARGET_EXPRESS) + defined(EOLO_TARGET_EXPRESS_LEGACY)) != 1
  #error "Define exactamente un target EOLO_TARGET_* antes de incluir Board/Pinout.h"
#endif

#if defined(EOLO_PINOUT) && !defined(EOLO_TARGET_EXPRESS_LEGACY)
  #error "EOLO_PINOUT solo se admite para EOLO_TARGET_EXPRESS_LEGACY"
#endif

#if defined(EOLO_PINOUT) && EOLO_PINOUT != EXPRESS_REV1_ALT
  #error "EOLO_PINOUT no reconocido para EOLO Express Rev.1"
#endif

#if defined(EOLO_TARGET_DRON)
  #include "Pinouts/Dron.h"
#elif defined(EOLO_TARGET_DRON_LEGACY)
  #include "Pinouts/DronLegacy.h"
#elif defined(EOLO_TARGET_STANDARD)
  #include "Pinouts/Standard.h"
#elif defined(EOLO_TARGET_EXPRESS_LEGACY)
  #if defined(EOLO_PINOUT) && EOLO_PINOUT == EXPRESS_REV1_ALT
    #include "Pinouts/ExpressLegacyAlt.h"
  #else
    #include "Pinouts/ExpressLegacy.h"
  #endif
#elif defined(EOLO_TARGET_EXPRESS)
  #include "Pinouts/Express.h"
#else
  #error "Define un target EOLO_TARGET_* antes de incluir Board/Pinout.h"
#endif

#include "PinoutValidation.h"

#endif
