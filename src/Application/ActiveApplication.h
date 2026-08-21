#ifndef EOLO_APPLICATION_ACTIVE_APPLICATION_H
#define EOLO_APPLICATION_ACTIVE_APPLICATION_H

#include "../Config/ActiveProfile.h"

#if defined(EOLO_TARGET_DRON)
  #pragma message("Compilando para EOLO Dron headless.")
  #include "DronApplication.h"
  using ActiveApplication = DronApplication;
#elif defined(EOLO_TARGET_STANDARD)
  #pragma message("Compilando para EOLO Standard.")
  #include "UiApplication.h"
  using ActiveApplication = UiApplication;
#elif defined(EOLO_TARGET_EXPRESS_LEGACY)
  #pragma message("Compilando para EOLO Express Legacy.")
  #include "UiApplication.h"
  using ActiveApplication = UiApplication;
#elif defined(EOLO_TARGET_EXPRESS)
  #pragma message("Compilando para EOLO Express.")
  #include "UiApplication.h"
  using ActiveApplication = UiApplication;
#else
  #error "Define un target EOLO_TARGET_* en platformio.ini"
#endif

#ifdef FEATURE_FLOW_AFM07
  #pragma message("Sensor de flujo: AFM07")
#elif defined(FEATURE_FLOW_FS3000)
  #pragma message("Sensor de flujo legacy: FS3000")
#endif

#endif
