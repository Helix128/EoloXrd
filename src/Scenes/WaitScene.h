#ifndef WAIT_SCENE_H
#define WAIT_SCENE_H

#include "IScene.h"
#include "../Data/Context.h"
#include "../Drawing/SceneManager.h"
#include "../Drawing/GUI.h"

class WaitScene : public IScene
{
private:

public:
    void enter(Context &ctx) override
    {

    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        DateTime now = ctx.components.rtc.now();
        int nowUnix = ctx.components.rtc.now().unixtime();
        if(nowUnix >= ctx.session.startTime){
            SceneManager::setScene("captura",ctx);
        }
        
        ctx.u8g2.setFont(u8g2_font_helvB08_tf);
        //ctx.u8g2.drawStr(10, 25, "Esperando");

        ctx.u8g2.drawStr(10, 26, "Flujo actual:");
        
     
        ctx.u8g2.setFont(u8g2_font_helvB08_tf);
        char flowStr[10];
        snprintf(flowStr, sizeof(flowStr), "%.1f", ctx.components.flowSensor.flow);
        ctx.u8g2.drawStr(33, 36, "L/min");
        ctx.u8g2.drawStr(10, 36, flowStr);
        
        ctx.u8g2.drawStr(10, 46, "Hora de inicio:");

        ctx.u8g2.setFont(u8g2_font_helvB12_tf);
        char timeStr[20];
        DateTime startTime = DateTime(ctx.session.startTime);
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d",
            startTime.hour(),
            startTime.minute()
        );
        ctx.u8g2.drawStr(10, 60, timeStr);

        ctx.u8g2.sendBuffer();
    }
};
#endif