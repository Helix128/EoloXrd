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
    // Zona muerta relativa a la consigna: la efectiva es
    // max(deadband, deadbandRel * consigna). 0 = sólo absoluta.
    float deadbandRel = 0.0f;
    // Error efectivo mínimo para contar deriva y recentrar.
    float driftThreshold = 0.05f;
    // true: dentro de la zona muerta el integral se congela (no decae).
    bool freezeIntegral = false;
    // Detector CUSUM de error sostenido en CENTER_LOCKED. cusumK = holgura
    // (L/min), cusumH = umbral (L/min acumulados). cusumH <= 0 lo desactiva.
    float cusumK = 0.0f;
    float cusumH = 0.0f;
    int cusumJumpMax = 0;           // Salto máximo de centro por alarma (cuentas)
    float cusumJumpFrac = 1.0f;     // Fracción del salto estimado (0,3..1); evita sobrecorrección
    uint32_t cusumHoldMs = 0;       // Espera tras un salto (0 = 2000 ms)
    // Ley continua (smoothLaw): PI incremental en dominio de flujo dividido
    // por la pendiente local de la planta (tabla x razón de ganancia
    // observada), así la ganancia del lazo no depende de la consigna. Sin
    // saltos de centro ni secantes: el PWM cambia a lo sumo smoothSlew
    // cuentas por muestra. Requiere tabla feedForward.
    bool smoothLaw = false;
    float smoothKi = 0.70f;         // 1/s: fracción del error corregida por segundo
    float smoothKp = 0.30f;         // Proporcional sobre la medición (adimensional, forma incremental)
    int smoothSlew = 40;            // Máx. |ΔPWM| por muestra tras el primer enclave
    int smoothSlewStart = 100;      // Máx. |ΔPWM| por muestra antes del primer enclave
    float smoothStartFraction = 0.95f; // Fracción de la consigna para el PWM inicial (tabla)
    uint32_t smoothHoldMs = 1100;   // Pausa del lazo tras un paso de tabla (+ rampa): tiempo muerto + 2τ
    float smoothKiStart = 0.70f;    // 1/s antes del enclave (arranque, cambio de consigna); kp se escala igual
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
    // Consigna fuera del alcance: PWM en el máximo efectivo con error positivo
    // sostenido.
    bool unreachable = false;
    float cusumPos = 0.0f;
    float cusumNeg = 0.0f;
    uint16_t cusumAlarms = 0;
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

    float cusumPos = 0.0f;
    float cusumNeg = 0.0f;
    uint16_t cusumSamples = 0;
    uint16_t cusumAlarms = 0;
    uint32_t cusumHoldUntilMs = 0;
    uint32_t saturatedSinceMs = 0;
    float jumpFrac = 1.0f;
    int lastJumpSign = 0;
    bool unreachable = false;
    static constexpr uint32_t kUnreachableConfirmMs = 3000;

    // Estado de la ley continua (smoothLaw).
    bool smoothActive = false;      // PWM interno inicializado
    float smoothPwm = 0.0f;         // PWM con fracción (evita ciclo límite por redondeo)
    float smoothRatio = 1.0f;       // Razón flujo real / tabla (EMA en régimen)
    float smoothPrevFlow = -1.0f;
    float smoothPrevError = 0.0f;
    uint32_t smoothHoldUntilMs = 0;
    int smoothOut = 0;
    bool smoothOutValid = false;
    uint8_t smoothInBand = 0;
    static constexpr float kSmoothLockBand = 0.02f;   // ±2 % para declarar enclave
    static constexpr uint8_t kSmoothLockSamples = 3;
    static constexpr float kSmoothRatioAlpha = 0.15f;
    static constexpr float kSmoothQuasiSteady = 0.03f; // |Δflujo| / consigna por muestra
    static constexpr float kSmoothDyDeadband = 0.025f; // L/min: oscilación de lectura de 2-3 pasos de resolución

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

    void resetCusum()
    {
        cusumPos = 0.0f;
        cusumNeg = 0.0f;
        cusumSamples = 0;
    }

    void resetJumpDamping()
    {
        jumpFrac = clampFloat(tune.cusumJumpFrac, 0.3f, 1.0f);
        lastJumpSign = 0;
    }

    void resetCusumState()
    {
        resetCusum();
        cusumHoldUntilMs = 0;
    }

    // Cuentas PWM por L/min alrededor del punto de operación. Usa la pendiente
    // de la tabla corregida por la razón de ganancia observada; sin tabla,
    // aproxima la planta como proporcional (centro / flujo).
    float countsPerLpm(float flow, int pwm) const
    {
        const FlowFeedForwardTable &ff = tune.feedForward;
        if (ff.available())
        {
            const float ratio = gainRatioAt(flow, pwm);
            const float f0 = flow / ratio;
            const int p0 = ff.pwmForFlow(f0 - 0.25f);
            const int p1 = ff.pwmForFlow(f0 + 0.25f);
            if (p1 > p0)
                return static_cast<float>(p1 - p0) / (0.5f * ratio);
        }
        if (flow > 0.1f && pwm > 0)
            return static_cast<float>(pwm) / flow;
        return 0.0f;
    }

public:
    void setTune(const SmartFlowTune &newTune)
    {
        tune = newTune;
        resetJumpDamping();
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
        resetCusumState();
        resetJumpDamping();
        saturatedSinceMs = 0;
        unreachable = false;
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
        resetCusumState();
        resetJumpDamping();
        saturatedSinceMs = 0;
        unreachable = false;
        // Ley continua: con preserveModel (fin de kick, fallo de sensor) se
        // retoma desde el mismo PWM interno; sin él, se reinicia desde la tabla.
        smoothPrevFlow = -1.0f;
        smoothPrevError = 0.0f;
        smoothHoldUntilMs = 0;
        if (!preserveModel)
        {
            smoothActive = false;
            smoothPwm = 0.0f;
            smoothRatio = 1.0f;
            smoothInBand = 0;
            smoothOutValid = false;
        }
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

    // Pendiente local (L/min por cuenta) de la tabla en pwm, escalada por la
    // razón de ganancia observada. Acotada para que un tramo plano de la
    // tabla no produzca pasos enormes.
    float smoothSlope(float pwm, int maxPwm) const
    {
        const FlowFeedForwardTable &ff = tune.feedForward;
        const int lo = clampInt(static_cast<int>(pwm) - 60, ff.startPwm(), maxPwm);
        const int hi = clampInt(static_cast<int>(pwm) + 60, ff.startPwm(), maxPwm);
        float slope = 0.0f;
        if (hi > lo)
            slope = (ff.flowForPwm(hi) - ff.flowForPwm(lo)) / static_cast<float>(hi - lo);
        slope *= smoothRatio;
        return clampFloat(slope, 0.0015f, 0.02f);
    }

    void startSmoothHold(uint32_t nowMs, int deltaPwm)
    {
        const uint32_t hold = tune.smoothHoldMs + rampMsFor(deltaPwm > 0 ? deltaPwm : 0);
        smoothHoldUntilMs = hold ? ((nowMs + hold) ? (nowMs + hold) : 1) : 0;
    }

    // Ley continua: PI incremental normalizado por la pendiente de la planta.
    // El PWM interno conserva fracciones; la salida cambia a lo sumo
    // smoothSlew cuentas por muestra. Los únicos pasos de tabla son el
    // arranque y los cambios de consigna, seguidos de una pausa que deja
    // responder a la planta antes de integrar.
    SmartFlowStatus updateSmooth(SmartFlowStatus &status, uint32_t nowMs, int currentPwm, float targetFlow, float rawFlow,
                                 float dtSeconds, int maxPwm, float errorFast, float errorSlow, float derivative)
    {
        const FlowFeedForwardTable &ff = tune.feedForward;
        const int floorPwm = clampInt(ff.startPwm(), 0, maxPwm);
        const bool targetChanged = previousTargetFlow >= 0.0f && fabsf(targetFlow - previousTargetFlow) > 0.001f;
        float pTerm = 0.0f;
        float iTerm = 0.0f;
        int nextPwm;

        if (!smoothActive || targetChanged)
        {
            const float ratio = clampFloat(smoothRatio, kMinGainRatio, kMaxGainRatio);
            const float before = smoothPwm;
            if (!smoothActive)
            {
                const float frac = clampFloat(tune.smoothStartFraction, 0.5f, 1.0f);
                smoothPwm = static_cast<float>(ff.pwmForFlow(frac * targetFlow / ratio));
            }
            else
            {
                smoothPwm += static_cast<float>(ff.pwmForFlow(targetFlow / ratio) -
                                                ff.pwmForFlow(previousTargetFlow / ratio));
            }
            smoothPwm = clampFloat(smoothPwm, static_cast<float>(floorPwm), static_cast<float>(maxPwm));
            startSmoothHold(nowMs, static_cast<int>(smoothPwm - (smoothActive ? before : static_cast<float>(currentPwm))));
            smoothActive = true;
            smoothInBand = 0;
            centerFound = false;
            mode = SMART_FLOW_APPROACH;
        }
        else
        {
            const float dt = clampFloat(dtSeconds, 0.05f, 1.0f);
            const float error = targetFlow - rawFlow;
            const float dy = smoothPrevFlow >= 0.0f ? rawFlow - smoothPrevFlow : 0.0f;
            const bool holding = smoothHoldUntilMs != 0 && static_cast<int32_t>(nowMs - smoothHoldUntilMs) < 0;
            if (!holding) smoothHoldUntilMs = 0;
            const bool lowFlow = rawFlow < 0.2f || rawFlow < 0.1f * targetFlow;

            // Razón de ganancia real/tabla, sólo en régimen cuasi estacionario.
            if (!holding && !lowFlow && fabsf(dy) < kSmoothQuasiSteady * targetFlow)
            {
                const float table = ff.flowForPwm(static_cast<int>(smoothPwm + 0.5f));
                if (table > 0.05f)
                {
                    const float r = clampFloat(rawFlow / table, kMinGainRatio, kMaxGainRatio);
                    smoothRatio += kSmoothRatioAlpha * (r - smoothRatio);
                }
            }

            float deadband = tune.deadband;
            if (tune.deadbandRel > 0.0f && tune.deadbandRel * targetFlow > deadband)
                deadband = tune.deadbandRel * targetFlow;

            float dFlow = 0.0f;
            if (!holding)
            {
                // Antes del enclave (arranque, cambio de consigna) I y P se
                // escalan juntos: la anticipación kp/ki (≈ τ de la planta)
                // se conserva y el flujo que aún sube no se integra de más.
                const float ki = centerFound ? tune.smoothKi : tune.smoothKiStart;
                const float kp = tune.smoothKi > 0.0f ? tune.smoothKp * ki / tune.smoothKi : tune.smoothKp;
                if (fabsf(error) > deadband)
                    iTerm = ki * dt * error;
                // Zona muerta suave sobre Δflujo: el ruido y la resolución
                // (0,01 L/min) no mueven el PWM; un cambio real sí.
                const float dyDead = fmaxf(0.0f, fabsf(dy) - kSmoothDyDeadband);
                if (smoothPrevFlow >= 0.0f)
                    pTerm = -kp * (dy > 0.0f ? dyDead : -dyDead);
                dFlow = iTerm + pTerm;
            }
            // Línea tapada o bomba sin girar: no empujar el PWM hacia arriba.
            if (lowFlow && dFlow > 0.0f) dFlow = 0.0f;

            float dPwm = dFlow / smoothSlope(smoothPwm, maxPwm);
            const int slewCfg = centerFound ? tune.smoothSlew : tune.smoothSlewStart;
            const float slew = static_cast<float>(slewCfg > 0 ? slewCfg : 40);
            dPwm = clampFloat(dPwm, -slew, slew);
            smoothPwm = clampFloat(smoothPwm + dPwm, static_cast<float>(floorPwm), static_cast<float>(maxPwm));
            // La histéresis de salida retiene hasta 2 cuentas: el paso real
            // se mide contra la salida, no contra el PWM interno.
            if (smoothOutValid)
                smoothPwm = clampFloat(smoothPwm, static_cast<float>(smoothOut) - slew,
                                       static_cast<float>(smoothOut) + slew);

            if (fabsf(error) <= kSmoothLockBand * targetFlow)
            {
                if (smoothInBand < UINT8_MAX) smoothInBand++;
            }
            else
            {
                smoothInBand = 0;
            }
            if (!centerFound && smoothInBand >= kSmoothLockSamples)
                centerFound = true;
            mode = centerFound ? SMART_FLOW_CENTER_LOCKED : (holding ? SMART_FLOW_APPROACH : SMART_FLOW_SEEK_CENTER);
        }

        smoothPrevError = targetFlow - rawFlow;
        smoothPrevFlow = rawFlow;
        previousTargetFlow = targetFlow;
        nextPwm = clampInt(static_cast<int>(lroundf(smoothPwm)), 0, maxPwm);
        // Histéresis de 2 cuentas (≈0,006 L/min, bajo la resolución del
        // sensor): el redondeo y el ruido no hacen parpadear la salida.
        if (smoothOutValid && abs(nextPwm - smoothOut) <= 2 &&
            fabsf(smoothPwm - static_cast<float>(smoothOut)) < 2.0f)
            nextPwm = smoothOut;
        smoothOut = nextPwm;
        smoothOutValid = true;
        centerPwm = nextPwm;

        if (nextPwm >= maxPwm && errorFast > tune.deadband)
        {
            if (saturatedSinceMs == 0) saturatedSinceMs = nowMs ? nowMs : 1;
            else if (nowMs - saturatedSinceMs >= kUnreachableConfirmMs) unreachable = true;
        }
        else
        {
            saturatedSinceMs = 0;
            unreachable = false;
        }

        status.pwm = nextPwm;
        status.centerPwm = nextPwm;
        status.trimPwm = 0;
        status.pwmFF = nextPwm;
        status.pidCorrection = 0;
        status.mode = mode;
        status.centerFound = centerFound;
        status.fastFlow = fastFlow;
        status.slowFlow = slowFlow;
        status.errorFast = errorFast;
        status.errorSlow = errorSlow;
        status.integral = smoothRatio;      // Razón de ganancia observada
        status.derivativeFlow = derivative;
        status.pTerm = pTerm;
        status.iTerm = iTerm;
        status.estimatedGain = smoothSlope(smoothPwm, maxPwm);
        status.confidence = centerFound ? 1.0f : 0.2f;
        status.modelValid = smoothActive;
        status.lowerClamp = nextPwm <= floorPwm;
        status.upperSaturation = nextPwm >= maxPwm;
        status.minFlowPwm = nextPwm;
        status.maxUsefulPwm = maxPwm;
        status.unreachable = unreachable;
        return status;
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

        if (tune.smoothLaw && tune.feedForward.available())
            return updateSmooth(status, nowMs, currentPwm, targetFlow, rawFlow, dtSeconds, maxPwm,
                                errorFast, errorSlow, derivative);

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
                resetCusumState();
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
                float deadband = tune.deadband;
                if (tune.deadbandRel > 0.0f && tune.deadbandRel * targetFlow > deadband)
                    deadband = tune.deadbandRel * targetFlow;
                float effectiveError = 0.0f;
                if (fabsf(errorFast) > deadband)
                {
                    effectiveError = (errorFast > 0.0f) ? (errorFast - deadband) : (errorFast + deadband);
                }

                // CUSUM sobre el error crudo: detecta un desplazamiento
                // sostenido del punto de operación (filtro, batería, desgaste)
                // que el trim acotado no alcanza a compensar. La alarma absorbe
                // el trim en el centro y suma un salto acotado estimado con la
                // pendiente de la planta.
                if (tune.cusumH > 0.0f && cusumHoldUntilMs != 0 &&
                    static_cast<int32_t>(nowMs - cusumHoldUntilMs) < 0)
                {
                    // Tras un salto: esperar tiempo muerto + respuesta de la
                    // planta antes de volver a acumular.
                    resetCusum();
                }
                else if (tune.cusumH > 0.0f && (rawFlow < 0.2f || rawFlow < 0.1f * targetFlow))
                {
                    // Flujo casi nulo (línea tapada): pausar; un salto aquí
                    // llevaría la bomba al PWM máximo sin efecto.
                    resetCusum();
                }
                else if (tune.cusumH > 0.0f)
                {
                    cusumHoldUntilMs = 0;
                    const float e = targetFlow - rawFlow;
                    cusumPos = fmaxf(0.0f, cusumPos + e - tune.cusumK);
                    cusumNeg = fmaxf(0.0f, cusumNeg - e - tune.cusumK);
                    if (cusumSamples < UINT16_MAX) cusumSamples++;
                    const bool alarmPos = cusumPos > tune.cusumH;
                    const bool alarmNeg = cusumNeg > tune.cusumH;
                    if (alarmPos || alarmNeg)
                    {
                        // Error medio desde el último reinicio (>= k por definición).
                        const float meanErr = (alarmPos ? 1.0f : -1.0f) *
                            (tune.cusumK + (alarmPos ? cusumPos : cusumNeg) / static_cast<float>(cusumSamples));
                        const float slope = countsPerLpm(fastFlow, centerPwm + static_cast<int>(currentTrim));
                        // Anti-rebote: si el signo se invierte entre saltos
                        // consecutivos, reducir la fracción aplicada.
                        const int jumpSign = alarmPos ? 1 : -1;
                        if (lastJumpSign != 0 && jumpSign != lastJumpSign)
                            jumpFrac = fmaxf(0.3f, jumpFrac * 0.7f);
                        lastJumpSign = jumpSign;
                        int jump = static_cast<int>(lroundf(slope * meanErr * jumpFrac));
                        const int jumpMax = tune.cusumJumpMax > 0 ? tune.cusumJumpMax : 200;
                        jump = clampInt(jump, -jumpMax, jumpMax);
                        centerPwm = clampInt(centerPwm + static_cast<int>(currentTrim) + jump, 0, maxPwm);
                        currentTrim = 0.0f;
                        integral = 0.0f;
                        isDrifting = false;
                        driftStartMs = 0;
                        if (cusumAlarms < UINT16_MAX) cusumAlarms++;
                        mode = SMART_FLOW_RECENTER;
                        resetCusum();
                        const uint32_t hold = (tune.cusumHoldMs > 0 ? tune.cusumHoldMs : 2000UL) +
                                              rampMsFor(jump);
                        cusumHoldUntilMs = (nowMs + hold) ? (nowMs + hold) : 1;
                    }
                }

                // Detección de deriva sostenida para auto-recentrado suave
                if (tune.recenterDelayMs > 0 && fabsf(effectiveError) > tune.driftThreshold)
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
                else if (!tune.freezeIntegral)
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

        // Consigna inalcanzable: mando en el máximo efectivo y aún por debajo.
        if (nextPwm >= maxPwm && errorFast > tune.deadband)
        {
            if (saturatedSinceMs == 0) saturatedSinceMs = nowMs ? nowMs : 1;
            else if (nowMs - saturatedSinceMs >= kUnreachableConfirmMs) unreachable = true;
        }
        else
        {
            saturatedSinceMs = 0;
            unreachable = false;
        }

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
        status.unreachable = unreachable;
        status.cusumPos = cusumPos;
        status.cusumNeg = cusumNeg;
        status.cusumAlarms = cusumAlarms;

        return status;
    }
};

#endif // EOLO_SMART_FLOW_CONTROLLER_H
