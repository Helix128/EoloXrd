#ifndef EOLO_CORE_CALIBRATION_CALIBRATION_MODEL_H
#define EOLO_CORE_CALIBRATION_CALIBRATION_MODEL_H

#include <math.h>

class CalibrationModel
{
public:
    static constexpr int MaxPoints = 200;
    static constexpr int MinPoints = 6;
    static constexpr float MinFlowRange = 0.25f;

    static void sortByFlow(int numPoints, int *pwm0, int *pwm1, float *flows)
    {
        for (int i = 0; i < numPoints - 1; i++)
        {
            for (int j = 0; j < numPoints - i - 1; j++)
            {
                if (flows[j] > flows[j + 1])
                {
                    float tf = flows[j];
                    flows[j] = flows[j + 1];
                    flows[j + 1] = tf;

                    int tp0 = pwm0[j];
                    pwm0[j] = pwm0[j + 1];
                    pwm0[j + 1] = tp0;

                    int tp1 = pwm1[j];
                    pwm1[j] = pwm1[j + 1];
                    pwm1[j + 1] = tp1;
                }
            }
        }
    }

    static bool validate(int numPoints, const float *flows)
    {
        if (numPoints < MinPoints)
            return false;

        float first = flows[0];
        float last = flows[numPoints - 1];
        if ((last - first) < MinFlowRange)
            return false;

        for (int i = 1; i < numPoints; i++)
        {
            if (flows[i] < flows[i - 1])
                return false;
        }
        return true;
    }

    static bool getMotorPwms(bool isLoaded,
                             int numPoints,
                             const int *pwm0,
                             const int *pwm1,
                             const float *flows,
                             float targetFlow,
                             int &outPwm0,
                             int &outPwm1)
    {
        if (!isLoaded || numPoints < 2)
        {
            outPwm0 = 0;
            outPwm1 = 0;
            return false;
        }

        if (targetFlow <= flows[0])
        {
            outPwm0 = pwm0[0];
            outPwm1 = pwm1[0];
            return true;
        }

        if (targetFlow >= flows[numPoints - 1])
        {
            outPwm0 = pwm0[numPoints - 1];
            outPwm1 = pwm1[numPoints - 1];
            return true;
        }

        for (int i = 0; i < numPoints - 1; i++)
        {
            float f0 = flows[i];
            float f1 = flows[i + 1];

            if (targetFlow >= f0 && targetFlow <= f1)
            {
                float denom = f1 - f0;
                if (fabsf(denom) < 1e-6f)
                {
                    outPwm0 = (pwm0[i] + pwm0[i + 1]) / 2;
                    outPwm1 = (pwm1[i] + pwm1[i + 1]) / 2;
                    return true;
                }

                float t = (targetFlow - f0) / denom;
                outPwm0 = pwm0[i] + static_cast<int>(t * (pwm0[i + 1] - pwm0[i]));
                outPwm1 = pwm1[i] + static_cast<int>(t * (pwm1[i + 1] - pwm1[i]));
                return true;
            }
        }

        outPwm0 = pwm0[numPoints - 1];
        outPwm1 = pwm1[numPoints - 1];
        return true;
    }
};

#endif
