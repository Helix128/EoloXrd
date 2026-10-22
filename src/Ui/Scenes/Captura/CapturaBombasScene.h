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

        Renderer::centeredText(ctx.u8g2, "Probar bombas", Layout::CenterX, Layout::KvFirstY, FONT_TITLE);

        FlowData flowData;
        if (!ctx.components.flowSensor.getData(flowData) || !flowData.valid)
        {
            flowData.flow = -1.0;
        }

        constexpr int Motors = MotorManager::motorCount;
        char powerStr[8];
        snprintf(powerStr, sizeof(powerStr), "%d%%", targetPct);
        char labels[Motors][10];
        char pwmStr[Motors][8];
        char flowStr[16];
        if (flowData.flow < 0) snprintf(flowStr, sizeof(flowStr), "--");
        else snprintf(flowStr, sizeof(flowStr), "%.2f " UNIT_FLOW, flowData.flow);

        Renderer::KeyValue rows[Motors + 2];
        int n = 0;
        rows[n++] = {"Potencia", powerStr};
        for (int i = 0; i < Motors; i++)
        {
            if (Motors == 1) snprintf(labels[i], sizeof(labels[i]), "Motor");
            else snprintf(labels[i], sizeof(labels[i]), "Motor %d", i + 1);
            snprintf(pwmStr[i], sizeof(pwmStr[i]), "%d", ctx.components.motor.pwmValues[i]);
            rows[n++] = {labels[i], pwmStr[i]};
        }
        rows[n++] = {"Flujo", flowStr};
        Renderer::keyValueRows(ctx.u8g2, rows, n, 1);
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
