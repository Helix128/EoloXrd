#ifndef CAPTURA_BOMBAS_SCENE_H
#define CAPTURA_BOMBAS_SCENE_H

#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/GUI.h"

class CapturaBombasScene : public IScene
{
private:
public:
    int targetPct = 0;
    void enter(Context &ctx) override
    {
    
    }

    void update(Context &ctx) override
    {
        ctx.components.flowSensor.readData();

        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        ctx.u8g2.setFont(FONT_BOLD);
        ctx.u8g2.drawStr(10, 28, "Motor 1:");
        ctx.u8g2.drawStr(10, 42, "Motor 2:");
        ctx.u8g2.drawStr(10, 56, "Flujo:");

        int pwm1 = ctx.components.motor.pwmValues[0];
        int pwm2 = ctx.components.motor.pwmValues[1];
        float flow = ctx.components.flowSensor.flow;

        char pwm1Str[6];
        snprintf(pwm1Str, sizeof(pwm1Str), "%d", pwm1);
        char pwm2Str[6];
        snprintf(pwm2Str, sizeof(pwm2Str), "%d", pwm2);

        String flowStr = String(flow, 2) + " L/m";

        ctx.u8g2.setFont(FONT_REGULAR_S);
        ctx.u8g2.drawStr(80, 28, pwm1Str);
        ctx.u8g2.drawStr(80, 42, pwm2Str);
        ctx.u8g2.drawStr(80, 56, flowStr.c_str());

        ctx.u8g2.sendBuffer();

        targetPct += ctx.components.input.getEncoderDelta() * 1;
        targetPct = constrain(targetPct, 0, 100);
        ctx.components.motor.setPowerPct(targetPct);

        if (ctx.components.input.isButtonPressed())
        {      
            ctx.components.motor.setPowerPct(0);
            SceneManager::setScene("captura_menu", ctx);
        }
    }
};
#endif