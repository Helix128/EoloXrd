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
        ctx.pauseCapture();
    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        ctx.u8g2.setFont(u8g2_font_helvB10_tf);
        ctx.u8g2.drawStr(10, 24, "Motor 1:");
        ctx.u8g2.drawStr(10, 44, "Motor 2:");

        int pwm1 = ctx.components.motor.pwmValues[0];
        int pwm2 = ctx.components.motor.pwmValues[1];

        char pwm1Str[6];
        snprintf(pwm1Str, sizeof(pwm1Str), "%d", pwm1);
        char pwm2Str[6];
        snprintf(pwm2Str, sizeof(pwm2Str), "%d", pwm2);

        ctx.u8g2.setFont(u8g2_font_helvB08_tf);
        ctx.u8g2.drawStr(60, 24, pwm1Str);
        ctx.u8g2.drawStr(60, 44, pwm2Str);

        ctx.u8g2.sendBuffer();

        targetPct += ctx.components.input.encoderDelta * 1;
        targetPct = constrain(targetPct, 0, 100);
        ctx.components.motor.setPowerPct(targetPct);

        if (ctx.components.input.buttonPressed)
        {
            ctx.resumeCapture();
            SceneManager::setScene("captura_menu", ctx);
        }
    }
};
#endif