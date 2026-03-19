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
    enum CalibrationState
    {
        INIT,
        STABILIZING,
        SAMPLING,
        SORTING,
        SAVING,
        COMPLETE
    };

    CalibrationState state = INIT;
    float currentPct = 0.0f;
    int sampleCount = 0;
    float sumFlow = 0.0f;
    unsigned long lastSampleTime = 0;
    unsigned long stateStartTime = 0;
    
    float lastFlowForStab = -1.0f;
    int stableCount = 0;
    unsigned long lastStabCheckTime = 0;
    
    static const int SAMPLES_PER_POINT = 25;
    static const int SAMPLE_INTERVAL_MS = 25;
    static const int STAB_CHECK_INTERVAL_MS = 25;
    static const int MAX_STABILIZE_TIME_MS = 2500;
    static const int REQUIRED_STABLE_READINGS = 10;
    constexpr static const float STAB_TOLERANCE = 0.025f;

public:
    void enter(Context &ctx) override
    {
        state = INIT;
        currentPct = 0.0f;
        sampleCount = 0;
        sumFlow = 0.0f;
        lastSampleTime = 0;
        stateStartTime = millis();
        
        LOG_LN("Iniciando calibración Motor-Flujo");
        LOG_LN("ADVERTENCIA: Este proceso tardará varios minutos. No interrumpir.");
        
        ctx.numCalPoints = 0;
        
        ctx.components.motor.setPowerPct(0);
        for (int i = 0; i < 20; i++) {
            delay(50);
        }
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
        ctx.u8g2.drawStr(titleX, 28, titleWithDots);
        
        int progress = (int)((currentPct / 100.0f) * 100.0f);
        int barWidth = 100;
        int barHeight = 12;
        int barX = (128 - barWidth) / 2;
        int barY = 32;
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

        int remainingPct = 100 - (int)currentPct;
        if (remainingPct < 0) remainingPct = 0;
        int remainingSlots = ctx.MAX_CAL_POINTS - ctx.numCalPoints;
        if (remainingSlots < 0) remainingSlots = 0;
        int remainingPoints = (remainingPct < remainingSlots) ? remainingPct : remainingSlots;

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
                if (currentPct <= 100.0f && ctx.numCalPoints < ctx.MAX_CAL_POINTS)
                {
                    ctx.components.motor.setPowerPct(currentPct);
                    state = STABILIZING;
                    stateStartTime = millis();
                    lastStabCheckTime = millis();
                    stableCount = 0;
                    lastFlowForStab = -1.0f;
                    sampleCount = 0;
                    sumFlow = 0.0f;
                }
                else
                {
                    state = SORTING;
                }
                break;
                
            case STABILIZING:
                if (millis() - stateStartTime >= MAX_STABILIZE_TIME_MS)
                {
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
                    }
                    sampleCount++;
                    lastSampleTime = millis();
                    
                    if (sampleCount >= SAMPLES_PER_POINT)
                    {
                        float measuredFlow = sumFlow / SAMPLES_PER_POINT;
                        
                        LOG_F("Punto %.1f%%: %.3f L/min \n", currentPct, measuredFlow);
                        
                        ctx.calMotorPcts[ctx.numCalPoints] = currentPct;
                        ctx.calFlows[ctx.numCalPoints] = measuredFlow;
                        ctx.numCalPoints++;
                        
                        currentPct += 1.0f;
                        state = INIT;
                    }
                }
                break;
                
            case SORTING:
                ctx.components.motor.setPowerPct(0);
                
                if (ctx.numCalPoints < 2)
                {
                    LOG_LN("ERROR: Pocos puntos.");
                    ctx.isCalibrationLoaded = false;
                    state = COMPLETE;
                    break;
                }
                
                for (int i = 0; i < ctx.numCalPoints - 1; i++)
                {
                    for (int j = 0; j < ctx.numCalPoints - i - 1; j++)
                    {
                        if (ctx.calFlows[j] > ctx.calFlows[j + 1])
                        {
                            float tf = ctx.calFlows[j];
                            ctx.calFlows[j] = ctx.calFlows[j + 1];
                            ctx.calFlows[j + 1] = tf;

                            float tm = ctx.calMotorPcts[j];
                            ctx.calMotorPcts[j] = ctx.calMotorPcts[j + 1];
                            ctx.calMotorPcts[j + 1] = tm;
                        }
                    }
                }
                
                state = SAVING;
                break;
                
            case SAVING:
                ctx.saveCalibration();
                ctx.isCalibrationLoaded = true;
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