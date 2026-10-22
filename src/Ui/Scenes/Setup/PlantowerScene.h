#ifndef PLANTOWER_SCENE_H
#define PLANTOWER_SCENE_H

#include "../../Drawing/Fonts.h"
#include "../IScene.h"
#include "../../../Common/Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/Renderer.h"
#include "../../Drawing/SceneManager.h"

class PlantowerScene : public IScene
{
private:
    bool enablePM = true;
public:
    static constexpr const char *Name = "plantower";

    uint16_t frameIntervalMs() const override { return 100; }

    void enter(Context &ctx) override
    {
        SceneManager::setScene("wait",ctx);
    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        int delta = ctx.components.input.getEncoderDelta();
        if(delta!=0){
            enablePM = !enablePM;
        }

        if(ctx.components.input.isButtonPressed()){
            ctx.session.usePlantower = enablePM;
            ctx.saveSession();
            SceneManager::setScene("wait",ctx);
        }

        Renderer::yesNoChoice(ctx.u8g2, "Usar sensor PM", enablePM, "+ consumo de energ\xED" "a");
        ctx.u8g2.sendBuffer();
    }
};
#endif
