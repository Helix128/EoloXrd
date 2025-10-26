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

        unsigned long int nowUnix = ctx.getUnixTime();
        if(nowUnix >= ctx.session.startDate.unixtime()){
             
            ctx.components.input.resetCounter();
            SceneManager::setScene("captura",ctx);
            ctx.u8g2.sendBuffer();
            return;
        }
        
        ctx.u8g2.setFont(u8g2_font_helvB08_tf);
        //ctx.u8g2.drawStr(10, 25, "Esperando");

        ctx.u8g2.drawStr(10, 26, "Flujo actual");
        
     
        ctx.u8g2.setFont(u8g2_font_helvB08_tf);
        char flowStr[10];

        float flow = ctx.components.flowSensor.flow;

        #if BAREBONES == true // valores inventados para testing
        flow = millis() % 20;
        #endif

        snprintf(flowStr, sizeof(flowStr), "%.1f", flow);
        ctx.u8g2.drawStr(33, 36, "L/min");
        ctx.u8g2.drawStr(10, 36, flowStr);
        
        ctx.u8g2.drawStr(10, 46, "Inicio");

        ctx.u8g2.setFont(u8g2_font_helvB12_tf);
        char timeStr[20];
        DateTime startTime = ctx.session.startDate;
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d",
            startTime.hour(),
            startTime.minute()
        );
        ctx.u8g2.drawStr(10, 60, timeStr);

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

        ctx.u8g2.setFont(u8g2_font_helvB08_tf);
        ctx.u8g2.drawStr(75, 46, "Restante");   
        ctx.u8g2.drawStr(75, 60, timeLeftStr);

        ctx.u8g2.sendBuffer();
    }
};
#endif