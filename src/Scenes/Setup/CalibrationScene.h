#ifndef CALIBRATION_SCENE_H
#define CALIBRATION_SCENE_H

#include "../../Config.h"
#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/SceneManager.h"

class CalibrationScene : public IScene
{
private:
    enum CalibrationPhase
    {
        PHASE_SMALL = 0,
        PHASE_BIG,
        PHASE_BOTH,
        PHASE_DONE
    };

    enum CalibrationState
    {
        INIT,
        STABILIZING,
        SAMPLING,
        SORTING,
        SAVING,
        COMPLETE
    };

    CalibrationPhase phase = PHASE_SMALL;
    CalibrationState state = INIT;
    float currentPct = 0.0f;
    int sampleCount = 0;
    int sampleAttempts = 0;
    float sumFlow = 0.0f;
    unsigned long lastSampleTime = 0;
    unsigned long stateStartTime = 0;
    
    float lastFlowForStab = -1.0f;
    int stableCount = 0;
    int unstablePoints = 0;
    int phaseUnstablePoints[3] = {0, 0, 0};
    int phasePointCounts[3] = {0, 0, 0};
    unsigned long lastStabCheckTime = 0;
    
    static const int SAMPLES_PER_POINT = 5;
    static const int MAX_SAMPLE_ATTEMPTS = 35;
    static const int SAMPLE_INTERVAL_MS = 350;
    static const int STAB_CHECK_INTERVAL_MS = 25;
    static const int MAX_STABILIZE_TIME_MS = 2500;
    static const int REQUIRED_STABLE_READINGS = 10;
    constexpr static const float STAB_TOLERANCE = 0.025f;

    const char *phaseLabel() const
    {
        switch (phase)
        {
        case PHASE_SMALL:
            return "Fase 1/3: chico";
        case PHASE_BIG:
            return "Fase 2/3: grande";
        case PHASE_BOTH:
            return "Fase 3/3: ambos";
        default:
            return "Finalizando";
        }
    }

    MotorManager::DistributionMode phaseMode() const
    {
        switch (phase)
        {
        case PHASE_SMALL:
            return MotorManager::SMALL_ONLY;
        case PHASE_BIG:
            return MotorManager::BIG_ONLY;
        case PHASE_BOTH:
            return MotorManager::BOTH_EXTREME;
        default:
            return MotorManager::LEGACY_GLOBAL;
        }
    }

    void startPhase(Context &ctx, CalibrationPhase newPhase)
    {
        phase = newPhase;
        state = INIT;
        currentPct = 0.0f;
        sampleCount = 0;
        sampleAttempts = 0;
        sumFlow = 0.0f;
        lastSampleTime = 0;
        stateStartTime = millis();
        lastFlowForStab = -1.0f;
        stableCount = 0;
        lastStabCheckTime = 0;
        ctx.calibration.numPoints = 0;

        if (newPhase == PHASE_DONE)
        {
            ctx.components.motor.clearCalibrationOverrideMode();
            ctx.components.motor.setDistributionMode(MotorManager::AUTO_BY_FLOW);
            return;
        }

        ctx.components.motor.setCalibrationOverrideMode(phaseMode());
        LOG_F("Iniciando %s\n", phaseLabel());
    }

    void finishCurrentPhase(Context &ctx)
    {
        if (phase >= PHASE_SMALL && phase <= PHASE_BOTH)
        {
            phaseUnstablePoints[phase] = unstablePoints;
            phasePointCounts[phase] = ctx.calibration.numPoints;
        }

        if (ctx.calibration.numPoints > 1)
        {
            CalibrationManager::sortArrays(ctx.calibration.motorPcts, ctx.calibration.flows, ctx.calibration.numPoints);
            ctx.calibration.commitPhase(phaseMode(), ctx.calibration.motorPcts, ctx.calibration.flows, ctx.calibration.numPoints);
            LOG_F("%s completada con %d puntos\n", phaseLabel(), ctx.calibration.numPoints);
        }
        else
        {
            LOG_F("%s con pocos puntos válidos (%d)\n", phaseLabel(), ctx.calibration.numPoints);
        }

        if (phase == PHASE_SMALL)
        {
            startPhase(ctx, PHASE_BIG);
        }
        else if (phase == PHASE_BIG)
        {
            startPhase(ctx, PHASE_BOTH);
        }
        else
        {
            startPhase(ctx, PHASE_DONE);
            state = SAVING;
        }
    }

    float getPctStep(float pct) const
    {
        if (pct < 25.0f)
            return 0.5f;
        if (pct < 60.0f)
            return 1.5f;
        return 3.0f;
    }

    int estimateRemainingPoints(const Context &ctx) const
    {
        int slots = CalibrationManager::MAX_POINTS - ctx.calibration.numPoints;
        if (slots <= 0 || currentPct > 100.0f)
            return 0;

        float pct = currentPct;
        int points = 0;
        while (pct <= 100.0f && points < slots)
        {
            points++;
            pct += getPctStep(pct);
        }
        return points;
    }

public:
    void enter(Context &ctx) override
    {
        unstablePoints = 0;
        phaseUnstablePoints[0] = phaseUnstablePoints[1] = phaseUnstablePoints[2] = 0;
        phasePointCounts[0] = phasePointCounts[1] = phasePointCounts[2] = 0;

        LOG_LN("Iniciando calibración completa 3 fases");
        LOG_LN("ADVERTENCIA: Este proceso tardará varios minutos. No interrumpir.");

        ctx.calibration.isLoaded = false;
        ctx.components.motor.setPowerPct(0);
        for (int i = 0; i < 20; i++)
        {
            delay(50);
        }

        startPhase(ctx, PHASE_SMALL);
    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);
        
        ctx.u8g2.setFont(FONT_BOLD_S);
        const char* title = "Calibrando bombas";
        int dotCount = (millis()/500)%3+1;
        char titleWithDots[32];
        strcpy(titleWithDots, title);
        for (int i = 0; i < dotCount; i++) {
            strcat(titleWithDots, ".");
        }
        int titleWidth = ctx.u8g2.getStrWidth(titleWithDots);
        int titleX = (128 - titleWidth) / 2;
        ctx.u8g2.drawStr(titleX, 20, titleWithDots);

        ctx.u8g2.setFont(FONT_REGULAR_S);
        int phaseWidth = ctx.u8g2.getStrWidth(phaseLabel());
        int phaseX = (128 - phaseWidth) / 2;
        ctx.u8g2.drawStr(phaseX, 30, phaseLabel());
        
        int progress = (int)((currentPct / 100.0f) * 100.0f);
        int barWidth = 100;
        int barHeight = 12;
        int barX = (128 - barWidth) / 2;
        int barY = 34;
        progress = constrain(progress, 0, 100);
        ctx.u8g2.drawRFrame(barX, barY, barWidth, barHeight, 3);
        
        int radius = 3;
        int minBarHeight = radius * 2;
        int safeBarHeight = (barHeight < minBarHeight) ? minBarHeight : barHeight;

        int fillWidth = (int)((progress / 100.0f) * (barWidth - 2));
        if (fillWidth > 0)
        {
            ctx.u8g2.drawBox(barX + 1, barY + 1, fillWidth, safeBarHeight - 2);
        }
        
        ctx.u8g2.setFont(FONT_REGULAR_S);
        char progressText[16];
        sprintf(progressText, "%d%%", progress);
        int progressWidth = ctx.u8g2.getStrWidth(progressText);

        int remainingPoints = estimateRemainingPoints(ctx);

        unsigned long perPointMs = (unsigned long)MAX_STABILIZE_TIME_MS + (unsigned long)SAMPLES_PER_POINT * (unsigned long)SAMPLE_INTERVAL_MS;
        unsigned long remainingMs = (unsigned long)remainingPoints * perPointMs;

        unsigned long totalSec = (remainingMs + 999) / 1000;
        unsigned int mins = totalSec / 60;
        unsigned int secs = totalSec % 60;
        char etaText[24];
        if (remainingPoints == 0) {
            sprintf(etaText, "Restante: 0s");
        } else {
            sprintf(etaText, "Restante: %u:%02u", mins, secs);
        }
        int etaWidth = ctx.u8g2.getStrWidth(etaText);

        int gap = 8;
        int centerX = 128 / 2-24;
        int leftX = centerX - (gap / 2) - progressWidth;
        int rightX = centerX + (gap / 2);
        
        if (leftX < 0) leftX = 0;
        if (rightX + etaWidth > 128) rightX = 128 - etaWidth;

        ctx.u8g2.drawStr(leftX, barY + barHeight + 12, progressText);
        ctx.u8g2.drawStr(rightX, barY + barHeight + 12, etaText);
        
        ctx.u8g2.sendBuffer();
        
        switch(state)
        {
            case INIT:
                if (phase == PHASE_DONE)
                {
                    state = SAVING;
                }
                else if (currentPct <= 100.0f && ctx.calibration.numPoints < CalibrationManager::MAX_POINTS)
                {
                    ctx.components.motor.setPowerPct(currentPct);
                    state = STABILIZING;
                    stateStartTime = millis();
                    lastStabCheckTime = millis();
                    stableCount = 0;
                    lastFlowForStab = -1.0f;
                    sampleCount = 0;
                    sampleAttempts = 0;
                    sumFlow = 0.0f;
                }
                else
                {
                    finishCurrentPhase(ctx);
                }
                break;
                
            case STABILIZING:
                if (millis() - stateStartTime >= MAX_STABILIZE_TIME_MS)
                {
                    unstablePoints++;
                    LOG_F("Advertencia: punto %.1f%% no logró estabilidad (timeout).\n", currentPct);
                    state = SAMPLING;
                    lastSampleTime = millis();
                }
                else if (millis() - lastStabCheckTime >= STAB_CHECK_INTERVAL_MS)
                {
                    FlowData flowData;
                    if (ctx.components.flowSensor.getData(flowData) && flowData.valid)
                    {
                        if (lastFlowForStab >= 0.0f)
                        {
                            float diff = flowData.flow - lastFlowForStab;
                            if (diff < 0) diff = -diff;
                            
                            if (diff <= STAB_TOLERANCE)
                            {
                                stableCount++;
                            }
                            else
                            {
                                stableCount = 0;
                            }
                        }
                        lastFlowForStab = flowData.flow;
                        
                        if (stableCount >= REQUIRED_STABLE_READINGS)
                        {
                            state = SAMPLING;
                            lastSampleTime = millis();
                        }
                    }
                    lastStabCheckTime = millis();
                }
                break;
                
            case SAMPLING:
                if (millis() - lastSampleTime >= SAMPLE_INTERVAL_MS)
                {
                    FlowData flowData;
                    if (!ctx.components.flowSensor.getData(flowData) || !flowData.valid)
                    {
                        LOG_LN("Error al leer sensor de flujo");
                    }
                    else
                    {
                        sumFlow += flowData.flow;
                        sampleCount++;
                    }
                    sampleAttempts++;
                    lastSampleTime = millis();
                    
                    if (sampleCount >= SAMPLES_PER_POINT || sampleAttempts >= MAX_SAMPLE_ATTEMPTS)
                    {
                        if (sampleCount <= 0)
                        {
                            LOG_F("Punto %.1f%% descartado: sin muestras válidas.\n", currentPct);
                            state = INIT;
                            break;
                        }

                        float measuredFlow = sumFlow / sampleCount;

                        if (sampleAttempts >= MAX_SAMPLE_ATTEMPTS && sampleCount < SAMPLES_PER_POINT)
                        {
                            LOG_F("Punto %.1f%% con muestreo parcial (%d/%d válidas).\n", currentPct, sampleCount, SAMPLES_PER_POINT);
                            unstablePoints++;
                        }
                        
                        LOG_F("Punto %.1f%%: %.3f L/min \n", currentPct, measuredFlow);
                        
                        ctx.calibration.motorPcts[ctx.calibration.numPoints] = currentPct;
                        ctx.calibration.flows[ctx.calibration.numPoints] = measuredFlow;
                        ctx.calibration.numPoints++;
                        
                        currentPct += getPctStep(currentPct);
                        state = INIT;
                    }
                }
                break;
                
            case SORTING:
                finishCurrentPhase(ctx);
                break;
                
            case SAVING:
                ctx.components.motor.setPowerPct(0);
                ctx.components.motor.clearCalibrationOverrideMode();
                ctx.components.motor.setDistributionMode(MotorManager::AUTO_BY_FLOW);
                ctx.calibration.save();
                ctx.calibration.isLoaded = ctx.calibration.hasCompletePhaseCalibration();
                LOG_F("Resumen fases -> small: %d pts (%d inestables), big: %d pts (%d inestables), both: %d pts (%d inestables)\n",
                      phasePointCounts[0], phaseUnstablePoints[0],
                      phasePointCounts[1], phaseUnstablePoints[1],
                      phasePointCounts[2], phaseUnstablePoints[2]);
                if (!ctx.calibration.isLoaded)
                {
                    LOG_LN("Calibración completa NO válida: faltan fases o curvas sin calidad mínima.");
                }
                if (unstablePoints > 0)
                {
                    LOG_F("Calibración completada con %d puntos de calidad degradada.\n", unstablePoints);
                }
                state = COMPLETE;
                stateStartTime = millis();
                break;
                
            case COMPLETE:
                if (millis() - stateStartTime >= 2000)
                {
                    ESP.restart();
                }
                break;
        }
    }
};
#endif