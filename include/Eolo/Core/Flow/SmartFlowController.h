#ifndef EOLO_SMART_FLOW_CONTROLLER_H
#define EOLO_SMART_FLOW_CONTROLLER_H

#include <stdint.h>
#include <stdlib.h>
#include <math.h>

struct SmartFlowTune
{
    float kp = 18.0f;
    float ki = 1.4f;
    float kd = 6.0f;
    float integralLimit = 18.0f;
    float deadband = 0.10f;
    float minActive = 0.20f;
    float fastAlpha = 0.45f;
    float slowAlpha = 0.16f;
    float gainAlpha = 0.15f;
    float feedForwardAlpha = 0.18f;
    float minGain = 0.0015f;
    float maxGain = 0.0300f;
    float minLearnDeltaFlow = 0.12f;
    int minLearnDeltaPwm = 24;
    int minStep = 1;
    int maxStep = 14;
    int maxFeedForwardStep = 10;
};

enum SmartFlowMode : uint8_t
{
    SMART_FLOW_PID_ONLY = 0,
    SMART_FLOW_GAIN_PREDICT = 1,
    SMART_FLOW_INTERPOLATE = 2,
    SMART_FLOW_MIN_ACTIVE_BOOST = 3
};

struct SmartFlowStatus
{
    int pwm = 0;
    int pwmFF = 0;
    int pidCorrection = 0;
    int step = 0;
    SmartFlowMode mode = SMART_FLOW_PID_ONLY;
    float rawFlow = 0.0f;
    float fastFlow = 0.0f;
    float slowFlow = 0.0f;
    float errorFast = 0.0f;
    float errorSlow = 0.0f;
    float integral = 0.0f;
    float derivativeFlow = 0.0f;
    float pTerm = 0.0f;
    float iTerm = 0.0f;
    float dTerm = 0.0f;
    float estimatedGain = 0.0f;
    float confidence = 0.0f;
    int minFlowPwm = -1;
    int maxUsefulPwm = -1;
    bool modelValid = false;
    bool lowerClamp = false;
    bool upperSaturation = false;
};

class SmartFlowController
{
    struct Sample
    {
        uint32_t timeMs = 0;
        int pwm = 0;
        float rawFlow = 0.0f;
        float fastFlow = 0.0f;
        bool valid = false;
    };

    static const uint8_t sampleCount = 24;

    SmartFlowTune tune;
    Sample samples[sampleCount];
    uint8_t sampleHead = 0;
    uint8_t samplesStored = 0;
    float fastFlow = 0.0f;
    float slowFlow = 0.0f;
    float previousFastFlow = 0.0f;
    float previousErrorSlow = 0.0f;
    bool filterReady = false;
    float integral = 0.0f;
    float estimatedGain = 0.0f;
    float confidence = 0.0f;
    float smoothedPwmFF = 0.0f;
    bool smoothedPwmFFReady = false;
    int minFlowPwm = -1;
    int maxUsefulPwm = -1;
    uint8_t upperFlatCount = 0;

    static int clampInt(int value, int low, int high)
    {
        if (value < low) return low;
        if (value > high) return high;
        return value;
    }

    static float clampFloat(float value, float low, float high)
    {
        if (value < low) return low;
        if (value > high) return high;
        return value;
    }

    static int limitStep(int current, int target, int step, int maxPwm)
    {
        current = clampInt(current, 0, maxPwm);
        target = clampInt(target, 0, maxPwm);
        if (step <= 0) return target;
        if (target > current + step) return current + step;
        if (target < current - step) return current - step;
        return target;
    }

    int newestSampleIndex() const
    {
        if (samplesStored == 0) return -1;
        return (sampleHead + sampleCount - 1) % sampleCount;
    }

    bool interpolatePwm(float targetFlow, int &pwm) const
    {
        if (samplesStored < 3) return false;

        bool found = false;
        int bestLowPwm = 0;
        int bestHighPwm = 0;
        float bestLowFlow = 0.0f;
        float bestHighFlow = 0.0f;
        float bestSpan = 1000000.0f;

        for (uint8_t i = 0; i < sampleCount; i++)
        {
            if (!samples[i].valid) continue;
            for (uint8_t j = 0; j < sampleCount; j++)
            {
                if (i == j || !samples[j].valid) continue;
                float f0 = samples[i].fastFlow;
                float f1 = samples[j].fastFlow;
                if ((f0 <= targetFlow && targetFlow <= f1) || (f1 <= targetFlow && targetFlow <= f0))
                {
                    float span = fabsf(f1 - f0);
                    if (span < 0.15f || span >= bestSpan) continue;
                    bestSpan = span;
                    bestLowPwm = samples[i].pwm;
                    bestHighPwm = samples[j].pwm;
                    bestLowFlow = f0;
                    bestHighFlow = f1;
                    found = true;
                }
            }
        }

        if (!found) return false;
        float denom = bestHighFlow - bestLowFlow;
        if (fabsf(denom) < 0.001f) return false;
        float ratio = (targetFlow - bestLowFlow) / denom;
        pwm = bestLowPwm + static_cast<int>((bestHighPwm - bestLowPwm) * ratio);
        return true;
    }

    void addSample(uint32_t nowMs, int pwm, float rawFlow, float currentFastFlow)
    {
        samples[sampleHead].timeMs = nowMs;
        samples[sampleHead].pwm = pwm;
        samples[sampleHead].rawFlow = rawFlow;
        samples[sampleHead].fastFlow = currentFastFlow;
        samples[sampleHead].valid = true;
        sampleHead = (sampleHead + 1) % sampleCount;
        if (samplesStored < sampleCount) samplesStored++;
    }

    void updateUsefulRange(int pwm, float rawFlow)
    {
        if (rawFlow > tune.minActive)
        {
            if (minFlowPwm < 0 || pwm < minFlowPwm) minFlowPwm = pwm;
            if (maxUsefulPwm < 0 || pwm > maxUsefulPwm) maxUsefulPwm = pwm;
        }
    }

    void updateGainModel(float targetFlow)
    {
        int latestIdx = newestSampleIndex();
        if (latestIdx < 0 || samplesStored < 2) return;

        const Sample &latest = samples[latestIdx];
        for (uint8_t n = 1; n < samplesStored; n++)
        {
            int idx = (latestIdx + sampleCount - n) % sampleCount;
            const Sample &previous = samples[idx];
            if (!previous.valid || previous.timeMs == latest.timeMs) continue;

            int deltaPwm = latest.pwm - previous.pwm;
            float deltaFlow = latest.fastFlow - previous.fastFlow;
            if (abs(deltaPwm) < tune.minLearnDeltaPwm) continue;
            if (fabsf(deltaFlow) < tune.minLearnDeltaFlow) continue;
            if ((deltaPwm > 0 && deltaFlow <= 0.0f) || (deltaPwm < 0 && deltaFlow >= 0.0f)) continue;

            float newGain = deltaFlow / deltaPwm;
            if (newGain < tune.minGain || newGain > tune.maxGain) continue;

            if (estimatedGain <= 0.0f)
                estimatedGain = newGain;
            else
                estimatedGain = (1.0f - tune.gainAlpha) * estimatedGain + tune.gainAlpha * newGain;

            float errorAbs = fabsf(targetFlow - latest.fastFlow);
            float confidenceTarget = errorAbs < 0.35f ? 1.0f : (errorAbs < 1.0f ? 0.7f : 0.45f);
            confidence = clampFloat(confidence * 0.85f + confidenceTarget * 0.15f, 0.0f, 1.0f);
            return;
        }
    }

    int predictPwmForFlow(int currentPwm, float targetFlow, float measuredFlow, int maxPwm, SmartFlowMode &mode) const
    {
        int predicted = currentPwm;
        if (confidence > 0.35f && interpolatePwm(targetFlow, predicted))
        {
            mode = SMART_FLOW_INTERPOLATE;
        }
        else if (estimatedGain >= tune.minGain)
        {
            predicted = currentPwm + static_cast<int>((targetFlow - measuredFlow) / estimatedGain);
            mode = SMART_FLOW_GAIN_PREDICT;
        }
        else
        {
            float error = targetFlow - measuredFlow;
            predicted = currentPwm + static_cast<int>(tune.kp * error);
            mode = SMART_FLOW_PID_ONLY;
        }

        return clampInt(predicted, 0, maxPwm);
    }

    int smoothFeedForward(int currentPwm, int pwmFF, int maxPwm)
    {
        if (!smoothedPwmFFReady)
        {
            smoothedPwmFF = currentPwm;
            smoothedPwmFFReady = true;
        }

        float target = smoothedPwmFF + (pwmFF - smoothedPwmFF) * tune.feedForwardAlpha;
        float delta = target - smoothedPwmFF;
        if (delta > tune.maxFeedForwardStep) delta = tune.maxFeedForwardStep;
        if (delta < -tune.maxFeedForwardStep) delta = -tune.maxFeedForwardStep;
        smoothedPwmFF = clampFloat(smoothedPwmFF + delta, 0.0f, static_cast<float>(maxPwm));
        return clampInt(static_cast<int>(smoothedPwmFF), 0, maxPwm);
    }

    int adaptiveStepLimit(float error, float derivativeFlow, bool modelValid) const
    {
        (void)modelValid;
        float absError = fabsf(error);
        int step = tune.minStep;

        if (absError > 3.0f) step = tune.maxStep;
        else if (absError > 1.5f) step = static_cast<int>(tune.maxStep * 0.75f);
        else if (absError > 0.7f) step = static_cast<int>(tune.maxStep * 0.55f);
        else if (absError > 0.25f) step = static_cast<int>(tune.maxStep * 0.35f);

        if (fabsf(derivativeFlow) > 0.8f && absError < 1.2f) step = static_cast<int>(step * 0.60f);
        return clampInt(step, tune.minStep, tune.maxStep);
    }

    int updatePidTerms(float targetFlow,
                       float dtSeconds,
                       bool feedForwardValid,
                       bool outputSaturated,
                       bool errorDrivesAwayFromSaturation,
                       int &pidCorrection,
                       float &pTerm,
                       float &iTerm,
                       float &dTerm)
    {
        float errorFast = targetFlow - fastFlow;
        float errorSlow = targetFlow - slowFlow;
        if (fabsf(errorFast) < tune.deadband) errorFast = 0.0f;
        if (fabsf(errorSlow) < tune.deadband) errorSlow = 0.0f;

        bool signChanged = (previousErrorSlow > 0.0f && errorSlow < 0.0f) || (previousErrorSlow < 0.0f && errorSlow > 0.0f);
        bool integrate = (!outputSaturated || errorDrivesAwayFromSaturation) && fabsf(errorSlow) < 1.8f;
        if (!feedForwardValid) integrate = !outputSaturated || errorDrivesAwayFromSaturation;

        if (signChanged)
            integral *= 0.35f;
        if (integrate)
            integral += errorSlow * dtSeconds;

        integral = clampFloat(integral, -tune.integralLimit, tune.integralLimit);
        previousErrorSlow = errorSlow;

        float derivative = dtSeconds > 0.001f ? (fastFlow - previousFastFlow) / dtSeconds : 0.0f;
        float ffScale = feedForwardValid ? (1.0f - 0.65f * confidence) : 1.0f;
        pTerm = ffScale * tune.kp * errorFast;
        iTerm = tune.ki * integral;
        dTerm = -tune.kd * derivative;
        float correction = pTerm + iTerm + dTerm;
        pidCorrection = static_cast<int>(correction);
        return pidCorrection;
    }

public:
    void setTune(const SmartFlowTune &newTune)
    {
        tune = newTune;
    }

    const SmartFlowTune &getTune() const
    {
        return tune;
    }

    void resetAll()
    {
        resetController(false);
    }

    void resetController(bool preserveModel)
    {
        fastFlow = 0.0f;
        slowFlow = 0.0f;
        previousFastFlow = 0.0f;
        previousErrorSlow = 0.0f;
        filterReady = false;
        integral = 0.0f;
        upperFlatCount = 0;
        smoothedPwmFF = 0.0f;
        smoothedPwmFFReady = false;

        if (!preserveModel)
        {
            for (uint8_t i = 0; i < sampleCount; i++) samples[i].valid = false;
            sampleHead = 0;
            samplesStored = 0;
            estimatedGain = 0.0f;
            confidence = 0.0f;
            minFlowPwm = -1;
            maxUsefulPwm = -1;
        }
    }

    SmartFlowStatus update(uint32_t nowMs, int currentPwm, float targetFlow, float rawFlow, float dtSeconds, int maxPwm)
    {
        SmartFlowStatus status;
        status.rawFlow = rawFlow;

        if (!filterReady)
        {
            fastFlow = rawFlow;
            slowFlow = rawFlow;
            previousFastFlow = rawFlow;
            filterReady = true;
        }
        else
        {
            previousFastFlow = fastFlow;
            fastFlow = tune.fastAlpha * rawFlow + (1.0f - tune.fastAlpha) * fastFlow;
            slowFlow = tune.slowAlpha * rawFlow + (1.0f - tune.slowAlpha) * slowFlow;
        }

        status.fastFlow = fastFlow;
        status.slowFlow = slowFlow;
        status.errorFast = targetFlow - fastFlow;
        status.errorSlow = targetFlow - slowFlow;
        status.integral = integral;
        status.estimatedGain = estimatedGain;
        status.confidence = confidence;
        status.minFlowPwm = minFlowPwm;
        status.maxUsefulPwm = maxUsefulPwm;

        addSample(nowMs, currentPwm, rawFlow, fastFlow);
        updateUsefulRange(currentPwm, rawFlow);
        updateGainModel(targetFlow);

        bool modelValid = estimatedGain >= tune.minGain && confidence > 0.20f;
        SmartFlowMode mode = SMART_FLOW_PID_ONLY;
        int pwmFF = predictPwmForFlow(currentPwm, targetFlow, fastFlow, maxPwm, mode);

        if (fastFlow < tune.minActive && targetFlow > tune.minActive && currentPwm < maxPwm)
        {
            integral = 0.0f;
            mode = SMART_FLOW_MIN_ACTIVE_BOOST;
            pwmFF = maxPwm;
        }

        bool lowerClamp = false;
        if (modelValid && minFlowPwm >= 0 && targetFlow > tune.minActive && pwmFF < minFlowPwm - 80)
        {
            pwmFF = minFlowPwm - 80;
            lowerClamp = true;
        }

        pwmFF = smoothFeedForward(currentPwm, clampInt(pwmFF, 0, maxPwm), maxPwm);
        int pidCorrection = 0;
        float pTerm = 0.0f;
        float iTerm = 0.0f;
        float dTerm = 0.0f;
        bool predictedSaturated = pwmFF <= 0 || pwmFF >= maxPwm;
        bool errorDrivesAwayFromSaturation = (pwmFF <= 0 && targetFlow > fastFlow) || (pwmFF >= maxPwm && targetFlow < fastFlow);
        updatePidTerms(targetFlow, dtSeconds, modelValid, predictedSaturated, errorDrivesAwayFromSaturation,
                       pidCorrection, pTerm, iTerm, dTerm);

        int desired = clampInt(pwmFF + pidCorrection, 0, maxPwm);
        float derivative = dtSeconds > 0.001f ? (fastFlow - previousFastFlow) / dtSeconds : 0.0f;
        int step = adaptiveStepLimit(targetFlow - fastFlow, derivative, modelValid);
        int nextPwm = limitStep(currentPwm, desired, step, maxPwm);

        if (currentPwm > maxPwm - step && rawFlow < targetFlow - 0.5f)
            upperFlatCount++;
        else if (upperFlatCount > 0)
            upperFlatCount--;

        if (upperFlatCount >= 8)
        {
            maxUsefulPwm = currentPwm;
            confidence = clampFloat(confidence * 0.85f, 0.0f, 1.0f);
        }

        status.pwm = nextPwm;
        status.pwmFF = pwmFF;
        status.pidCorrection = pidCorrection;
        status.step = step;
        status.mode = mode;
        status.integral = integral;
        status.derivativeFlow = derivative;
        status.pTerm = pTerm;
        status.iTerm = iTerm;
        status.dTerm = dTerm;
        status.estimatedGain = estimatedGain;
        status.confidence = confidence;
        status.modelValid = modelValid;
        status.lowerClamp = lowerClamp;
        status.upperSaturation = upperFlatCount >= 8;
        status.minFlowPwm = minFlowPwm;
        status.maxUsefulPwm = maxUsefulPwm;
        return status;
    }
};

#endif // EOLO_SMART_FLOW_CONTROLLER_H
