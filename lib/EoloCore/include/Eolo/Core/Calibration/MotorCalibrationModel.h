#ifndef EOLO_CORE_CALIBRATION_MOTOR_CALIBRATION_MODEL_H
#define EOLO_CORE_CALIBRATION_MOTOR_CALIBRATION_MODEL_H

#include <math.h>
#include <stdint.h>

struct MotorCalibrationSampleStats
{
    uint8_t count = 0;
    float sum = 0.0f;
    float squareSum = 0.0f;

    void clear()
    {
        count = 0;
        sum = 0.0f;
        squareSum = 0.0f;
    }

    void add(float flow)
    {
        if (!isfinite(flow) || count == UINT8_MAX)
            return;
        ++count;
        sum += flow;
        squareSum += flow * flow;
    }

    float mean() const
    {
        return count == 0 ? 0.0f : sum / count;
    }

    float standardDeviation() const
    {
        if (count == 0)
            return 0.0f;
        float variance = squareSum / count - mean() * mean();
        return sqrtf(variance > 0.0f ? variance : 0.0f);
    }
};

struct MotorCalibrationPoint
{
    int pwm = 0;
    float flow = 0.0f;
    float stddev = 0.0f;
    uint8_t samples = 0;
};

class MotorCalibrationModel
{
public:
    static bool makePoint(const MotorCalibrationSampleStats &stats,
                          int pwm,
                          float minValidFlow,
                          float minFlowDelta,
                          float maxFlowStddev,
                          const MotorCalibrationPoint *previous,
                          MotorCalibrationPoint &point)
    {
        const float mean = stats.mean();
        const float stddev = stats.standardDeviation();
        if (stats.count == 0 || !isfinite(mean) || !isfinite(stddev) ||
            mean < minValidFlow || stddev > maxFlowStddev ||
            (previous != nullptr && mean - previous->flow < minFlowDelta))
            return false;

        point.pwm = pwm;
        point.flow = mean;
        point.stddev = stddev;
        point.samples = stats.count;
        return true;
    }
};

#endif
