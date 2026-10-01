#ifndef EOLO_SENSORS_ACTIVE_FLOW_SENSOR_H
#define EOLO_SENSORS_ACTIVE_FLOW_SENSOR_H

// El sensor de flujo es una diferencia de hardware de la variante. Este
// selector sólo elige la implementación; el agregado Components conserva su
// API única mediante ActiveFlowSensor.
#if defined(FEATURE_FLOW_AFM07) && defined(FEATURE_FLOW_FS3000)
  #error "Conflicto: define solo un sensor de flujo"
#elif defined(FEATURE_FLOW_AFM07)
  #include "AFM07.h"
using ActiveFlowSensor = AFM07;
#elif defined(FEATURE_FLOW_FS3000)
  #include "FS3000.h"
using ActiveFlowSensor = FS3K;
#else
  #error "Define explícitamente un sensor de flujo FEATURE_FLOW_*"
#endif

#endif
