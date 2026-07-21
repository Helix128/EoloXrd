#ifndef EOLO_TYPES_ANEMOMETER_DATA_H
#define EOLO_TYPES_ANEMOMETER_DATA_H

struct AnemometerData
{
    float speed = 0.0f;
    float windKph = 0.0f;
    int direction = 0;
    bool valid = false;
};

#endif
