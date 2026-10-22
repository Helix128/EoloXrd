#ifndef CAPTURA_BOMBAS_SCENE_H
#define CAPTURA_BOMBAS_SCENE_H

#include "../../Drawing/Fonts.h"
#include "../IScene.h"
#include "../../../Common/Data/Context.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/Renderer.h"
#include "../../Drawing/Units.h"

class CapturaBombasScene : public IScene
{
private:
public:
    static constexpr const char *Name = "captura_bombas";

    uint16_t frameIntervalMs() const override { return 100; }

    int targetPct = 0;
    void enter(Context &ctx) override
    {
    
    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        int pwm1 = ctx.components.motor.pwmValues[0];
        int pwm2 = ctx.components.motor.pwmValues[1];

        FlowData flowData;
        if (!ctx.components.flowSensor.getData(flowData) || !flowData.valid)
        {
            flowData.flow = -1.0;
        }

        char pwm1Str[6];
        snprintf(pwm1Str, sizeof(pwm1Str), "%d", pwm1);
        char pwm2Str[6];
        snprintf(pwm2Str, sizeof(pwm2Str), "%d", pwm2);
        char flowStr[16];
        snprintf(flowStr, sizeof(flowStr), "%.2f " UNIT_FLOW, flowData.flow);

        const Renderer::KeyValue rows[] = {
            {"Motor 1:", pwm1Str},
            {"Motor 2:", pwm2Str},
            {"Flujo:", flowStr},
        };
        Renderer::keyValueRows(ctx.u8g2, rows, 3);

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
