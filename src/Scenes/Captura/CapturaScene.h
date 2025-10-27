#ifndef CAPTURA_SCENE_H
#define CAPTURA_SCENE_H

#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/GUI.h"
#include "../../Config.h"

// Escena de logo/splash al iniciar la app
class CapturaScene : public IScene
{
private:
public:
    void enter(Context &ctx) override
    {
        if (ctx.isPaused)
        {
            ctx.resumeCapture();
            Serial.println("Resumiendo captura.");
        }
        else if (!ctx.isCapturing)
        {
            ctx.beginCapture();
            Serial.println("Iniciando captura.");
        }

        if(ctx.session.usePlantower&&!ctx.components.plantower.isActive)
        {
            ctx.components.plantower.setPower(true);
        }
        else if(!ctx.session.usePlantower&&ctx.components.plantower.isActive)
        {
            ctx.components.plantower.setPower(false);
        }
    }

    void update(Context &ctx) override
    {
        ctx.updateCapture();

        ctx.u8g2.clearBuffer();
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

        ctx.u8g2.sendBuffer();
    }

    void mainPanel(Context &ctx)
    {
        float lmin = ctx.components.flowSensor.flow;
        float volume = ctx.session.capturedVolume; 

        char lminStr[10];
        char volStr[10];
        snprintf(lminStr, sizeof(lminStr), "%.2f", lmin);
        snprintf(volStr, sizeof(volStr), "%.3f", volume*0.001);

        int sectionWidth = 128 / 2;
        int startX1 = 0;
        int startX2 = sectionWidth;

        // Flow
        {
            ctx.u8g2.setFont(FONT_BOLD_S);
            int labelWidth = ctx.u8g2.getStrWidth("Flujo");
            int labelX = startX1 + (sectionWidth - labelWidth) / 2;
            ctx.u8g2.drawStr(labelX, 24, "Flujo");

            ctx.u8g2.setFont(FONT_REGULAR);
            int valueWidth = ctx.u8g2.getStrWidth(lminStr);
            ctx.u8g2.setFont(u8g2_font_4x6_tf);
            int unitWidth = ctx.u8g2.getStrWidth("L/min");
            int totalWidth = valueWidth + unitWidth + 1;
            int valueX = startX1 + (sectionWidth - totalWidth) / 2;
            ctx.u8g2.setFont(FONT_REGULAR);
            ctx.u8g2.drawStr(valueX, 38, lminStr);
            ctx.u8g2.setFont(u8g2_font_4x6_tf);
            ctx.u8g2.drawStr(valueX + valueWidth + 1, 38, "L/min");
        }

        // Captured Volume
        {
            ctx.u8g2.setFont(FONT_BOLD_S);
            int labelWidth = ctx.u8g2.getStrWidth("Vol. captura");
            int labelX = startX2 + (sectionWidth - labelWidth) / 2;
            ctx.u8g2.drawStr(labelX, 24, "Vol. captura");

            ctx.u8g2.setFont(FONT_REGULAR);
            int valueWidth = ctx.u8g2.getStrWidth(volStr);
            ctx.u8g2.setFont(u8g2_font_4x6_tf);
            int unitWidth = ctx.u8g2.getStrWidth("m3");
            int totalWidth = valueWidth + unitWidth + 1;
            int valueX = startX2 + (sectionWidth - totalWidth) / 2;
            ctx.u8g2.setFont(FONT_REGULAR);
            ctx.u8g2.drawStr(valueX, 38, volStr);
            ctx.u8g2.setFont(u8g2_font_4x6_tf);
            ctx.u8g2.drawStr(valueX + valueWidth + 1, 38, "m3");
        }
    }

    int cycleFooter = 0;
    void footer(Context &ctx)
    {
        ctx.u8g2.drawHLine(0, 40, 128);

        switch (cycleFooter)
        {
        case 0: // temp hum presion
        {
            float temp = ctx.components.bme.temperature;
            float hum = ctx.components.bme.humidity;
            float presAtm = ctx.components.bme.pressure;

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

            int sectionWidth = 128 / 3;
            int startX1 = 0;
            int startX2 = sectionWidth;
            int startX3 = 2 * sectionWidth;

            // Temp
            {
                ctx.u8g2.setFont(FONT_BOLD_S);
                int labelWidth = ctx.u8g2.getStrWidth("Temp");
                int labelX = startX1 + (sectionWidth - labelWidth) / 2;
                ctx.u8g2.drawStr(labelX, 50, "Temp");

                ctx.u8g2.setFont(FONT_REGULAR_S);
                int valueWidth = ctx.u8g2.getStrWidth(tempStr);
                ctx.u8g2.setFont(u8g2_font_4x6_tf);
                int unitWidth = ctx.u8g2.getStrWidth("C");
                int totalValueWidth = valueWidth + unitWidth + 1;
                int valueX = startX1 + (sectionWidth - totalValueWidth) / 2;
                ctx.u8g2.setFont(FONT_REGULAR_S);
                ctx.u8g2.drawStr(valueX, 60, tempStr);
                ctx.u8g2.setFont(u8g2_font_4x6_tf);
                ctx.u8g2.drawStr(valueX + valueWidth + 1, 60, "C");
            }

            // Hum
            {
                ctx.u8g2.setFont(FONT_BOLD_S);
                int labelWidth = ctx.u8g2.getStrWidth("Hum");
                int labelX = startX2 + (sectionWidth - labelWidth) / 2;
                ctx.u8g2.drawStr(labelX, 50, "Hum");

                ctx.u8g2.setFont(FONT_REGULAR_S);
                int valueWidth = ctx.u8g2.getStrWidth(humStr);
                ctx.u8g2.setFont(u8g2_font_4x6_tf);
                int unitWidth = ctx.u8g2.getStrWidth("%");
                int totalValueWidth = valueWidth + unitWidth + 1;
                int valueX = startX2 + (sectionWidth - totalValueWidth) / 2;
                ctx.u8g2.setFont(FONT_REGULAR_S);
                ctx.u8g2.drawStr(valueX, 60, humStr);
                ctx.u8g2.setFont(u8g2_font_4x6_tf);
                ctx.u8g2.drawStr(valueX + valueWidth + 1, 60, "%");
            }

            // Presion
            {
                ctx.u8g2.setFont(FONT_BOLD_S);
                int labelWidth = ctx.u8g2.getStrWidth("Presion");
                int labelX = startX3 + (128 - startX3 - labelWidth) / 2;
                ctx.u8g2.drawStr(labelX, 50, "Presion");

                ctx.u8g2.setFont(FONT_REGULAR_S);
                int valueWidth = ctx.u8g2.getStrWidth(presStr);
                ctx.u8g2.setFont(u8g2_font_4x6_tf);
                int unitWidth = ctx.u8g2.getStrWidth("hPa");
                int totalValueWidth = valueWidth + unitWidth + 1;
                int valueX = startX3 + (128 - startX3 - totalValueWidth) / 2;
                ctx.u8g2.setFont(FONT_REGULAR_S);
                ctx.u8g2.drawStr(valueX, 60, presStr);
                ctx.u8g2.setFont(u8g2_font_4x6_tf);
                ctx.u8g2.drawStr(valueX + valueWidth + 1, 60, "hPa");
            }
            break;
        }
        case 1: // pm1 pm2.5 pm10
        {
            if (!ctx.session.usePlantower)
            {
                cycleFooter += ctx.components.input.getEncoderDelta();
                break;
            }
            float pm1 = ctx.components.plantower.pm1;
            float pm25 = ctx.components.plantower.pm25;
            float pm10 = ctx.components.plantower.pm10;

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
            int sectionWidth = 128 / 3;
            int startX1 = 0;
            int startX2 = sectionWidth;
            int startX3 = 2 * sectionWidth;

            // PM1.0
            {
                ctx.u8g2.setFont(FONT_BOLD_S);
                int labelWidth = ctx.u8g2.getStrWidth("PM1.0");
                int labelX = startX1 + (sectionWidth - labelWidth) / 2;
                ctx.u8g2.drawStr(labelX, 50, "PM1.0");

                ctx.u8g2.setFont(FONT_REGULAR_S);
                int valueWidth = ctx.u8g2.getStrWidth(pm1Str);
                ctx.u8g2.setFont(u8g2_font_4x6_tf);
                int unitWidth = ctx.u8g2.getStrWidth("ugm2");
                int totalValueWidth = valueWidth + unitWidth + 1;
                int valueX = startX1 + (sectionWidth - totalValueWidth) / 2;
                ctx.u8g2.setFont(FONT_REGULAR_S);
                ctx.u8g2.drawStr(valueX, 60, pm1Str);
                ctx.u8g2.setFont(u8g2_font_4x6_tf);
                ctx.u8g2.drawStr(valueX + valueWidth + 1, 60, "ugm2");
            }

            // PM2.5
            {
                ctx.u8g2.setFont(FONT_BOLD_S);
                int labelWidth = ctx.u8g2.getStrWidth("PM2.5");
                int labelX = startX2 + (sectionWidth - labelWidth) / 2;
                ctx.u8g2.drawStr(labelX, 50, "PM2.5");

                ctx.u8g2.setFont(FONT_REGULAR_S);
                int valueWidth = ctx.u8g2.getStrWidth(pm25Str);
                ctx.u8g2.setFont(u8g2_font_4x6_tf);
                int unitWidth = ctx.u8g2.getStrWidth("ugm2");
                int totalValueWidth = valueWidth + unitWidth + 1;
                int valueX = startX2 + (sectionWidth - totalValueWidth) / 2;
                ctx.u8g2.setFont(FONT_REGULAR_S);
                ctx.u8g2.drawStr(valueX, 60, pm25Str);
                ctx.u8g2.setFont(u8g2_font_4x6_tf);
                ctx.u8g2.drawStr(valueX + valueWidth + 1, 60, "ugm2");
            }

            // PM10
            {
                ctx.u8g2.setFont(FONT_BOLD_S);
                int labelWidth = ctx.u8g2.getStrWidth("PM10");
                int labelX = startX3 + (128 - startX3 - labelWidth) / 2;
                ctx.u8g2.drawStr(labelX, 50, "PM10");

                ctx.u8g2.setFont(FONT_REGULAR_S);
                int valueWidth = ctx.u8g2.getStrWidth(pm10Str);
                ctx.u8g2.setFont(u8g2_font_4x6_tf);
                int unitWidth = ctx.u8g2.getStrWidth("ugm2");
                int totalValueWidth = valueWidth + unitWidth + 1;
                int valueX = startX3 + (128 - startX3 - totalValueWidth) / 2;
                ctx.u8g2.setFont(FONT_REGULAR_S);
                ctx.u8g2.drawStr(valueX, 60, pm10Str);
                ctx.u8g2.setFont(u8g2_font_4x6_tf);
                ctx.u8g2.drawStr(valueX + valueWidth + 1, 60, "ugm2");
            }
            break;
        }
        case 2:
        { // flujo configurado

            float targetFlow = ctx.session.targetFlow;
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