#ifndef BATTERY_H
#define BATTERY_H

// La fuente de batería forma parte del hardware de la variante. Las dos
// implementaciones mantienen la clase Battery y su API histórica; este
// selector no agrega comportamiento ni adapta valores.
#ifdef FEATURE_DUAL_BATTERY
  #include "BatteryDualI2C.h"
#else
  #include "BatteryAdc.h"
#endif

#endif
