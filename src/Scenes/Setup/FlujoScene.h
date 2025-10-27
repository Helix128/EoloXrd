#ifndef FLUJO_SCENE_H
#define FLUJO_SCENE_H

#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/SceneManager.h"

class FlujoScene : public IScene
{
public:
    
    float targetFlow = 5.0f;
    void enter(Context &ctx) override
    {
        ctx.clearSession();
    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        float delta = ctx.components.input.getEncoderDelta();
        targetFlow += delta * 0.1f;
        targetFlow = constrain(targetFlow, 0.0f, 8.0f);

        if(ctx.components.input.isButtonPressed()){
             
            ctx.components.input.resetCounter();
            ctx.session.targetFlow = targetFlow;
            SceneManager::setScene("tiempo",ctx);
        }
        ctx.u8g2.setFont(FONT_BOLD_L);
        int titleWidth = ctx.u8g2.getStrWidth("Flujo objetivo");
        int titleX = (128 - titleWidth) / 2;
        ctx.u8g2.drawStr(titleX, 30, "Flujo objetivo");
        ctx.u8g2.setFont(FONT_REGULAR);
        char flowStr[20];
        snprintf(flowStr, sizeof(flowStr), "%.1f L/min", targetFlow);
        int valueWidth = ctx.u8g2.getStrWidth(flowStr);
        int valueX = (128 - valueWidth) / 2;
        ctx.u8g2.drawStr(valueX, 50, flowStr);
        ctx.u8g2.sendBuffer();
    }
};
#endif