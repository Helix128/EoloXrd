#ifndef CALIBRATION_SCENE_H
#define CALIBRATION_SCENE_H

#include "../../Config.h"
#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/Renderer.h"

class CalibrationScene : public IScene
{
private:
    enum CalibrationState
    {
        INIT,
        DISCOVERY_STABILIZE,
        DISCOVERY_SAMPLE,
        RAMP_STABILIZE,
        RAMP_SAMPLE,
        SAVING,
        COMPLETE
    };

    CalibrationState state = INIT;

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
    unsigned long lastSampleTime = 0;
    unsigned long stateStartTime = 0;

    // Stabilization state
    float lastFlowForStab = -1.0f;
    int stableCount = 0;
    int unstablePoints = 0;
    unsigned long lastStabCheckTime = 0;

    static const int SAMPLES_PER_POINT = 5;
    static const int MAX_SAMPLE_ATTEMPTS = 35;
    static const int SAMPLE_INTERVAL_MS = 350;
    static const int STAB_CHECK_INTERVAL_MS = 25;
    static const int MAX_STABILIZE_TIME_MS = 2500;
    static const int REQUIRED_STABLE_READINGS = 10;
    constexpr static const float STAB_TOLERANCE = 0.025f;
    static const int DISCOVERY_PWM = MAX_PWM / 2;

    int getPwmStep(int pwm) const
    {
        if (pwm < MAX_PWM / 4)
            return 10;
        if (pwm < MAX_PWM / 2)
            return 30;
        return 60;
    }

    void resetSampling()
    {
        sampleCount = 0;
        sampleAttempts = 0;
        sumFlow = 0.0f;
        lastSampleTime = millis();
        stableCount = 0;
        lastFlowForStab = -1.0f;
        lastStabCheckTime = millis();
        stateStartTime = millis();
    }

    int weakMotor() const { return discoveryFlow[0] <= discoveryFlow[1] ? 0 : 1; }
    int strongMotor() const { return 1 - weakMotor(); }

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

    void recordPoint(Context &ctx, float measuredFlow)
    {
        if (ctx.calibration.numPoints >= CalibrationManager::MAX_POINTS)
            return;

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

        LOG_F("Punto %d: pwm[%d,%d] -> %.3f L/min\n",
              idx, ctx.calibration.pwm0[idx], ctx.calibration.pwm1[idx], measuredFlow);
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

public:
    static constexpr const char *Name = "calibration";

    uint16_t frameIntervalMs() const override { return 250; }

    void enter(Context &ctx) override
    {
        unstablePoints = 0;
        discoveryMotor = 0;
        discoveryFlow[0] = discoveryFlow[1] = 0;
        rampingStrong = false;
        currentPwm = 0;

        LOG_LN("Iniciando calibración: discovery + rampa continua");
        ctx.calibration.isLoaded = false;
        ctx.calibration.numPoints = 0;
        ctx.components.motor.setPwm(0);
        delay(1000);

        state = INIT;
    }

    void update(Context &ctx) override
    {
        // === RENDER ===
        Renderer::beginFrame(ctx.u8g2);
        GUI::displayHeader(ctx);

        ctx.u8g2.setFont(FONT_BOLD_S);
        const char *title = "Calibrando";
        int dotCount = (millis() / 500) % 3 + 1;
        char titleBuf[32];
        strcpy(titleBuf, title);
        for (int i = 0; i < dotCount; i++)
            strcat(titleBuf, ".");
        Renderer::centeredText(ctx.u8g2, titleBuf, 64, 20, FONT_BOLD_S);

        ctx.u8g2.setFont(FONT_REGULAR_S);
        const char *phaseLabel;
        if (state <= DISCOVERY_SAMPLE)
            phaseLabel = "Descubrimiento";
        else if (!rampingStrong)
            phaseLabel = "Rampa motor debil";
        else
            phaseLabel = "Rampa motor fuerte";

        Renderer::centeredText(ctx.u8g2, phaseLabel, 64, 30, FONT_REGULAR_S);

        // Progress bar
        int progress = constrain((int)getOverallProgress(), 0, 100);
        int barW = 100, barH = 12;
        int barX = (128 - barW) / 2, barY = 34;
        Renderer::progressBar(ctx.u8g2, barX, barY, barW, barH, progress);

        // Progress text + ETA
        char progressText[8];
        sprintf(progressText, "%d%%", progress);

        int remaining = estimateRemainingPoints();
        unsigned long perPointMs = MAX_STABILIZE_TIME_MS + SAMPLES_PER_POINT * SAMPLE_INTERVAL_MS;
        unsigned long remainSec = ((unsigned long)remaining * perPointMs + 999) / 1000;
        char etaText[24];
        sprintf(etaText, "~%lu:%02lu", remainSec / 60, remainSec % 60);

        ctx.u8g2.setFont(FONT_REGULAR_S);
        int pW = ctx.u8g2.getStrWidth(progressText);
        int eW = ctx.u8g2.getStrWidth(etaText);
        ctx.u8g2.drawStr((128 / 2) - pW - 4, barY + barH + 12, progressText);
        ctx.u8g2.drawStr((128 / 2) + 4, barY + barH + 12, etaText);

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
                if (ctx.components.flowSensor.getData(fd) && fd.valid)
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
                if (ctx.components.flowSensor.getData(fd) && fd.valid)
                {
                    sumFlow += fd.flow;
                    sampleCount++;
                }
                sampleAttempts++;
                lastSampleTime = millis();

                if (sampleCount >= SAMPLES_PER_POINT || sampleAttempts >= MAX_SAMPLE_ATTEMPTS)
                {
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
                        applyMotorsForCurrentPoint(ctx);
                        resetSampling();
                        state = RAMP_STABILIZE;
                    }
                }
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
                if (ctx.components.flowSensor.getData(fd) && fd.valid)
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
                if (ctx.components.flowSensor.getData(fd) && fd.valid)
                {
                    sumFlow += fd.flow;
                    sampleCount++;
                }
                sampleAttempts++;
                lastSampleTime = millis();

                if (sampleCount >= SAMPLES_PER_POINT || sampleAttempts >= MAX_SAMPLE_ATTEMPTS)
                {
                    if (sampleCount > 0)
                    {
                        float measuredFlow = sumFlow / sampleCount;
                        recordPoint(ctx, measuredFlow);
                    }

                    // Avanzar PWM
                    currentPwm += getPwmStep(currentPwm);

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

                    applyMotorsForCurrentPoint(ctx);
                    resetSampling();
                    state = RAMP_STABILIZE;
                }
            }
            break;
        }

        case SAVING:
        {
            ctx.components.motor.setPwm(0);
            ctx.calibration.sortByFlow();
            ctx.calibration.save();
            ctx.calibration.isLoaded = ctx.calibration.validate();

            LOG_F("Calibración: %d puntos, %d inestables\n",
                  ctx.calibration.numPoints, unstablePoints);

            if (!ctx.calibration.isLoaded)
            {
                LOG_LN("Calibración NO válida (curva no cumple mínimos).");
            }

            state = COMPLETE;
            stateStartTime = millis();
            break;
        }

        case COMPLETE:
        {
            if (millis() - stateStartTime >= 2000)
                ESP.restart();
            break;
        }
        }
    }
};
#endif
