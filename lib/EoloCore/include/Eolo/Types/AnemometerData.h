#ifndef EOLO_TYPES_ANEMOMETER_DATA_H
#define EOLO_TYPES_ANEMOMETER_DATA_H

#include <stdint.h>

struct AnemometerData
{
    float speed = 0.0f;
    float windKph = 0.0f;
    int direction = 0;
    bool valid = false;
    // Igual que FlowData: permite mostrar una ausencia como "offline" sin
    // bloquear a los consumidores que sólo dependen de valid.
    bool fresh = false;
    bool stale = false;
    uint32_t ageMs = 0;
};

#endif
