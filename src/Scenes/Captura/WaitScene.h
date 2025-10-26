#ifndef WAIT_SCENE_H
#define WAIT_SCENE_H

#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/GUI.h"
#include "../../Config.h"

class WaitScene : public IScene
{
public:
    void enter(Context &ctx) override
    {   

    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        if(ctx.components.input.isButtonPressed()){
            ctx.components.input.resetCounter();
            ctx.saveSession();
            SceneManager::setScene("inicio",ctx);
            return;
        }
        unsigned long int nowUnix = ctx.getUnixTime();
        if(nowUnix >= ctx.session.startDate.unixtime()){
             
            ctx.components.input.resetCounter();
            SceneManager::setScene("captura",ctx);
            ctx.u8g2.sendBuffer();
            return;
        }
        
        ctx.u8g2.setFont(u8g2_font_helvB08_tf);
        //ctx.u8g2.drawStr(10, 25, "Esperando");
        int labelWidth = ctx.u8g2.getStrWidth("Flujo actual");
        int labelX = (128 - labelWidth) / 2;
        ctx.u8g2.drawStr(labelX, 23, "Flujo actual");

        ctx.u8g2.setFont(u8g2_font_helvR08_tf);
        char flowStr[10];

        float flow = ctx.components.flowSensor.flow;

        #if BAREBONES == true // valores inventados para testing
        flow = millis() % 20;
        #endif

        snprintf(flowStr, sizeof(flowStr), "%.1f", flow);
        int valueWidth = ctx.u8g2.getStrWidth(flowStr);
        int unitWidth = ctx.u8g2.getStrWidth("L/min");
        int totalWidth = valueWidth + unitWidth + 1;
        int valueX = (128 - totalWidth) / 2;
        ctx.u8g2.drawStr(valueX, 34, flowStr);
        ctx.u8g2.drawStr(valueX + valueWidth + 1, 34, "L/min");
        ctx.u8g2.drawHLine(0, 35, 128);

        char timeStr[20];
        DateTime startTime = ctx.session.startDate;
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d",
            startTime.hour(),
            startTime.minute()
        );

        // Mostrar tiempo faltante
        int secondsLeft = ctx.session.startDate.unixtime() - nowUnix;
        int hoursLeft = secondsLeft / 3600;
        int minutesLeft = (secondsLeft % 3600) / 60;
        int secondsLeftRemainder = secondsLeft % 60;
        
        char timeLeftStr[20];
        snprintf(timeLeftStr, sizeof(timeLeftStr), "%02d:%02d:%02d",
            hoursLeft,
            minutesLeft,
            secondsLeftRemainder
        );
        // Calculate widths for centering
        ctx.u8g2.setFont(u8g2_font_helvB12_tf);
        int width_start = ctx.u8g2.getStrWidth(timeStr);
        int width_remain = ctx.u8g2.getStrWidth(timeLeftStr);
        int space = 10;
        int total_width = width_start + width_remain + space;
        int start_x = (128 - total_width) / 2;
        int left_time_x = start_x;
        int right_time_x = start_x + width_start + space;

        // Calculate label positions
        ctx.u8g2.setFont(u8g2_font_helvR08_tf);
        int width_label_start = ctx.u8g2.getStrWidth("Inicio");
        int width_label_remain = ctx.u8g2.getStrWidth("Restante");
        int left_label_x = left_time_x + (width_start - width_label_start) / 2;
        int right_label_x = right_time_x + (width_remain - width_label_remain) / 2;

        // Draw labels and times
        ctx.u8g2.drawStr(left_label_x, 46, "Inicio");
        ctx.u8g2.setFont(u8g2_font_helvB12_tf);
        ctx.u8g2.drawStr(left_time_x, 60, timeStr);
        ctx.u8g2.setFont(u8g2_font_helvR08_tf);
        ctx.u8g2.drawStr(right_label_x, 46, "Restante");
        ctx.u8g2.setFont(u8g2_font_helvB12_tf);
        ctx.u8g2.drawStr(right_time_x, 60, timeLeftStr);

        ctx.u8g2.sendBuffer();
    }
};
#endif