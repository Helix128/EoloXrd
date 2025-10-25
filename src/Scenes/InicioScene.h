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
        {"Nueva sesion", "flujo"},
        {"Cargar sesion", "captura"},
        {"Capturar ahora", "flujo_now"}
    };
public:

    int selectIndex = 0;
    bool canLoad = false;
    void enter(Context &ctx) override
    {
        canLoad = ctx.canLoadSession();
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
        ctx.u8g2.setFont(u8g2_font_helvB10_tf);

        if(!canLoad && selectIndex==1){
            selectIndex += ctx.components.input.encoderDelta;
        }
        
        for (int i = 0; i < 3; i++)
        {   
            if(i==1 && !canLoad){
                ctx.u8g2.setDrawColor(1);
                ctx.u8g2.drawStr(2, 30 + i * 14, "Cargar sesion (x)");
                continue;
            }
            if(selectIndex==i){
                ctx.u8g2.drawBox(-1, 32 + i * 14 - 14, 131, 14);
                ctx.u8g2.setDrawColor(0);
            } else {
                ctx.u8g2.setDrawColor(1);
            }
            ctx.u8g2.drawStr(2, 30 + i * 14, menuOptions[i].label);
            ctx.u8g2.setDrawColor(1);
        }

        ctx.u8g2.sendBuffer();
    }
};
#endif