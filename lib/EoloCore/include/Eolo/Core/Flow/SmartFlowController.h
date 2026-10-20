#ifndef EOLO_SMART_FLOW_CONTROLLER_H
#define EOLO_SMART_FLOW_CONTROLLER_H

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <Eolo/Core/Flow/FlowFeedForwardTable.h>

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
    // Curva PWM→flujo para aproximación directa; vacía = búsqueda por rampa.
    FlowFeedForwardTable feedForward;
    float rampCountsPerSecond = 0.0f; // Rampa física del motor (0 = desconocida)
    // Backward-compatibility aliases
    int maxStep = 16;
    int maxFeedForwardStep = 10;
};

enum SmartFlowMode : uint8_t
{
    SMART_FLOW_SEEK_CENTER = 0,     // Seeking target flow crossing / operating point
    SMART_FLOW_CENTER_LOCKED = 1,   // Center found and locked; soft trimming active
    SMART_FLOW_RECENTER = 2,        // Shifting center due to sustained drift
    SMART_FLOW_APPROACH = 4,        // Feed-forward jump + bounded secant corrections
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
    int approachPwm = 0;            // Last feed-forward command (0 = unused)
    uint8_t approachSecants = 0;
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

    // Aproximación por feed-forward: se arma cada vez que se pierde el centro.
    bool approachArmed = true;
    bool approachActive = false;
    bool approachBaseSettled = false;
    int approachCmd = 0;
    int approachBasePwm = 0;
    float approachBaseFlow = 0.0f;
    float approachExpectedDelta = 0.0f;
    float approachHistory[4] = {};
    uint8_t approachSamples = 0;
    uint8_t approachSecants = 0;
    uint32_t approachStartMs = 0;
    uint32_t approachRampMs = 0;

    // Fracción del salto inicial: queda por debajo del objetivo porque la
    // bomba no puede frenar rápido. 0,87 ≈ 1/1,15: una tabla con ganancia
    // 15 % optimista todavía no sobrepasa la consigna.
    static constexpr float kApproachFraction = 0.87f;
    static constexpr float kSecantGain = 0.90f;
    static constexpr uint8_t kMaxSecants = 2;
    // Régimen: la media de las 2 últimas muestras difiere < kApproachStableDelta
    // de la de las 2 anteriores y el flujo ya recorrió la mitad del cambio
    // esperado (el tramo muerto inicial también parece "estable"). Sin eso,
    // el timeout cubre tiempo muerto + 3τ del peor caso estimado.
    static constexpr uint32_t kApproachMinSettleMs = 600;
    static constexpr uint32_t kApproachMaxSettleMs = 4000;
    static constexpr float kApproachStableDelta = 0.05f;
    static constexpr float kApproachMinResponse = 0.5f;
    static constexpr int kSecantMinDeltaPwm = 20;
    static constexpr float kMinGainRatio = 0.6f;
    static constexpr float kMaxGainRatio = 1.6f;

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
        // Un centro enclavado deja una medida en régimen para la secante.
        if (!approachArmed)
            approachBaseSettled = centerFound;
        approachArmed = true;
        approachActive = false;
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
            approachArmed = false;
            approachActive = false;
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
        if (!preserveModel || !centerFound)
        {
            approachArmed = true;
            approachActive = false;
            approachBaseSettled = false;
        }

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

private:
    uint32_t rampMsFor(int deltaPwm) const
    {
        if (tune.rampCountsPerSecond <= 0.0f) return 0;
        return static_cast<uint32_t>(fabsf(static_cast<float>(deltaPwm)) * 1000.0f / tune.rampCountsPerSecond);
    }

    // Razón ganancia real / tabla medida en un régimen (flujo, PWM). Supone un
    // error multiplicativo de la tabla (batería, tubo, desgaste).
    float gainRatioAt(float flow, int pwm) const
    {
        const float tableFlow = tune.feedForward.flowForPwm(pwm);
        if (tableFlow <= 0.05f || flow <= 0.05f) return 1.0f;
        return clampFloat(flow / tableFlow, kMinGainRatio, kMaxGainRatio);
    }

    void startApproachStep(uint32_t nowMs, uint32_t rampMs, float expectedDelta)
    {
        approachSamples = 0;
        approachStartMs = nowMs;
        approachRampMs = rampMs;
        approachExpectedDelta = expectedDelta;
    }

    void beginApproach(uint32_t nowMs, int currentPwm, float targetFlow, float baseFlow, int maxPwm)
    {
        const FlowFeedForwardTable &ff = tune.feedForward;
        approachArmed = false;
        approachActive = true;
        approachSecants = 0;
        approachBasePwm = currentPwm;
        approachBaseFlow = baseFlow;

        const float partialFlow = baseFlow + kApproachFraction * (targetFlow - baseFlow);
        int cmd;
        if (approachBaseSettled)
        {
            // Desde un régimen conocido: paso relativo escalado por la ganancia
            // observada; conserva la deriva que ya corrigió el mantenimiento.
            const float ratio = gainRatioAt(baseFlow, currentPwm);
            cmd = currentPwm + ff.pwmForFlow(partialFlow / ratio) - ff.pwmForFlow(baseFlow / ratio);
            startApproachStep(nowMs, rampMsFor(cmd - currentPwm), partialFlow - baseFlow);
        }
        else
        {
            // Arranque: el PWM físico aún sube desde ~0 tras el kick y la bomba
            // puede no haber girado todavía; no bajar del umbral de arranque.
            cmd = ff.pwmForFlow(partialFlow);
            if (cmd < ff.startPwm()) cmd = ff.startPwm();
            startApproachStep(nowMs, rampMsFor(cmd), partialFlow - baseFlow);
        }
        approachCmd = clampInt(cmd, 0, maxPwm);
    }

    // Devuelve true cuando la aproximación terminó con el flujo ya dentro de
    // la banda: el PWM actual es el centro y puede enclavarse directamente.
    bool updateApproach(uint32_t nowMs, float targetFlow, float rawFlow, float settlingBand, int maxPwm)
    {
        approachHistory[3] = approachHistory[2];
        approachHistory[2] = approachHistory[1];
        approachHistory[1] = approachHistory[0];
        approachHistory[0] = rawFlow;
        if (approachSamples < UINT8_MAX) approachSamples++;

        const uint32_t elapsed = nowMs - approachStartMs;
        if (approachSamples < 4 || elapsed < approachRampMs + kApproachMinSettleMs)
            return false;
        const float flow = 0.5f * (approachHistory[0] + approachHistory[1]);
        const float older = 0.5f * (approachHistory[2] + approachHistory[3]);
        const bool stable = fabsf(flow - older) < kApproachStableDelta;
        const bool responded = fabsf(flow - approachBaseFlow) >= kApproachMinResponse * fabsf(approachExpectedDelta);
        if (!(stable && responded) && elapsed < approachRampMs + kApproachMaxSettleMs)
            return false;

        const float error = targetFlow - flow;
        if (fabsf(error) <= settlingBand || approachSecants >= kMaxSecants)
        {
            approachActive = false;
            return fabsf(error) <= settlingBand;
        }

        // Corrección secante acotada sobre la curva: el paso sale de la tabla
        // (respeta su no linealidad). Con base en régimen se escala por la
        // ganancia incremental del paso anterior; si no, por la ganancia
        // absoluta del punto actual.
        const FlowFeedForwardTable &ff = tune.feedForward;
        float tableStep;
        const int deltaPwm = approachCmd - approachBasePwm;
        if (approachBaseSettled && abs(deltaPwm) >= kSecantMinDeltaPwm)
        {
            const int tableDelta = ff.pwmForFlow(flow) - ff.pwmForFlow(approachBaseFlow);
            const float ratio = clampFloat(static_cast<float>(tableDelta) / static_cast<float>(deltaPwm),
                                           kMinGainRatio, kMaxGainRatio);
            tableStep = static_cast<float>(ff.pwmForFlow(targetFlow) - ff.pwmForFlow(flow)) / ratio;
        }
        else
        {
            const float ratio = gainRatioAt(flow, approachCmd);
            tableStep = static_cast<float>(ff.pwmForFlow(targetFlow / ratio) - ff.pwmForFlow(flow / ratio));
        }
        const int step = static_cast<int>(lroundf(kSecantGain * tableStep));
        approachBasePwm = approachCmd;
        approachBaseFlow = flow;
        approachBaseSettled = true;
        approachCmd = clampInt(approachCmd + step, 0, maxPwm);
        approachSecants++;
        startApproachStep(nowMs, rampMsFor(step), kSecantGain * error);
        return false;
    }

public:
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
            if (approachArmed && tune.feedForward.available())
                beginApproach(nowMs, currentPwm, targetFlow,
                              approachBaseSettled ? fastFlow : rawFlow, maxPwm);
            else
                approachArmed = false;

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

            const bool approachInBand = approachActive &&
                                        updateApproach(nowMs, targetFlow, rawFlow, settlingBand, maxPwm);

            if (approachActive)
            {
                mode = SMART_FLOW_APPROACH;
                nextPwm = approachCmd;
            }
            else if (approachInBand || settledTicks >= requiredSettledTicks)
            {
                if (approachInBand)
                {
                    // El EMA aún arrastra la subida; re-sincronizarlo evita que
                    // el trim reaccione a un error que ya no existe.
                    fastFlow = rawFlow;
                    slowFlow = rawFlow;
                }
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
                    approachArmed = true;
                    approachBaseSettled = false;
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
        status.approachPwm = approachCmd;
        status.approachSecants = approachSecants;
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
