#ifndef END_MENU_SCENE_H
#define END_MENU_SCENE_H

#include "../BaseMenuScene.h"

class EndMenuScene : public BaseMenuScene
{
public:
    void enter(Context &ctx) override
    {
        clearOptions();
        addOption("Reiniciar EOLO", [](Context& ctx) { SceneManager::setScene("RESET", ctx); });
        addOption("Regresar", [](Context& ctx) { SceneManager::setScene("end", ctx); });
    }
};
#endif