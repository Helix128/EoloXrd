#ifndef EOLO_CORE_SENSORS_ANEMOMETER_MODEL_H
#define EOLO_CORE_SENSORS_ANEMOMETER_MODEL_H

#include <math.h>
#include <stdint.h>
#include <Eolo/Types/AnemometerData.h>

class AnemometerModel
{
public:
    static float speedFromRaw(int rawSpeed)
    {
        return rawSpeed / 100.0f;
    }

    static float kphFromMetersPerSecond(float speed)
    {
        return isfinite(speed) ? speed * 3.6f : -1.0f;
    }

    static void applyReadSuccess(AnemometerData &data, uint32_t &lastSuccessMs, int rawSpeed, int direction, uint32_t nowMs)
    {
        data.speed = speedFromRaw(rawSpeed);
        data.windKph = kphFromMetersPerSecond(data.speed);
        data.direction = direction;
        data.valid = rawSpeed >= 0 && isfinite(data.speed) && isfinite(data.windKph);
        data.fresh = data.valid;
        data.stale = !data.valid;
        data.ageMs = 0;
        lastSuccessMs = nowMs;
    }

    static void applyReadFailure(AnemometerData &data, uint32_t lastSuccessMs, uint32_t nowMs, uint32_t staleDataMs)
    {
        data.valid = lastSuccessMs > 0 && (nowMs - lastSuccessMs) <= staleDataMs;
    }

    static bool refreshValidity(AnemometerData &data, uint32_t lastSuccessMs, uint32_t nowMs, uint32_t staleDataMs)
    {
        if (lastSuccessMs == 0 || (nowMs - lastSuccessMs) > staleDataMs)
            data.valid = false;
        return data.valid;
    }

    static bool refreshAge(AnemometerData &data, uint32_t lastSuccessMs, uint32_t nowMs,
                           uint32_t freshDataMs, uint32_t staleDataMs)
    {
        data.ageMs = lastSuccessMs ? nowMs - lastSuccessMs : staleDataMs + 1;
        data.fresh = lastSuccessMs && data.ageMs <= freshDataMs;
        data.stale = !lastSuccessMs || data.ageMs > freshDataMs;
        data.valid = lastSuccessMs && data.ageMs <= staleDataMs;
        return data.valid;
    }
};

#endif // EOLO_CORE_SENSORS_ANEMOMETER_MODEL_H
