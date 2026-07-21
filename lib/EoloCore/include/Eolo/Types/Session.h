#ifndef EOLO_TYPES_SESSION_H
#define EOLO_TYPES_SESSION_H

#include <stdint.h>

static constexpr uint8_t MaxFlowSections = 8;

struct FlowSection
{
    uint32_t durationSeconds = 0;
    float targetFlow = 5.0f;
};

typedef struct Session{
    uint32_t startUnix = 0;
    unsigned int duration = 0;
    unsigned long elapsedTime = 0;
    unsigned long lastLog = 0;
    float targetFlow = 5.0;
    bool usePlantower = true;
    float capturedVolume = 0.0;
    uint8_t flowSectionCount = 0;
    FlowSection flowSections[MaxFlowSections];
}Session;

#endif
