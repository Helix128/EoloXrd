#ifndef CAPTURA_SCENE_H
#define CAPTURA_SCENE_H

#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/Renderer.h"
#include "../../Config.h"

// Escena de logo/splash al iniciar la app
class CapturaScene : public IScene
{
private:
public:
    static constexpr const char *Name = "captura";

    uint16_t frameIntervalMs() const override { return 250; }

    void enter(Context &ctx) override
    {
        if (ctx.isPaused)
        {
            LOG_LN("Resumiendo captura.");
            ctx.resumeCapture();
            LOG_LN("Captura reanudada.");
        }
        else if (!ctx.isCapturing)
        {
            LOG_LN("Iniciando captura.");
            ctx.beginCapture();
            LOG_LN("Captura iniciada.");
        }
    }

    void update(Context &ctx) override
    {
        Renderer::beginFrame(ctx.u8g2);
        GUI::displayHeader(ctx);

        int delta = ctx.components.input.getEncoderDelta();
        bool button = ctx.components.input.isButtonPressed();

        if (delta != 0)
        {
            cycleFooter = (cycleFooter + delta);
            if (cycleFooter < 0)
                cycleFooter = 5;
            else if (cycleFooter > 5)
                cycleFooter = 0;
        }

        if (button)
        {
            ctx.saveSession();
            SceneManager::setScene("captura_menu", ctx);
        }

        footer(ctx);
        mainPanel(ctx);

        Renderer::endFrame(ctx.u8g2);
    }

    void mainPanel(Context &ctx)
    {   
        const UiSnapshot &snapshot = ctx.getUiSnapshot();
        float lmin = snapshot.flow.valid ? snapshot.flow.flow : -1.0f;
        float volume = snapshot.flow.capturedVolume; 

        char lminStr[10];
        char volStr[10];
        snprintf(lminStr, sizeof(lminStr), "%.2f", lmin);
        snprintf(volStr, sizeof(volStr), "%.3f", volume*0.001);

        Renderer::metricRow(ctx.u8g2, "Flujo", lminStr, "L/min", "Vol. captura", volStr, "m3", nullptr, nullptr, nullptr, 24, 38);
    }

    int cycleFooter = 0;
    void footer(Context &ctx)
    {
        Renderer::separatorLine(ctx.u8g2, 40);
        const UiSnapshot &snapshot = ctx.getUiSnapshot();

        switch (cycleFooter)
        {
        case 0: // temp hum presion
        {
            float temp = snapshot.environment.temperature;
            float hum = snapshot.environment.humidity;
            float presAtm = snapshot.environment.pressure;

#if BAREBONES == true // valores inventados para testing
            temp = 25.0;
            hum = 50.0;
            presAtm = 1013.0;
#endif

            char tempStr[6];
            char humStr[5];
            char presStr[5];
            snprintf(tempStr, sizeof(tempStr), "%.2f", temp);
            snprintf(humStr, sizeof(humStr), "%.1f", hum);
            snprintf(presStr, sizeof(presStr), "%.0f", presAtm);

            Renderer::metricRow(ctx.u8g2, "Temp", tempStr, "C", "Hum", humStr, "%", "Presion", presStr, "hPa");
            break;
        }
        case 1: // pm1 pm2.5 pm10
        {
            if (!snapshot.airQuality.enabled)
            {
                cycleFooter += ctx.components.input.getEncoderDelta();
                break;
            }

            float pm1 = snapshot.airQuality.valid ? snapshot.airQuality.pm1 : 0.0f;
            float pm25 = snapshot.airQuality.valid ? snapshot.airQuality.pm25 : 0.0f;
            float pm10 = snapshot.airQuality.valid ? snapshot.airQuality.pm10 : 0.0f;

#if BAREBONES == true
            pm1 = 512;
            pm25 = 512;
            pm10 = 512;
#endif

            char pm1Str[5];
            char pm25Str[5];
            char pm10Str[5];

            snprintf(pm1Str, sizeof(pm1Str), "%.0f", pm1);
            snprintf(pm25Str, sizeof(pm25Str), "%.0f", pm25);
            snprintf(pm10Str, sizeof(pm10Str), "%.0f", pm10);
            Renderer::metricRow(ctx.u8g2, "PM1.0", pm1Str, "ugm2", "PM2.5", pm25Str, "ugm2", "PM10", pm10Str, "ugm2");
            break;
        }
        case 2:
        { // flujo configurado

            float targetFlow = snapshot.flow.targetFlow;
            char targetFlowStr[10];
            snprintf(targetFlowStr, sizeof(targetFlowStr), "%.1f", targetFlow);
            ctx.u8g2.setFont(FONT_BOLD_S);
            int labelWidth = ctx.u8g2.getStrWidth("Flujo configurado");
            int labelX = 64 - labelWidth / 2;
            ctx.u8g2.drawStr(labelX, 50, "Flujo configurado");
            ctx.u8g2.setFont(FONT_REGULAR_S);
            int valueWidth = ctx.u8g2.getStrWidth(targetFlowStr);
            int unitWidth = ctx.u8g2.getStrWidth("L/min");
            int totalWidth = valueWidth + unitWidth + 1;
            int valueX = 64 - totalWidth / 2;
            ctx.u8g2.drawStr(valueX, 60, targetFlowStr);
            ctx.u8g2.drawStr(valueX + valueWidth + 1, 60, "L/min");
            break;
        }
        case 3:
        { // tiempo transcurrido
            unsigned long elapsed = ctx.session.elapsedTime;
            unsigned long hours = elapsed / 3600;
            unsigned long minutes = (elapsed % 3600) / 60;
            unsigned long seconds = elapsed % 60;

            char timeStr[9];
            snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu:%02lu", hours, minutes, seconds);
            ctx.u8g2.setFont(FONT_BOLD_S);
            int labelWidth = ctx.u8g2.getStrWidth("Tiempo transcurrido");
            int centerX = 64;
            int labelX = centerX - labelWidth / 2;
            ctx.u8g2.drawStr(labelX, 50, "Tiempo transcurrido");
            ctx.u8g2.setFont(FONT_REGULAR_S);
            int valueWidth = ctx.u8g2.getStrWidth(timeStr);
            int valueX = centerX - valueWidth / 2;
            ctx.u8g2.drawStr(valueX, 60, timeStr);
            break;
        }
        case 4:
        {
            // tiempo restante
            unsigned long duration = ctx.session.duration;
            unsigned long remaining = duration - (ctx.session.elapsedTime);
            unsigned long hours = remaining / 3600;
            unsigned long minutes = (remaining % 3600) / 60;
            unsigned long seconds = remaining % 60;
            char remainingStr[9];
            snprintf(remainingStr, sizeof(remainingStr), "%02lu:%02lu:%02lu", hours, minutes, seconds);
            ctx.u8g2.setFont(FONT_BOLD_S);
            int labelWidth = ctx.u8g2.getStrWidth("Tiempo restante");
            int centerX = 64;
            int labelX = centerX - labelWidth / 2;
            ctx.u8g2.drawStr(labelX, 50, "Tiempo restante");
            ctx.u8g2.setFont(FONT_REGULAR_S);
            int valueWidth = ctx.u8g2.getStrWidth(remainingStr);
            int valueX = centerX - valueWidth / 2;
            ctx.u8g2.drawStr(valueX, 60, remainingStr);
            break;
        }
        case 5:
        {
            // tiempos de inicio y fin
            DateTime start = ctx.session.startDate;
            DateTime end = start + ctx.session.duration;

            String startStr = start.timestamp(DateTime::TIMESTAMP_TIME);
            String endStr = end.timestamp(DateTime::TIMESTAMP_TIME);
            ctx.u8g2.setFont(FONT_BOLD_S);

            int startWidth = ctx.u8g2.getStrWidth(startStr.c_str());
            int endWidth = ctx.u8g2.getStrWidth(endStr.c_str());

            int labelStartWidth = ctx.u8g2.getStrWidth("Inicio");
            int labelEndWidth = ctx.u8g2.getStrWidth("Fin");

            int leftCenter = 32;
            int startX = leftCenter - startWidth / 2;
            int labelStartX = leftCenter - labelStartWidth / 2;

            int rightCenter = 96;
            int endX = rightCenter - endWidth / 2;
            int labelEndX = rightCenter - labelEndWidth / 2;

            ctx.u8g2.drawStr(labelStartX, 50, "Inicio");
            ctx.u8g2.drawStr(labelEndX, 50, "Fin");

            ctx.u8g2.setFont(FONT_REGULAR_S);
            ctx.u8g2.drawStr(startX, 60, startStr.c_str());
            ctx.u8g2.drawStr(endX, 60, endStr.c_str());
            break;
        }
        }
    }
};
#endif
