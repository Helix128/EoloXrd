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
        ctx.beginCapture();
    }

    void update(Context &ctx) override
    {
        ctx.updateCapture();

        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        int delta = ctx.components.input.encoderDelta;
        bool button = ctx.components.input.buttonPressed;

        if (delta != 0)
        {
            cycleFooter = (cycleFooter + delta);
            if(cycleFooter<0)
                cycleFooter = 4;
            else if(cycleFooter>4)
                cycleFooter = 0;
        }

        if(button){
            // TODO: MENU, por ahora solo termina la sesión forzosamente
            ctx.endCapture();
        }

        footer(ctx);
        mainPanel(ctx);

        ctx.u8g2.sendBuffer();
    }

    void updateCapture(Context &ctx)
    {
        if (!ctx.isCapturing)
            return;

        unsigned long now = ctx.getCurrentSeconds();
        ctx.session.elapsedTime = now - ctx.session.startTime;

        if (now > ctx.session.endTime)
        {
            ctx.endCapture();
            return;
        }

        # if !BAREBONES
        ctx.components.flowSensor.readData();
        // adjustMotorPower();
        #else
        ctx.components.flowSensor.flow = ctx.session.targetFlow+millis()%2;
        # endif

        if (now - ctx.session.lastLog >= ctx.CAPTURE_INTERVAL)
        {
            ctx.session.lastLog = now;

            # if !BAREBONES
            ctx.components.logger.capture(
                now,
                ctx.components.flowSensor.flow,
                ctx.session.targetFlow,
                ctx.components.bme.temperature,
                ctx.components.bme.humidity,
                ctx.components.bme.pressure,
                ctx.components.plantower.pm1,
                ctx.components.plantower.pm25,
                ctx.components.plantower.pm10,
                ctx.components.battery.getPct());
            # else
            Serial.println("Capturando datos... (modo barebones, solo serial)");
            # endif
        }
    }
    void mainPanel(Context &ctx)
    {
        float lmin = ctx.components.flowSensor.flow;
        ctx.u8g2.setFont(u8g2_font_helvB12_tf);
        char lminStr[10];
        snprintf(lminStr, sizeof(lminStr), "%.1f", lmin);
        ctx.u8g2.drawStr(50, 30, lminStr);

        ctx.u8g2.setFont(u8g2_font_helvB08_tf);
        ctx.u8g2.drawStr(48, 39, "L/min");
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

            char tempStr[5];
            char humStr[5];
            char presStr[5];
            snprintf(tempStr, sizeof(tempStr), "%.1f", temp);
            snprintf(humStr, sizeof(humStr), "%.1f", hum);
            snprintf(presStr, sizeof(presStr), "%.0f", presAtm);

            ctx.u8g2.setFont(u8g2_font_helvB08_tf);
            ctx.u8g2.drawStr(0, 50, "Temp");
            ctx.u8g2.drawStr(0, 60, tempStr);
            ctx.u8g2.setFont(u8g2_font_4x6_tf);
            ctx.u8g2.drawStr(22, 60, "C");

            ctx.u8g2.setFont(u8g2_font_helvB08_tf);
            ctx.u8g2.drawStr(45, 50, "Hum");
            ctx.u8g2.drawStr(45, 60, humStr);
            ctx.u8g2.setFont(u8g2_font_4x6_tf);
            ctx.u8g2.drawStr(67, 60, "%");

            ctx.u8g2.setFont(u8g2_font_helvB08_tf);
            ctx.u8g2.drawStr(80, 50, "Pres");
            ctx.u8g2.drawStr(80, 60, presStr);
            ctx.u8g2.setFont(u8g2_font_4x6_tf);
            ctx.u8g2.drawStr(102, 60, "hPa");
            break;
        }
        case 1: // pm1 pm2.5 pm10
        {
            if (!ctx.session.usePlantower)
            {
                cycleFooter-=ctx.components.input.encoderDelta;
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

            ctx.u8g2.setFont(u8g2_font_helvB08_tf);
            ctx.u8g2.drawStr(0, 50, "PM1.0");
            ctx.u8g2.drawStr(0, 60, pm1Str);
            ctx.u8g2.setFont(u8g2_font_4x6_tf);
            ctx.u8g2.drawStr(18, 60, "ugm2");

            ctx.u8g2.setFont(u8g2_font_helvB08_tf);
            ctx.u8g2.drawStr(45, 50, "PM2.5");
            ctx.u8g2.drawStr(45, 60, pm25Str);
            ctx.u8g2.setFont(u8g2_font_4x6_tf);
            ctx.u8g2.drawStr(63, 60, "ugm2");

            ctx.u8g2.setFont(u8g2_font_helvB08_tf);
            ctx.u8g2.drawStr(88, 50, "PM10");
            ctx.u8g2.drawStr(88, 60, pm10Str);
            ctx.u8g2.setFont(u8g2_font_4x6_tf);
            ctx.u8g2.drawStr(106, 60, "ugm2");
            break;
        }
        case 2:
        { // flujo configurado

            float targetFlow = ctx.session.targetFlow;
            char targetFlowStr[10];
            snprintf(targetFlowStr, sizeof(targetFlowStr), "%.1f", targetFlow);
            ctx.u8g2.setFont(u8g2_font_helvB08_tf);
            ctx.u8g2.drawStr(25, 50, "Flujo configurado");
            ctx.u8g2.drawStr(42, 60, targetFlowStr);
            ctx.u8g2.setFont(u8g2_font_4x6_tf);
            ctx.u8g2.drawStr(62, 60, "L/min");
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
            ctx.u8g2.setFont(u8g2_font_helvB08_tf);
            ctx.u8g2.drawStr(15, 50, "Tiempo transcurrido");
            ctx.u8g2.drawStr(35, 60, timeStr);
            break;
        }
        case 4: {
            // tiempo restante
            unsigned long duration = ctx.session.endTime - ctx.session.startTime;
            unsigned long remaining = duration - (ctx.session.elapsedTime);
            unsigned long hours = remaining / 3600;
            unsigned long minutes = (remaining % 3600) / 60;
            unsigned long seconds = remaining % 60;

            char remainingStr[9];
            snprintf(remainingStr, sizeof(remainingStr), "%02lu:%02lu:%02lu", hours, minutes, seconds);
            ctx.u8g2.setFont(u8g2_font_helvB08_tf);
            ctx.u8g2.drawStr(15, 50, "Tiempo restante");
            ctx.u8g2.drawStr(35, 60, remainingStr);
            break;
        }
        }
    }
};
#endif