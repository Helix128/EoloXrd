#ifndef INICIO_SCENE_H
#define INICIO_SCENE_H

#include "IScene.h"
#include "../Data/Context.h"
#include "../Drawing/GUI.h"
#include "../Drawing/SceneManager.h"


typedef struct MenuOption{
    const char* label;
    const char* scene;
} MenuOption;

// Escena de logo/splash al iniciar la app
class InicioScene : public IScene
{
private:
    MenuOption menuOptions[3] = {
        {"Continuar sesion", "continuar"},
        {"Nueva sesion", "setup"},
        {"Captura inmediata", "setup"}
    };
public:

    int selectIndex = 0;

    void enter(Context &ctx) override
    {

    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);
        
        selectIndex += ctx.components.input.encoderDelta;
        selectIndex = constrain(selectIndex, 0, 2);
        if(ctx.components.input.buttonPressed){
            ctx.components.input.resetCounter();
            SceneManager::setScene(menuOptions[selectIndex].scene,ctx);
        }
        for (int i = 0; i < 3; i++)
        {   
            if(selectIndex==i){
            ctx.u8g2.drawFrame(10, 20 + i * 10 - 10, 120, 10);
            }
            ctx.u8g2.drawStr(10, 20 + i * 10, menuOptions[i].label);
        }

        ctx.u8g2.sendBuffer();
    }
};
#endif