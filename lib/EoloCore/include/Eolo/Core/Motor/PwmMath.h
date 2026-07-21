#ifndef EOLO_CORE_MOTOR_PWM_MATH_H
#define EOLO_CORE_MOTOR_PWM_MATH_H

#include <math.h>

class PwmMath
{
public:
    static int clamp(int pwm, int maxPwm)
    {
        if (pwm < 0)
            return 0;
        if (pwm > maxPwm)
            return maxPwm;
        return pwm;
    }

    static int nextRamped(int current, int target, int step, int maxPwm)
    {
        current = clamp(current, maxPwm);
        target = clamp(target, maxPwm);
        if (step <= 0 || target <= current)
            return target;
        int next = current + step;
        return next > target ? target : next;
    }

    static int limitStep(int current, int target, int maxStep, int maxPwm)
    {
        current = clamp(current, maxPwm);
        target = clamp(target, maxPwm);
        if (maxStep <= 0)
            return target;
        if (target > current + maxStep)
            return current + maxStep;
        if (target < current - maxStep)
            return current - maxStep;
        return target;
    }

    static int nextClosedLoop(int basePwm,
                              int currentPwm,
                              float targetFlow,
                              float measuredFlow,
                              float dtSeconds,
                              float &integral,
                              float deadbandLpm,
                              float kp,
                              float ki,
                              float integralLimit,
                              int maxStepPwm,
                              int maxPwm)
    {
        basePwm = clamp(basePwm, maxPwm);
        currentPwm = clamp(currentPwm, maxPwm);
        if (dtSeconds < 0.0f)
            dtSeconds = 0.0f;

        float error = targetFlow - measuredFlow;
        if (fabsf(error) < deadbandLpm)
            error = 0.0f;

        integral += error * dtSeconds;
        if (integralLimit >= 0.0f)
        {
            if (integral > integralLimit)
                integral = integralLimit;
            else if (integral < -integralLimit)
                integral = -integralLimit;
        }

        float correction = kp * error + ki * integral;
        int desiredPwm = clamp(basePwm + static_cast<int>(correction), maxPwm);
        return limitStep(currentPwm, desiredPwm, maxStepPwm, maxPwm);
    }
};

#endif // EOLO_CORE_MOTOR_PWM_MATH_H
