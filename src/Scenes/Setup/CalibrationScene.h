#ifndef CALIBRATION_SCENE_H
#define CALIBRATION_SCENE_H

#include "../../Config.h"
#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/SceneManager.h"

// Escena de calibración del motor
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
    
    static const int SAMPLES_PER_POINT = 12;
    static const int STABILIZE_TIME_MS = 1000;
    static const int SAMPLE_INTERVAL_MS = 50;

public:
    void enter(Context &ctx) override
    {
        state = INIT;
        currentPct = 0.0f;
        sampleCount = 0;
        sumFlow = 0.0f;
        lastSampleTime = 0;
        stateStartTime = millis();
        
        Serial.println("=== Iniciando calibración Motor-Flujo ===");
        Serial.println("ADVERTENCIA: Este proceso tardará varios minutos. No interrumpir.");
        
        // Inicializar arrays de calibración
        ctx.numCalPoints = 0;
        
        // Apagar motor y estabilizar sensor
        ctx.components.motor.setPowerPct(0);
        for (int i = 0; i < 20; i++) {
            ctx.components.flowSensor.readData();
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
        
        // Barra de progreso
        int progress = (int)((currentPct / 100.0f) * 100.0f);
        int barWidth = 100;
        int barHeight = 12;
        int barX = (128 - barWidth) / 2;
        int barY = 32;
        progress = constrain(progress, 0, 100);
        ctx.u8g2.drawRFrame(barX, barY, barWidth, barHeight, 3);
        // Ensure barHeight is at least twice the radius to avoid overflow
        int radius = 3;
        int minBarHeight = radius * 2;
        int safeBarHeight = (barHeight < minBarHeight) ? minBarHeight : barHeight;

        int fillWidth = (int)((progress / 100.0f) * (barWidth - 2));
        if (fillWidth > 0)
        {
            ctx.u8g2.drawBox(barX + 1, barY + 1, fillWidth, safeBarHeight - 2);
        }
        
        // Mostrar porcentaje
        ctx.u8g2.setFont(FONT_REGULAR_S);
        char progressText[16];
        sprintf(progressText, "%d%%", progress);
        int progressWidth = ctx.u8g2.getStrWidth(progressText);

        // Calcular puntos restantes (porcentaje y espacio disponible en array)
        int remainingPct = 100 - (int)currentPct;
        if (remainingPct < 0) remainingPct = 0;
        int remainingSlots = ctx.MAX_CAL_POINTS - ctx.numCalPoints;
        if (remainingSlots < 0) remainingSlots = 0;
        int remainingPoints = (remainingPct < remainingSlots) ? remainingPct : remainingSlots;

        // Estimar tiempo restante por punto y total (suma de delays por iteración)
        unsigned long perPointMs = (unsigned long)STABILIZE_TIME_MS + (unsigned long)SAMPLES_PER_POINT * (unsigned long)SAMPLE_INTERVAL_MS;
        unsigned long remainingMs = (unsigned long)remainingPoints * perPointMs;

        // Formatear ETA a mm:ss (o "0s" si no hay puntos restantes)
        unsigned long totalSec = (remainingMs + 999) / 1000; // redondeo hacia arriba
        unsigned int mins = totalSec / 60;
        unsigned int secs = totalSec % 60;
        char etaText[24];
        if (remainingPoints == 0) {
            sprintf(etaText, "Restante: 0s");
        } else {
            sprintf(etaText, "Restante: %u:%02u", mins, secs);
        }
        int etaWidth = ctx.u8g2.getStrWidth(etaText);

        // Dibujar ambos textos en un layout horizontal alrededor del centro
        int gap = 8; // espacio entre textos alrededor del centro
        int centerX = 128 / 2-24;
        int leftX = centerX - (gap / 2) - progressWidth; // porcentaje a la izquierda
        int rightX = centerX + (gap / 2);                // ETA a la derecha
        // Asegurar límites simples
        if (leftX < 0) leftX = 0;
        if (rightX + etaWidth > 128) rightX = 128 - etaWidth;

        ctx.u8g2.drawStr(leftX, barY + barHeight + 12, progressText);
        ctx.u8g2.drawStr(rightX, barY + barHeight + 12, etaText);
        
        // Mostrar estado
        const char* stateText = "";
        switch(state)
        {
            case INIT: stateText = "Inicializando..."; break;
            case STABILIZING: stateText = "Estabilizando..."; break;
            case SAMPLING: stateText = "Muestreando..."; break;
            case SORTING: stateText = "Ordenando datos..."; break;
            case SAVING: stateText = "Guardando..."; break;
            case COMPLETE: stateText = "Completo!"; break;
        }
        int stateWidth = ctx.u8g2.getStrWidth(stateText);
        //ctx.u8g2.drawStr((128 - stateWidth) / 2, 60, stateText);
        
        ctx.u8g2.sendBuffer();
        
        // Lógica de calibración
        switch(state)
        {
            case INIT:
                if (currentPct <= 100.0f && ctx.numCalPoints < ctx.MAX_CAL_POINTS)
                {
                    ctx.components.motor.setPowerPct(currentPct);
                    state = STABILIZING;
                    stateStartTime = millis();
                    sampleCount = 0;
                    sumFlow = 0.0f;
                }
                else
                {
                    state = SORTING;
                }
                break;
                
            case STABILIZING:
                if (millis() - stateStartTime >= STABILIZE_TIME_MS)
                {
                    state = SAMPLING;
                    lastSampleTime = millis();
                }
                break;
                
            case SAMPLING:
                if (millis() - lastSampleTime >= SAMPLE_INTERVAL_MS)
                {
                    ctx.components.flowSensor.readData();
                    sumFlow += ctx.components.flowSensor.flow;
                    sampleCount++;
                    lastSampleTime = millis();
                    
                    if (sampleCount >= SAMPLES_PER_POINT)
                    {
                        float measuredFlow = sumFlow / SAMPLES_PER_POINT;
                        
                        Serial.print("Motor ");
                        Serial.print(currentPct, 1);
                        Serial.print("% -> Flujo medido (prom): ");
                        Serial.print(measuredFlow, 3);
                        Serial.println(" L/min");
                        
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
                    Serial.println("ERROR: Pocos puntos de calibración capturados.");
                    ctx.isCalibrationLoaded = false;
                    state = COMPLETE;
                    break;
                }
                
                // Ordenar puntos por flujo ascendente
                Serial.println("Ordenando puntos de calibración por flujo...");
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
                
                Serial.println("Calibración completa y ordenada.");
                Serial.print("Puntos capturados: ");
                Serial.println(ctx.numCalPoints);
                
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