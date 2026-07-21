#ifndef INSTANT_FLUJO_SCENE_H
#define INSTANT_FLUJO_SCENE_H

#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/Renderer.h"

class InstantFlujoScene : public IScene
{
public:
    static constexpr const char *Name = "flujo_now";

    uint16_t frameIntervalMs() const override { return 100; }

    float targetFlow = 5.0f;
    void enter(Context &ctx) override
    {

    }

    void update(Context &ctx) override
    {
        Renderer::beginFrame(ctx.u8g2);
        GUI::displayHeader(ctx);

        float delta = ctx.components.input.getEncoderDelta();
        targetFlow += delta * 0.1f;
        targetFlow = constrain(targetFlow, 0.0f, 8.0f);

        if(ctx.components.input.isButtonPressed()){
             
            ctx.components.input.resetCounter();
            ctx.session.targetFlow = targetFlow;
            ctx.session.startUnix = ctx.components.rtc.now().unixtime();
            ctx.session.duration = 3600;
            ctx.saveSession();
            SceneManager::setScene("plantower",ctx);
        }
        char flowStr[20];
        snprintf(flowStr, sizeof(flowStr), "%.1f L/min", targetFlow);
        
        static int animCounter = 0;
        animCounter++;
        Renderer::valueSelector(ctx.u8g2, "Flujo objetivo", flowStr, animCounter);
        
        Renderer::endFrame(ctx.u8g2);
    }
};
#endif
