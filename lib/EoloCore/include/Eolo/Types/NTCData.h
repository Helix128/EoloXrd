#ifndef EOLO_TYPES_NTC_DATA_H
#define EOLO_TYPES_NTC_DATA_H

struct NTCData
{
    int raw = 0;
    float voltage = 0.0f;
    float resistance = 0.0f;
    float temperature = -99.0f;
    bool valid = false;
};

#endif // EOLO_TYPES_NTC_DATA_H
