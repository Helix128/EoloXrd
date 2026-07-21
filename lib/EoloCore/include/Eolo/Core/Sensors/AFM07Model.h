#ifndef EOLO_CORE_SENSORS_AFM07_MODEL_H
#define EOLO_CORE_SENSORS_AFM07_MODEL_H

#include <math.h>
#include <stdint.h>
#include <Eolo/Types/FlowData.h>

class AFM07Model
{
public:
    static float flowFromRaw(uint16_t raw, float divisor)
    {
        if (!isfinite(divisor) || divisor <= 0.0f)
            return -1.0f;
        return (float)raw / divisor;
    }

    static void applyReadSuccess(FlowData &data, uint32_t &lastSuccessMs, uint16_t raw, uint32_t nowMs, float divisor)
    {
        float flow = flowFromRaw(raw, divisor);
        if (flow < 0.0f || !isfinite(flow))
        {
            data.valid = false;
            data.fresh = false;
            data.stale = true;
            data.ageMs = 0;
            return;
        }
        data.flow = flow;
        data.velocity = flow;
        data.valid = true;
        data.fresh = true;
        data.stale = false;
        data.ageMs = 0;
        lastSuccessMs = nowMs;
    }

    static void applyReadFailure(FlowData &data, uint32_t lastSuccessMs, uint32_t nowMs, uint32_t freshDataMs, uint32_t staleDataMs)
    {
        uint32_t ageMs = lastSuccessMs > 0 ? nowMs - lastSuccessMs : staleDataMs + 1;
        data.ageMs = ageMs;
        data.fresh = false;
        data.stale = ageMs > freshDataMs;
        data.valid = lastSuccessMs > 0 && ageMs <= staleDataMs;
    }

    static bool refreshAge(FlowData &data, uint32_t lastSuccessMs, uint32_t nowMs, uint32_t freshDataMs, uint32_t staleDataMs)
    {
        uint32_t ageMs = lastSuccessMs > 0 ? nowMs - lastSuccessMs : staleDataMs + 1;
        data.ageMs = ageMs;
        data.fresh = lastSuccessMs > 0 && ageMs <= freshDataMs;
        data.stale = lastSuccessMs > 0 && ageMs > freshDataMs;
        if (lastSuccessMs == 0 || ageMs > staleDataMs)
        {
            data.valid = false;
            data.fresh = false;
        }
        return data.valid;
    }
};

#endif // EOLO_CORE_SENSORS_AFM07_MODEL_H
