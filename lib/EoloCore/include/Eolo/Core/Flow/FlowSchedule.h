#ifndef EOLO_CORE_FLOW_FLOW_SCHEDULE_H
#define EOLO_CORE_FLOW_FLOW_SCHEDULE_H

#include <math.h>
#include <stdint.h>
#include <Eolo/Types/Session.h>

class FlowSchedule
{
public:
    static bool validFlow(float flow, float minFlow, float maxFlow)
    {
        return !isnan(flow) && flow >= minFlow && flow <= maxFlow;
    }

    static uint32_t durationSum(uint8_t count, const FlowSection *sections)
    {
        uint32_t total = 0;
        uint8_t capped = count > MaxFlowSections ? MaxFlowSections : count;
        for (uint8_t i = 0; i < capped; i++)
            total += sections[i].durationSeconds;
        return total;
    }

    static bool validate(uint8_t count,
                         const FlowSection *sections,
                         uint32_t totalDurationSeconds,
                         uint32_t infiniteDuration,
                         float minFlow,
                         float maxFlow)
    {
        if (count == 0)
            return true;
        if (sections == nullptr || count > MaxFlowSections)
            return false;

        uint32_t total = 0;
        for (uint8_t i = 0; i < count; i++)
        {
            if (sections[i].durationSeconds == 0)
                return false;
            if (!validFlow(sections[i].targetFlow, minFlow, maxFlow))
                return false;
            total += sections[i].durationSeconds;
        }

        if (totalDurationSeconds != infiniteDuration && total > totalDurationSeconds)
            return false;

        return true;
    }

    static float targetAtElapsed(float defaultFlow,
                                 uint8_t count,
                                 const FlowSection *sections,
                                 uint32_t elapsedSeconds)
    {
        if (count == 0 || sections == nullptr)
            return defaultFlow;

        uint32_t end = 0;
        uint8_t capped = count > MaxFlowSections ? MaxFlowSections : count;
        for (uint8_t i = 0; i < capped; i++)
        {
            end += sections[i].durationSeconds;
            if (elapsedSeconds < end)
                return sections[i].targetFlow;
        }

        return sections[capped - 1].targetFlow;
    }
};

#endif
