#ifndef EOLO_SMART_FLOW_CONTROLLER_H
#define EOLO_SMART_FLOW_CONTROLLER_H

#include <stdint.h>
#include <stdlib.h>
#include <math.h>

struct SmartFlowTune
{
    float kp = 14.0f;               // Soft proportional gain
    float ki = 0.4f;                // Soft integral gain
    float kd = 0.0f;                // Soft derivative gain
    float integralLimit = 16.0f;    // Integral accumulation ceiling
    float deadband = 0.10f;         // Deadband in L/min where trim is zero
    float minActive = 0.20f;        // Minimum active flow setpoint
    float fastAlpha = 0.35f;        // Fast EMA filter alpha
    float slowAlpha = 0.15f;        // Slow EMA filter alpha
    int seekStep = 16;              // Max PWM step during initial center seek
    int softTrimMax = 64;           // Max +- PWM excursion from centerPwm
    int softStepLimit = 2;          // Max PWM change per cycle in soft trim
    float sensitivity = 1.0f;       // Post-center sensitivity multiplier
    uint32_t recenterDelayMs = 4000;// Sustained drift time before recentering
    int recenterStep = 1;           // PWM step size when shifting center
    float breakoutThreshold = 2.0f; // Error threshold for re-seeking
    // Backward-compatibility aliases
    int maxStep = 16;
    int maxFeedForwardStep = 10;
};

enum SmartFlowMode : uint8_t
{
    SMART_FLOW_SEEK_CENTER = 0,     // Seeking target flow crossing / operating point
    SMART_FLOW_CENTER_LOCKED = 1,   // Center found and locked; soft trimming active
    SMART_FLOW_RECENTER = 2,        // Shifting center due to sustained drift
    // Backward-compatibility aliases
    SMART_FLOW_PID_ONLY = 0,
    SMART_FLOW_GAIN_PREDICT = 1,
    SMART_FLOW_INTERPOLATE = 2,
    SMART_FLOW_MIN_ACTIVE_BOOST = 3
};

struct SmartFlowStatus
{
    int pwm = 0;
    int centerPwm = 0;
    int trimPwm = 0;
    int pwmFF = 0;                  // Alias to centerPwm / base
    int pidCorrection = 0;          // Alias to trimPwm
    int step = 0;
    SmartFlowMode mode = SMART_FLOW_SEEK_CENTER;
    bool centerFound = false;
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
    SmartFlowTune tune;
    float fastFlow = 0.0f;
    float slowFlow = 0.0f;
    float previousFastFlow = 0.0f;
    float previousRawFlow = 0.0f;
    int previousPwm = 0;
    bool filterReady = false;

    bool centerFound = false;
    int centerPwm = 0;
    float currentTrim = 0.0f;
    float integral = 0.0f;
    uint8_t settledTicks = 0;
    bool isDrifting = false;
    uint32_t driftStartMs = 0;
    bool isBreakout = false;
    uint32_t breakoutStartMs = 0;
    SmartFlowMode mode = SMART_FLOW_SEEK_CENTER;
    float previousTargetFlow = -1.0f;

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

public:
    void setTune(const SmartFlowTune &newTune)
    {
        tune = newTune;
    }

    const SmartFlowTune &getTune() const
    {
        return tune;
    }

    void uncenter()
    {
        centerFound = false;
        centerPwm = 0;
        mode = SMART_FLOW_SEEK_CENTER;
        currentTrim = 0.0f;
        integral = 0.0f;
        settledTicks = 0;
        isDrifting = false;
        driftStartMs = 0;
        isBreakout = false;
        breakoutStartMs = 0;
    }

    void seedCenter(int pwm)
    {
        if (pwm > 0)
        {
            centerPwm = pwm;
            centerFound = true;
            mode = SMART_FLOW_CENTER_LOCKED;
            currentTrim = 0.0f;
            integral = 0.0f;
            settledTicks = 3;
            isDrifting = false;
            isBreakout = false;
            previousTargetFlow = -1.0f;
        }
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
        previousRawFlow = 0.0f;
        previousPwm = 0;
        filterReady = false;
        integral = 0.0f;
        currentTrim = 0.0f;
        settledTicks = 0;
        isDrifting = false;
        driftStartMs = 0;
        isBreakout = false;
        breakoutStartMs = 0;
        previousTargetFlow = -1.0f;

        if (!preserveModel)
        {
            centerFound = false;
            centerPwm = 0;
            mode = SMART_FLOW_SEEK_CENTER;
        }
        else if (centerFound && centerPwm > 0)
        {
            mode = SMART_FLOW_CENTER_LOCKED;
        }
    }

    SmartFlowStatus update(uint32_t nowMs, int currentPwm, float targetFlow, float rawFlow, float dtSeconds, int maxPwm)
    {
        SmartFlowStatus status;
        status.rawFlow = rawFlow;

        // 1. Filtrado de flujo de aire
        if (!filterReady)
        {
            fastFlow = rawFlow;
            slowFlow = rawFlow;
            previousFastFlow = rawFlow;
            previousRawFlow = rawFlow;
            previousPwm = currentPwm;
            filterReady = true;
        }
        else
        {
            previousFastFlow = fastFlow;
            previousRawFlow = rawFlow;
            fastFlow = tune.fastAlpha * rawFlow + (1.0f - tune.fastAlpha) * fastFlow;
            slowFlow = tune.slowAlpha * rawFlow + (1.0f - tune.slowAlpha) * slowFlow;
        }

        const float errorFast = targetFlow - fastFlow;
        const float errorSlow = targetFlow - slowFlow;
        const float derivative = dtSeconds > 0.001f ? (fastFlow - previousFastFlow) / dtSeconds : 0.0f;

        // 2. Comprobación de cambio de consigna o setpoint inactivo
        if (targetFlow <= tune.minActive)
        {
            resetController(false);
            status.pwm = 0;
            status.fastFlow = fastFlow;
            status.slowFlow = slowFlow;
            status.mode = SMART_FLOW_SEEK_CENTER;
            return status;
        }

        // Cambio de consigna activo durante ejecución: forzar re-búsqueda suave de centro
        if (previousTargetFlow >= 0.0f && fabsf(targetFlow - previousTargetFlow) > 0.001f)
        {
            uncenter();
        }
        previousTargetFlow = targetFlow;

        int nextPwm = currentPwm;
        int activeSeekStep = tune.seekStep > 0 ? tune.seekStep : tune.maxStep;
        if (activeSeekStep <= 0) activeSeekStep = 16;

        float pTerm = 0.0f;
        float iTerm = 0.0f;
        float dTerm = 0.0f;

        // 3. FASE: Búsqueda y Estabilización de Centro (SEEK_CENTER)
        if (!centerFound)
        {
            mode = SMART_FLOW_SEEK_CENTER;
            const float absErr = fabsf(errorFast);

            // Ventana de Estabilización (Settling Window):
            // Para confirmar el centro de operación y evitar enclavar en un sobrepico o transitorio,
            // el flujo debe permanecer dentro de la ventana objetivo (+-deadband) durante 3 ticks consecutivos.
            const float settlingBand = (tune.deadband > 0.10f) ? tune.deadband : 0.10f;
            if (fastFlow > tune.minActive && absErr <= settlingBand)
            {
                settledTicks++;
            }
            else
            {
                settledTicks = 0;
            }

            const uint8_t requiredSettledTicks = 3;

            if (settledTicks >= requiredSettledTicks)
            {
                centerPwm = currentPwm;
                centerPwm = clampInt(centerPwm, 0, maxPwm);
                centerFound = true;
                mode = SMART_FLOW_CENTER_LOCKED;
                currentTrim = 0.0f;
                integral = 0.0f;
                isDrifting = false;
                driftStartMs = 0;
                isBreakout = false;
                breakoutStartMs = 0;
                nextPwm = centerPwm;
            }
            else
            {
                // Rampa proporcional continua y amortiguada (Anti-hunting):
                // A gran distancia acelera el acercamiento; cerca del setpoint atenúa el paso
                // proporcionalmente al error para entrar suavemente sin sobrepicos ni oscilaciones.
                int stepSize = static_cast<int>(absErr * 30.0f);
                if (stepSize < 2) stepSize = 2;
                if (stepSize > activeSeekStep) stepSize = activeSeekStep;

                if (errorFast > 0.0f)
                    nextPwm = limitStep(currentPwm, maxPwm, stepSize, maxPwm);
                else
                    nextPwm = limitStep(currentPwm, 0, stepSize, maxPwm);
            }
        }
        else // 4. FASE: Centro Enclavado y Trimming Suave (CENTER_LOCKED / RECENTER)
        {
            // Verificación de perturbación extrema (breakout)
            if (fabsf(errorFast) > tune.breakoutThreshold)
            {
                if (!isBreakout)
                {
                    isBreakout = true;
                    breakoutStartMs = nowMs;
                }
                else if ((nowMs - breakoutStartMs) > 6000UL)
                {
                    // Romper enclave para re-buscar centro ante cambio drástico persistente
                    centerFound = false;
                    isBreakout = false;
                    breakoutStartMs = 0;
                    mode = SMART_FLOW_SEEK_CENTER;
                }
            }
            else
            {
                isBreakout = false;
                breakoutStartMs = 0;
            }

            if (centerFound)
            {
                // Manejo de zona muerta: dentro de deadband, error efectivo es 0
                float effectiveError = 0.0f;
                if (fabsf(errorFast) > tune.deadband)
                {
                    effectiveError = (errorFast > 0.0f) ? (errorFast - tune.deadband) : (errorFast + tune.deadband);
                }

                // Detección de deriva sostenida para auto-recentrado suave
                if (tune.recenterDelayMs > 0 && fabsf(effectiveError) > 0.05f)
                {
                    if (!isDrifting)
                    {
                        isDrifting = true;
                        driftStartMs = nowMs;
                    }
                    else if ((nowMs - driftStartMs) >= tune.recenterDelayMs)
                    {
                        int shift = (effectiveError > 0.0f) ? tune.recenterStep : -tune.recenterStep;
                        centerPwm = clampInt(centerPwm + shift, 0, maxPwm);
                        mode = SMART_FLOW_RECENTER;
                        driftStartMs = nowMs;
                    }
                }
                else
                {
                    isDrifting = false;
                    driftStartMs = 0;
                    mode = SMART_FLOW_CENTER_LOCKED;
                }

                // Cálculo del Soft PID (Trimming suave)
                const float sens = (tune.sensitivity > 0.05f) ? tune.sensitivity : 1.0f;
                pTerm = sens * tune.kp * effectiveError;

                // Integración lenta con anti-windup estricto
                if (fabsf(effectiveError) > 0.01f)
                {
                    integral += sens * effectiveError * dtSeconds;
                    integral = clampFloat(integral, -tune.integralLimit, tune.integralLimit);
                }
                else
                {
                    // En zona muerta, reducir suavemente el acumulador integral
                    integral *= 0.95f;
                }
                iTerm = tune.ki * integral;

                // Derivativo suave (anti-ruido)
                dTerm = -tune.kd * derivative;

                float targetTrim = pTerm + iTerm + dTerm;
                int maxTrim = tune.softTrimMax > 0 ? tune.softTrimMax : 64;
                targetTrim = clampFloat(targetTrim, -static_cast<float>(maxTrim), static_cast<float>(maxTrim));

                // Slew-rate limiter sobre el trimming (garantiza transiciones ultra-suaves)
                int maxSoftStep = tune.softStepLimit > 0 ? tune.softStepLimit : 2;
                float trimDelta = targetTrim - currentTrim;
                trimDelta = clampFloat(trimDelta, -static_cast<float>(maxSoftStep), static_cast<float>(maxSoftStep));
                currentTrim += trimDelta;

                int desiredPwm = centerPwm + static_cast<int>(currentTrim);
                nextPwm = clampInt(desiredPwm, 0, maxPwm);
            }
        }

        previousPwm = currentPwm;

        // Actualizar datos de estado
        status.pwm = nextPwm;
        status.centerPwm = centerPwm;
        status.trimPwm = static_cast<int>(currentTrim);
        status.pwmFF = centerPwm;
        status.pidCorrection = static_cast<int>(currentTrim);
        status.step = abs(nextPwm - currentPwm);
        status.mode = mode;
        status.centerFound = centerFound;
        status.fastFlow = fastFlow;
        status.slowFlow = slowFlow;
        status.errorFast = errorFast;
        status.errorSlow = errorSlow;
        status.integral = integral;
        status.derivativeFlow = derivative;
        status.pTerm = pTerm;
        status.iTerm = iTerm;
        status.dTerm = dTerm;
        status.estimatedGain = (centerPwm > 0) ? (fastFlow / static_cast<float>(centerPwm)) : 0.0f;
        status.confidence = centerFound ? 1.0f : 0.2f;
        status.modelValid = centerFound;
        status.lowerClamp = (nextPwm <= 0);
        status.upperSaturation = (nextPwm >= maxPwm);
        status.minFlowPwm = (centerFound && centerPwm > 0) ? centerPwm : -1;
        status.maxUsefulPwm = maxPwm;

        return status;
    }
};

#endif // EOLO_SMART_FLOW_CONTROLLER_H
