#ifndef EOLO_CORE_SENSORS_ANEMOMETER_MODEL_H
#define EOLO_CORE_SENSORS_ANEMOMETER_MODEL_H

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
        return speed * 3.6f;
    }

    static void applyReadSuccess(AnemometerData &data, uint32_t &lastSuccessMs, int rawSpeed, int direction, uint32_t nowMs)
    {
        data.speed = speedFromRaw(rawSpeed);
        data.windKph = kphFromMetersPerSecond(data.speed);
        data.direction = direction;
        data.valid = true;
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
};

#endif // EOLO_CORE_SENSORS_ANEMOMETER_MODEL_H
