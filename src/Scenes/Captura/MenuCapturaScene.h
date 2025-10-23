#ifndef CAPTURE_MENU_SCENE_H
#define CAPTURE_MENU_SCENE_H

#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/GUI.h"
#include "../../Config.h"

typedef struct CaptureMenuOption{
    const char* label;
    std::function<void()> action;
} CaptureMenuOption;

// Escena de logo/splash al iniciar la app
class MenuCapturaScene : public IScene
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
        ctx.updateCapture();

        ctx.u8g2.sendBuffer();
    }
};
#endif