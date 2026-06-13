#ifndef EOLO_TYPES_FLOW_DATA_H
#define EOLO_TYPES_FLOW_DATA_H

#include <stdint.h>

struct FlowData
{
    float flow = 0.0f;
    float velocity = 0.0f;
    bool valid = false;
    bool fresh = false;
    bool stale = false;
    uint32_t ageMs = 0;
};

#endif // EOLO_TYPES_FLOW_DATA_H
