#ifndef FLUJO_SCENE_H
#define FLUJO_SCENE_H

#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/Renderer.h"

class FlujoScene : public IScene
{
public:
    static constexpr const char *Name = "flujo";

    uint16_t frameIntervalMs() const override { return 100; }

    float targetFlow = 5.0f;
    void enter(Context &ctx) override
    {
        ctx.clearSession();
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
            SceneManager::setScene("tiempo",ctx);
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
