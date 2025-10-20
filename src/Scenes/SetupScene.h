#ifndef LOGO_SCENE_H
#define LOGO_SCENE_H

#include "IScene.h"
#include "../Data/Context.h"
#include "../Drawing/GUI.h"
#include "../Drawing/SceneManager.h"

class SetupScene : public IScene
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
        ctx.u8g2.sendBuffer();
    }
};
#endif