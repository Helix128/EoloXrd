#ifndef EOLO_TYPES_BME280_DATA_H
#define EOLO_TYPES_BME280_DATA_H

struct BME280Data
{
    float temperature = 0.0f;
    float humidity = 0.0f;
    float pressure = 0.0f;
    bool valid = false;
};

#endif // EOLO_TYPES_BME280_DATA_H
