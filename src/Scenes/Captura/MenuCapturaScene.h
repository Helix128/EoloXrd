#ifndef LOGO_SCENE_H
#define LOGO_SCENE_H

#include "IScene.h"
#include "../Data/Context.h"
#include "../Drawing/SceneManager.h"
#include "../Drawing/GUI.h"

typedef struct MenuOption{
    const char* label;
    std::function<void()> action;
} MenuOption;

// Escena de logo/splash al iniciar la app
class TemplateScene : public IScene
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