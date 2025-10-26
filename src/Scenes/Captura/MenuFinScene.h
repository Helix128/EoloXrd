#ifndef END_MENU_SCENE_H
#define END_MENU_SCENE_H

#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/GUI.h"
#include "../../Config.h"


typedef struct EndMenuOption{
    const char* label;
    const char* scene;
} EndMenuOption;

// Escena de logo/splash al iniciar la app
class EndMenuScene : public IScene
{
private:
    EndMenuOption menuOptions[2] = {
        {"Reiniciar EOLO", "RESET"},
        {"Regresar", "end"}
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
        
        selectIndex += ctx.components.input.getEncoderDelta();
        selectIndex = constrain(selectIndex, 0, 1);
        if(ctx.components.input.isButtonPressed()){
             
            ctx.components.input.resetCounter();
            SceneManager::setScene(menuOptions[selectIndex].scene,ctx);
        }
        ctx.u8g2.setFont(u8g2_font_helvB10_tf);
        for (int i = 0; i < 2; i++)
        {   
            if(selectIndex==i){
                ctx.u8g2.drawBox(-1, 32 + i * 14 - 14, 131, 14);
                ctx.u8g2.setDrawColor(0);
            } else {
                ctx.u8g2.setDrawColor(1);
            }
            ctx.u8g2.setFont(selectIndex==i?u8g2_font_helvB10_tf:u8g2_font_helvR10_tf);
            int labelWidth = ctx.u8g2.getStrWidth(menuOptions[i].label);
            int labelX = (128 - labelWidth) / 2;
            ctx.u8g2.drawStr(labelX, 30 + i * 14, menuOptions[i].label);
            ctx.u8g2.setDrawColor(1);
        }

        ctx.u8g2.sendBuffer();
    }
};
#endif