#ifndef EOLO_TYPES_PLANTOWER_DATA_H
#define EOLO_TYPES_PLANTOWER_DATA_H

#include <stdint.h>

struct PlantowerData
{
    uint16_t pm1_0 = 0;
    uint16_t pm2_5 = 0;
    uint16_t pm10_0 = 0;
    bool valid = false;
};

#endif
