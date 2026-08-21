#ifndef EOLO_TYPES_PLANTOWER_DATA_H
#define EOLO_TYPES_PLANTOWER_DATA_H

#include <stdint.h>

struct PlantowerData
{
    uint16_t pm1_0 = 0;
    uint16_t pm2_5 = 0;
    uint16_t pm10_0 = 0;
    // PMS5003T: temperatura en décimas de °C (con signo) y humedad en
    // décimas de %. Se conservan ya convertidas para los consumidores.
    float temperature = 0.0f;
    float humidity = 0.0f;
    bool valid = false;
};

#endif
