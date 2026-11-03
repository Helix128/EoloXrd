#ifndef CALIBRATION_SCENE_H
#define CALIBRATION_SCENE_H

#include "../../Drawing/Fonts.h"
#include "../../../Variants/Legacy.h"
#include "../IScene.h"
#include "../../../Common/Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/Renderer.h"
#include <math.h>

class CalibrationScene : public IScene
{
private:
    enum CalibrationState
    {
        INTRO,
        INIT,
        DISCOVERY_STABILIZE,
        DISCOVERY_SAMPLE,
        RAMP_KICK,
        RAMP_STABILIZE,
        RAMP_SAMPLE,
        SAVING,
        COMPLETE,
        PAUSED,
        FAILED
    };

    CalibrationState state = INIT;
    CalibrationState pausedFromState = INIT;

    // Discovery: medir cada motor por separado para determinar cuál es más débil
    int discoveryMotor = 0;       // 0 o 1: cuál motor se está probando
    float discoveryFlow[2] = {0}; // flujo medido para cada motor

    // Ramp: recorre PWM de 0→MAX primero el débil, luego el fuerte
    int currentPwm = 0;
    bool rampingStrong = false; // false = ramp del débil, true = ramp del fuerte

    // Sampling state
    int sampleCount = 0;
    int sampleAttempts = 0;
    float sumFlow = 0.0f;
    float sumSqFlow = 0.0f;
    unsigned long lastSampleTime = 0;
    unsigned long lastFreshSourceTime = 0;
    unsigned long stateStartTime = 0;

    // Stabilization state
    float lastFlowForStab = -1.0f;
    float lastAcceptedRampFlow = -1.0f;
    int stableCount = 0;
    int unstablePoints = 0;
    int rejectedPoints = 0;
    int plateauCount = 0;
    bool pauseCancelSelected = false;
    bool introMeasureSelected = false;
    bool scanStarted = false;
    const char *failureMessage = "Curva no guardada";
    CalibrationManager previousCalibration;
    unsigned long lastStabCheckTime = 0;

    static const int SAMPLES_PER_POINT = 4;
    static const int MAX_SAMPLE_ATTEMPTS = 28;
    static const int SAMPLE_INTERVAL_MS = 200;
    static const int STAB_CHECK_INTERVAL_MS = 100;
    static const int MAX_STABILIZE_TIME_MS = 4500;
    static const int REQUIRED_STABLE_READINGS = 3;
    static const int MAX_ACCEPT_SAMPLE_AGE_MS = 250;
    static const int KICK_TIME_MS = 350;
    static const int KICK_PWM = MAX_PWM;
    static const int KICK_BELOW_PWM = MAX_PWM / 4;
    constexpr static const float STAB_TOLERANCE = 0.05f;
    constexpr static const float MIN_VALID_FLOW = 0.05f;
    constexpr static const float MIN_FLOW_DELTA = 0.04f;
    constexpr static const float MAX_FLOW_STDDEV = 0.15f;
    static const int DISCOVERY_PWM = MAX_PWM / 2;

    int basePwmStep(int pwm) const
    {
        if (pwm < MAX_PWM / 4)
            return max(1, MAX_PWM / 90);
        if (pwm < MAX_PWM / 2)
            return max(1, MAX_PWM / 50);
        return max(1, MAX_PWM / 28);
    }

    int getPwmStep(int pwm, float flowDelta) const
    {
        int step = basePwmStep(pwm);
        if (flowDelta >= 0.0f && flowDelta < MIN_FLOW_DELTA * 1.5f)
            step *= 2;
        else if (flowDelta > 0.25f)
            step = max(1, step / 2);
        return constrain(step, max(1, MAX_PWM / 160), max(1, MAX_PWM / 14));
    }

    int getPwmStep(int pwm) const
    {
        return getPwmStep(pwm, -1.0f);
    }

    void resetSampling()
    {
        sampleCount = 0;
        sampleAttempts = 0;
        sumFlow = 0.0f;
        sumSqFlow = 0.0f;
        lastSampleTime = millis();
        lastFreshSourceTime = 0;
        stableCount = 0;
        lastFlowForStab = -1.0f;
        lastStabCheckTime = millis();
        stateStartTime = millis();
    }

    int weakMotor() const { return discoveryFlow[0] <= discoveryFlow[1] ? 0 : 1; }
    int strongMotor() const { return 1 - weakMotor(); }

    bool readFreshFlow(Context &ctx, FlowData &fd)
    {
        if (!ctx.components.flowSensor.getData(fd) || !fd.valid || fd.stale)
            return false;
        if (fd.ageMs > (uint32_t)MAX_ACCEPT_SAMPLE_AGE_MS)
            return false;

        unsigned long sourceTime = millis() - fd.ageMs;
        if (lastFreshSourceTime != 0)
        {
            unsigned long delta = sourceTime > lastFreshSourceTime ? sourceTime - lastFreshSourceTime : lastFreshSourceTime - sourceTime;
            if (delta < (unsigned long)STAB_CHECK_INTERVAL_MS)
                return false;
        }
        lastFreshSourceTime = sourceTime;
        return true;
    }

    bool hasInitialFlow(Context &ctx)
    {
        FlowData fd;
#ifdef FEATURE_FLOW_FS3000
        ctx.components.flowSensor.getData(fd);
        return fd.sampleId != 0 && !fd.stale &&
               fd.ageMs <= EoloConfig::flowPid.sensorStaleMs &&
               isfinite(fd.velocity) && fd.velocity >= 0.0f;
#else
        const bool dataAvailable = ctx.components.flowSensor.getData(fd);
        return dataAvailable && fd.valid && !fd.stale &&
               fd.ageMs <= EoloConfig::flowPid.sensorStaleMs;
#endif
    }

    void applyMotorsForCurrentPoint(Context &ctx)
    {
        int weak = weakMotor();
        int strong = strongMotor();

        if (!rampingStrong)
        {
            // Rampa del motor débil, motor fuerte apagado
            ctx.components.motor.setMotorPwm(weak, currentPwm);
            ctx.components.motor.setMotorPwm(strong, 0);
        }
        else
        {
            // Motor débil queda al máximo, rampa del motor fuerte
            ctx.components.motor.setMotorPwm(weak, MAX_PWM);
            ctx.components.motor.setMotorPwm(strong, currentPwm);
        }
    }

    bool shouldKickCurrentPoint() const
    {
        return currentPwm > 0 && currentPwm < KICK_BELOW_PWM;
    }

    void applyKickForCurrentPoint(Context &ctx)
    {
        int weak = weakMotor();
        int strong = strongMotor();
        if (!rampingStrong)
        {
            ctx.components.motor.setMotorPwm(weak, KICK_PWM);
            ctx.components.motor.setMotorPwm(strong, 0);
        }
        else
        {
            ctx.components.motor.setMotorPwm(weak, MAX_PWM);
            ctx.components.motor.setMotorPwm(strong, KICK_PWM);
        }
    }

    void startRampPoint(Context &ctx)
    {
        if (shouldKickCurrentPoint())
        {
            applyKickForCurrentPoint(ctx);
            stateStartTime = millis();
            state = RAMP_KICK;
            return;
        }
        applyMotorsForCurrentPoint(ctx);
        resetSampling();
        state = RAMP_STABILIZE;
    }

    bool recordPoint(Context &ctx, float measuredFlow, float stddev)
    {
        if (ctx.calibration.numPoints >= CalibrationManager::MAX_POINTS)
            return false;
        if (measuredFlow < MIN_VALID_FLOW || stddev > MAX_FLOW_STDDEV)
        {
            rejectedPoints++;
            LOG_F("Punto rechazado: flujo %.3f sd %.3f\n", measuredFlow, stddev);
            return false;
        }
        if (lastAcceptedRampFlow >= 0.0f && measuredFlow - lastAcceptedRampFlow < MIN_FLOW_DELTA)
        {
            rejectedPoints++;
            LOG_F("Punto rechazado: delta %.3f L/min\n", measuredFlow - lastAcceptedRampFlow);
            return false;
        }

        int weak = weakMotor();
        int strong = strongMotor();
        int idx = ctx.calibration.numPoints;

        if (!rampingStrong)
        {
            // Motor débil a currentPwm, fuerte a 0
            ctx.calibration.pwm0[idx] = (weak == 0) ? currentPwm : 0;
            ctx.calibration.pwm1[idx] = (weak == 1) ? currentPwm : 0;
        }
        else
        {
            // Motor débil a MAX, fuerte a currentPwm
            ctx.calibration.pwm0[idx] = (weak == 0) ? MAX_PWM : currentPwm;
            ctx.calibration.pwm1[idx] = (weak == 1) ? MAX_PWM : currentPwm;
        }
        ctx.calibration.flows[idx] = measuredFlow;
        ctx.calibration.numPoints++;
        lastAcceptedRampFlow = measuredFlow;

        LOG_F("Punto %d: pwm[%d,%d] -> %.3f L/min sd %.3f\n",
              idx, ctx.calibration.pwm0[idx], ctx.calibration.pwm1[idx], measuredFlow, stddev);
        return true;
    }

    void compactCalibration(Context &ctx)
    {
        int write = 0;
        float previousFlow = -1.0f;
        int previousTotalPwm = -1;
        for (int read = 0; read < ctx.calibration.numPoints; read++)
        {
            int totalPwm = ctx.calibration.pwm0[read] + ctx.calibration.pwm1[read];
            if (previousFlow >= 0.0f && ctx.calibration.flows[read] - previousFlow < MIN_FLOW_DELTA)
                continue;
            if (previousTotalPwm >= 0 && totalPwm < previousTotalPwm)
                continue;
            if (write != read)
            {
                ctx.calibration.pwm0[write] = ctx.calibration.pwm0[read];
                ctx.calibration.pwm1[write] = ctx.calibration.pwm1[read];
                ctx.calibration.flows[write] = ctx.calibration.flows[read];
            }
            previousFlow = ctx.calibration.flows[write];
            previousTotalPwm = totalPwm;
            write++;
        }
        int removed = ctx.calibration.numPoints - write;
        ctx.calibration.numPoints = write;
        if (removed > 0)
            LOG_F("Calibración: %d puntos compactados por monotonía\n", removed);
    }

    float getOverallProgress() const
    {
        // Discovery: 0-10%, weak ramp: 10-55%, strong ramp: 55-100%
        if (state <= DISCOVERY_SAMPLE)
        {
            return (discoveryMotor * 5.0f);
        }
        float pwmPct = (float)currentPwm / (float)MAX_PWM * 100.0f;
        if (!rampingStrong)
        {
            return 10.0f + pwmPct * 0.45f;
        }
        return 55.0f + pwmPct * 0.45f;
    }

    int estimateRemainingPoints() const
    {
        int points = 0;
        int pwm = currentPwm;
        while (pwm <= MAX_PWM)
        {
            points++;
            pwm += getPwmStep(pwm);
        }
        if (!rampingStrong)
        {
            // También sumar la rampa del motor fuerte
            pwm = 0;
            while (pwm <= MAX_PWM)
            {
                points++;
                pwm += getPwmStep(pwm);
            }
        }
        return points;
    }

    void failCalibration(Context &ctx, const char *message)
    {
        ctx.components.motor.setPwmImmediate(0);
        ctx.calibration = previousCalibration;
        scanStarted = false;
        failureMessage = message;
        state = FAILED;
    }

    void startCalibration(Context &ctx)
    {
        if (!hasInitialFlow(ctx))
        {
            ctx.components.motor.setPwmImmediate(0);
            ctx.calibration = previousCalibration;
            failureMessage = "Sin dato de flujo";
            state = FAILED;
            return;
        }

        unstablePoints = 0;
        rejectedPoints = 0;
        lastAcceptedRampFlow = -1.0f;
        discoveryMotor = 0;
        discoveryFlow[0] = discoveryFlow[1] = 0.0f;
        rampingStrong = false;
        currentPwm = 0;
        plateauCount = 0;
        scanStarted = true;
        failureMessage = "Curva no guardada";
        ctx.calibration.isLoaded = false;
        ctx.calibration.numPoints = 0;
        ctx.components.motor.setPwmImmediate(0);
        state = INIT;
        LOG_LN("Iniciando calibración: discovery + rampa continua");
    }

    void renderIntro(Context &ctx)
    {
        Renderer::beginFrame(ctx.u8g2);
        GUI::displayHeader(ctx);

        Renderer::centeredText(ctx.u8g2, "Control automatico", Layout::CenterX, 24, FONT_VALUE);
        if (!EoloConfig::useCalibrationSeed)
        {
            Renderer::centeredText(ctx.u8g2,
                                   EoloConfig::flowPid.feedForward.available() ? "Tabla del modelo" : "Ajuste por caudal",
                                   Layout::CenterX, 38, FONT_VALUE);
            Renderer::centeredText(ctx.u8g2, "Sin barrido manual", Layout::CenterX, 49, FONT_VALUE);
            Renderer::centeredText(ctx.u8g2, "Volver", Layout::CenterX, Layout::FooterValueY, FONT_VALUE);
        }
        else
        {
            Renderer::centeredText(ctx.u8g2, "Curva opcional de arranque", Layout::CenterX, 35, FONT_VALUE);
            Renderer::centeredText(ctx.u8g2, "Curva anterior intacta", Layout::CenterX, 44, FONT_VALUE);

            constexpr int boxY = 47;
            constexpr int boxH = 13;
            constexpr int boxW = 48;
            constexpr int gap = 8;
            const int startX = (Layout::ScreenW - 2 * boxW - gap) / 2;
            const char *labels[] = {"Medir", "Volver"};
            for (int i = 0; i < 2; ++i)
            {
                const bool selected = (i == 0) == introMeasureSelected;
                const int x = startX + i * (boxW + gap);
                ctx.u8g2.setDrawColor(1);
                if (selected)
                {
                    ctx.u8g2.drawBox(x, boxY, boxW, boxH);
                    ctx.u8g2.setDrawColor(0);
                }
                else
                {
                    ctx.u8g2.drawFrame(x, boxY, boxW, boxH);
                }
                ctx.u8g2.setFont(selected ? FONT_LABEL : FONT_VALUE);
                ctx.u8g2.drawStr(x + (boxW - ctx.u8g2.getStrWidth(labels[i])) / 2, 57, labels[i]);
                ctx.u8g2.setDrawColor(1);
            }
        }
        Renderer::endFrame(ctx.u8g2);
    }

    void renderPaused(Context &ctx)
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);
        Renderer::yesNoChoice(ctx.u8g2, "Pausa segura", !pauseCancelSelected,
                              "Curva anterior se conserva", "Seguir", "Salir");
        ctx.u8g2.sendBuffer();
    }

    void renderTerminal(Context &ctx)
    {
        Renderer::beginFrame(ctx.u8g2);
        GUI::displayHeader(ctx);
        if (state == COMPLETE)
        {
            Renderer::centeredText(ctx.u8g2, "Curva guardada", Layout::CenterX, 24, FONT_TITLE);
            char pointCount[20];
            snprintf(pointCount, sizeof(pointCount), "%d puntos", ctx.calibration.numPoints);
            Renderer::centeredText(ctx.u8g2, pointCount, Layout::CenterX, 42, FONT_VALUE);
        }
        else
        {
            Renderer::centeredText(ctx.u8g2, "Error de curva", Layout::CenterX, 24, FONT_TITLE);
            Renderer::centeredText(ctx.u8g2, failureMessage, Layout::CenterX, 38, FONT_VALUE);
            Renderer::centeredText(ctx.u8g2, "Curva anterior intacta", Layout::CenterX, 48, FONT_VALUE);
        }
        Renderer::centeredText(ctx.u8g2, "Volver", Layout::CenterX, Layout::FooterValueY, FONT_VALUE);
        Renderer::endFrame(ctx.u8g2);
    }

    void resumePaused(Context &ctx)
    {
        switch (pausedFromState)
        {
        case DISCOVERY_STABILIZE:
        case DISCOVERY_SAMPLE:
            ctx.components.motor.setMotorPwm(0, discoveryMotor == 0 ? DISCOVERY_PWM : 0);
            ctx.components.motor.setMotorPwm(1, discoveryMotor == 1 ? DISCOVERY_PWM : 0);
            resetSampling();
            state = DISCOVERY_STABILIZE;
            break;
        case RAMP_KICK:
        case RAMP_STABILIZE:
        case RAMP_SAMPLE:
            startRampPoint(ctx);
            break;
        case SAVING:
            state = SAVING;
            break;
        default:
            state = INIT;
            break;
        }
    }

public:
    static constexpr const char *Name = "calibration";

    uint16_t frameIntervalMs() const override { return 100; }

    void exit(Context &ctx) override
    {
        ctx.components.motor.setPwmImmediate(0);
        if (scanStarted && state != COMPLETE && state != FAILED)
            ctx.calibration = previousCalibration;
        scanStarted = false;
    }

    void enter(Context &ctx) override
    {
        previousCalibration = ctx.calibration;
        introMeasureSelected = false;
        pauseCancelSelected = true;
        scanStarted = false;
        failureMessage = "Curva no guardada";
        state = INTRO;
    }

    void update(Context &ctx) override
    {
        if (state == INTRO)
        {
            if (EoloConfig::useCalibrationSeed && ctx.components.input.getEncoderDelta() != 0)
                introMeasureSelected = !introMeasureSelected;
            if (ctx.components.input.isButtonPressed())
            {
                if (EoloConfig::useCalibrationSeed && introMeasureSelected)
                {
                    startCalibration(ctx);
                    return;
                }
                else
                {
                    SceneManager::setScene("inicio", ctx);
                    return;
                }
            }
            else
            {
                renderIntro(ctx);
                return;
            }
        }

        // El botón durante el barrido siempre corta las bombas de inmediato.
        // El siguiente toque confirma continuar/cancelar en vez de perder una
        // curva previamente guardada.
        if (state != COMPLETE && state != PAUSED && state != FAILED && ctx.components.input.isButtonPressed())
        {
            pausedFromState = state;
            ctx.components.motor.setPwmImmediate(0);
            pauseCancelSelected = true;
            state = PAUSED;
            renderPaused(ctx);
            return;
        }
        if (state == PAUSED)
        {
            if (ctx.components.input.getEncoderDelta() != 0) pauseCancelSelected = !pauseCancelSelected;
            if (ctx.components.input.isButtonPressed())
            {
                if (pauseCancelSelected)
                {
                    ctx.calibration = previousCalibration;
                    scanStarted = false;
                    SceneManager::setScene("inicio", ctx);
                    return;
                }
                else
                {
                    resumePaused(ctx);
                }
                return;
            }
            renderPaused(ctx);
            return;
        }
        if (state == COMPLETE || state == FAILED)
        {
            if (ctx.components.input.isButtonPressed())
            {
                SceneManager::setScene("inicio", ctx);
                return;
            }
            renderTerminal(ctx);
            return;
        }
        if (state == SAVING)
            ctx.components.motor.setPwmImmediate(0);

        // === RENDER ===
        Renderer::beginFrame(ctx.u8g2);
        GUI::displayHeader(ctx);

        if (state == SAVING)
        {
            Renderer::centeredText(ctx.u8g2, "Guardando curva", Layout::CenterX, 24, FONT_TITLE);
            Renderer::centeredText(ctx.u8g2, "Bombas apagadas", Layout::CenterX, 39, FONT_VALUE);
            Renderer::centeredText(ctx.u8g2, "No apagues EOLO", Layout::CenterX, 53, FONT_VALUE);
        }
        else
        {
            Renderer::centeredText(ctx.u8g2, "Calibrando", Layout::CenterX, 24, FONT_TITLE);

            ctx.u8g2.setFont(FONT_VALUE);
            const char *phaseLabel;
            if (state <= DISCOVERY_SAMPLE)
                phaseLabel = "Descubrimiento";
            else if (!rampingStrong)
                phaseLabel = "Rampa motor debil";
            else
                phaseLabel = "Rampa motor fuerte";

            Renderer::centeredText(ctx.u8g2, phaseLabel, Layout::CenterX, 35, FONT_VALUE);

            int progress = constrain((int)getOverallProgress(), 0, 100);
            constexpr int barW = 100;
            constexpr int barH = 7;
            constexpr int barX = (Layout::ScreenW - barW) / 2;
            constexpr int barY = 39;
            Renderer::progressBar(ctx.u8g2, barX, barY, barW, barH, progress);

            char progressText[8];
            snprintf(progressText, sizeof(progressText), "%d%%", progress);

            int remaining = estimateRemainingPoints();
            unsigned long perPointMs = MAX_STABILIZE_TIME_MS + SAMPLES_PER_POINT * SAMPLE_INTERVAL_MS;
            unsigned long remainSec = ((unsigned long)remaining * perPointMs + 999) / 1000;
            char etaText[24];
            snprintf(etaText, sizeof(etaText), "~%lu:%02lu", remainSec / 60, remainSec % 60);

            char footer[24];
            snprintf(footer, sizeof(footer), "%s  %s", progressText, etaText);
            Renderer::centeredText(ctx.u8g2, footer, Layout::CenterX, 60, FONT_VALUE);
        }

        Renderer::endFrame(ctx.u8g2);

        // === STATE MACHINE ===
        switch (state)
        {
        case INIT:
        {
            // Encender solo el motor a descubrir
            discoveryMotor = 0;
            ctx.components.motor.setMotorPwm(0, DISCOVERY_PWM);
            ctx.components.motor.setMotorPwm(1, 0);
            resetSampling();
            state = DISCOVERY_STABILIZE;
            LOG_LN("Discovery: probando motor 0");
            break;
        }

        case DISCOVERY_STABILIZE:
        {
            if (millis() - stateStartTime >= (unsigned long)MAX_STABILIZE_TIME_MS)
            {
                state = DISCOVERY_SAMPLE;
                lastSampleTime = millis();
                break;
            }
            if (millis() - lastStabCheckTime >= (unsigned long)STAB_CHECK_INTERVAL_MS)
            {
                FlowData fd;
                if (readFreshFlow(ctx, fd))
                {
                    if (lastFlowForStab >= 0.0f)
                    {
                        float diff = fd.flow - lastFlowForStab;
                        if (diff < 0) diff = -diff;
                        stableCount = (diff <= STAB_TOLERANCE) ? stableCount + 1 : 0;
                    }
                    lastFlowForStab = fd.flow;
                    if (stableCount >= REQUIRED_STABLE_READINGS)
                    {
                        state = DISCOVERY_SAMPLE;
                        lastSampleTime = millis();
                    }
                }
                lastStabCheckTime = millis();
            }
            break;
        }

        case DISCOVERY_SAMPLE:
        {
            if (millis() - lastSampleTime >= (unsigned long)SAMPLE_INTERVAL_MS)
            {
                FlowData fd;
                if (readFreshFlow(ctx, fd))
                {
                    sumFlow += fd.flow;
                    sumSqFlow += fd.flow * fd.flow;
                    sampleCount++;
                }
                sampleAttempts++;
                lastSampleTime = millis();

                if (sampleCount >= SAMPLES_PER_POINT || sampleAttempts >= MAX_SAMPLE_ATTEMPTS)
                {
                    if (sampleCount == 0)
                    {
                        failCalibration(ctx, "Sin dato de flujo");
                        break;
                    }

                    float avgFlow = (sampleCount > 0) ? (sumFlow / sampleCount) : 0.0f;
                    discoveryFlow[discoveryMotor] = avgFlow;
                    LOG_F("Discovery motor %d: %.3f L/min\n", discoveryMotor, avgFlow);

                    if (discoveryMotor == 0)
                    {
                        // Probar el otro motor
                        discoveryMotor = 1;
                        ctx.components.motor.setMotorPwm(0, 0);
                        ctx.components.motor.setMotorPwm(1, DISCOVERY_PWM);
                        resetSampling();
                        state = DISCOVERY_STABILIZE;
                        LOG_LN("Discovery: probando motor 1");
                    }
                    else
                    {
                        // Discovery completo, iniciar rampa
                        ctx.calibration.weakMotor = weakMotor();
                        LOG_F("Motor débil: %d (%.3f L/min), fuerte: %d (%.3f L/min)\n",
                              weakMotor(), discoveryFlow[weakMotor()],
                              strongMotor(), discoveryFlow[strongMotor()]);

                        rampingStrong = false;
                        currentPwm = 0;
                        lastAcceptedRampFlow = -1.0f;
                        startRampPoint(ctx);
                    }
                }
            }
            break;
        }

        case RAMP_KICK:
        {
            if (millis() - stateStartTime >= (unsigned long)KICK_TIME_MS)
            {
                applyMotorsForCurrentPoint(ctx);
                resetSampling();
                state = RAMP_STABILIZE;
            }
            break;
        }

        case RAMP_STABILIZE:
        {
            if (millis() - stateStartTime >= (unsigned long)MAX_STABILIZE_TIME_MS)
            {
                unstablePoints++;
                state = RAMP_SAMPLE;
                lastSampleTime = millis();
                break;
            }
            if (millis() - lastStabCheckTime >= (unsigned long)STAB_CHECK_INTERVAL_MS)
            {
                FlowData fd;
                if (readFreshFlow(ctx, fd))
                {
                    if (lastFlowForStab >= 0.0f)
                    {
                        float diff = fd.flow - lastFlowForStab;
                        if (diff < 0) diff = -diff;
                        stableCount = (diff <= STAB_TOLERANCE) ? stableCount + 1 : 0;
                    }
                    lastFlowForStab = fd.flow;
                    if (stableCount >= REQUIRED_STABLE_READINGS)
                    {
                        state = RAMP_SAMPLE;
                        lastSampleTime = millis();
                    }
                }
                lastStabCheckTime = millis();
            }
            break;
        }

        case RAMP_SAMPLE:
        {
            if (millis() - lastSampleTime >= (unsigned long)SAMPLE_INTERVAL_MS)
            {
                FlowData fd;
                if (readFreshFlow(ctx, fd))
                {
                    sumFlow += fd.flow;
                    sumSqFlow += fd.flow * fd.flow;
                    sampleCount++;
                }
                sampleAttempts++;
                lastSampleTime = millis();

                if (sampleCount >= SAMPLES_PER_POINT || sampleAttempts >= MAX_SAMPLE_ATTEMPTS)
                {
                    if (sampleCount == 0)
                    {
                        failCalibration(ctx, "Sin dato de flujo");
                        break;
                    }

                    float flowDelta = -1.0f;
                    if (sampleCount > 0)
                    {
                        float measuredFlow = sumFlow / sampleCount;
                        float variance = (sumSqFlow / sampleCount) - (measuredFlow * measuredFlow);
                        if (variance < 0.0f)
                            variance = 0.0f;
                        float stddev = sqrtf(variance);
                        if (lastAcceptedRampFlow >= 0.0f)
                            flowDelta = measuredFlow - lastAcceptedRampFlow;
                        recordPoint(ctx, measuredFlow, stddev);
                    }

                    // Avanzar PWM
                    currentPwm += getPwmStep(currentPwm, flowDelta);

                    if (flowDelta >= 0.0f && flowDelta < MIN_FLOW_DELTA * 1.5f) ++plateauCount;
                    else plateauCount = 0;
                    if ((lastAcceptedRampFlow >= 8.5f && lastAcceptedRampFlow > 0.0f) || plateauCount >= 3)
                    {
                        state = SAVING;
                        break;
                    }

                    if (currentPwm > MAX_PWM)
                    {
                        if (!rampingStrong)
                        {
                            // Pasar a rampa del motor fuerte
                            rampingStrong = true;
                            currentPwm = 0;
                            LOG_LN("Rampa motor fuerte iniciada");
                        }
                        else
                        {
                            // Calibración completa
                            state = SAVING;
                            break;
                        }
                    }

                    startRampPoint(ctx);
                }
            }
            break;
        }

        case SAVING:
        {
            ctx.calibration.sortByFlow();
            compactCalibration(ctx);
            ctx.calibration.isLoaded = ctx.calibration.validate() && ctx.calibration.save();

            LOG_F("Calibración: %d puntos, %d inestables, %d rechazados\n",
                  ctx.calibration.numPoints, unstablePoints, rejectedPoints);

            if (!ctx.calibration.isLoaded)
            {
                LOG_LN("Calibración NO válida (curva no cumple mínimos).");
                failCalibration(ctx, "Curva no guardada");
                break;
            }

            scanStarted = false;
            state = COMPLETE;
            stateStartTime = millis();
            break;
        }

        case COMPLETE:
        {
            if (ctx.components.input.isButtonPressed())
                SceneManager::setScene("inicio", ctx);
            break;
        }

        case FAILED:
        {
            if (ctx.components.input.isButtonPressed())
                SceneManager::setScene("inicio", ctx);
            break;
        }
        }
    }
};
#endif
