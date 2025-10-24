#ifndef INSTANT_FLUJO_SCENE_H
#define INSTANT_FLUJO_SCENE_H

#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/SceneManager.h"

class InstantFlujoScene : public IScene
{
public:
    
    float targetFlow = 5.0f;
    void enter(Context &ctx) override
    {

    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        float delta = ctx.components.input.encoderDelta;
        targetFlow += delta * 0.1f;
        targetFlow = constrain(targetFlow, 0.0f, 8.0f);

        if(ctx.components.input.buttonPressed){
             
            ctx.components.input.resetCounter();
            ctx.session.targetFlow = targetFlow;
            ctx.session.startTime = ctx.getCurrentSeconds();
            ctx.session.startDate = DateTime(ctx.session.startTime);
            ctx.session.endTime = ctx.session.startTime + 3600;
            ctx.session.endDate = DateTime(ctx.session.endTime);
            SceneManager::setScene("captura",ctx);
        }
        ctx.u8g2.setFont(u8g2_font_helvB12_tf);
        ctx.u8g2.drawStr(10, 30, "Flujo objetivo");
        char flowStr[10];
        snprintf(flowStr, sizeof(flowStr), "%.1f", targetFlow);
        ctx.u8g2.setFont(u8g2_font_helvB10_tf);
        ctx.u8g2.drawStr(40, 50, flowStr);
        ctx.u8g2.setFont(u8g2_font_helvB08_tf);
        ctx.u8g2.drawStr(80, 50, "L/min");
        ctx.u8g2.sendBuffer();
    }
};
#endif