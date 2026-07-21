#ifndef EOLO_CORE_SENSORS_FS3000_FLOW_MODEL_H
#define EOLO_CORE_SENSORS_FS3000_FLOW_MODEL_H

#include <math.h>

class FS3000FlowModel
{
public:
    static float flowFromVelocity(float speed)
    {
        if (!isfinite(speed) || speed <= 0.0f)
            return -1.0f;

        static constexpr float velocityPoints[] = {0.06f, 0.34f, 0.7f, 1.06f, 1.42f, 1.74f, 2.02f, 2.30f, 2.60f, 2.80f, 3.00f, 3.33f};
        static constexpr float flowPoints[] = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f};
        static constexpr int numPoints = 12;

        if (speed < velocityPoints[0])
            return 0.0f;

        for (int i = 0; i < numPoints - 1; i++)
        {
            if (speed < velocityPoints[i + 1])
                return interpolate(speed, velocityPoints[i], velocityPoints[i + 1], flowPoints[i], flowPoints[i + 1]);
        }

        return interpolate(speed,
                           velocityPoints[numPoints - 2],
                           velocityPoints[numPoints - 1],
                           flowPoints[numPoints - 2],
                           flowPoints[numPoints - 1]);
    }

private:
    static float interpolate(float x, float x0, float x1, float y0, float y1)
    {
        float slope = (y1 - y0) / (x1 - x0);
        return y0 + slope * (x - x0);
    }
};

#endif // EOLO_CORE_SENSORS_FS3000_FLOW_MODEL_H
