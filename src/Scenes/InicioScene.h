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
        {"Continuar sesion", "wait"},
        {"Captura rapida", "flujo_now"}
    };
    MenuOption availableOptions[3];
    int optionCount = 0;
public:

    int selectIndex = 0;
    bool canLoad = false;
    void enter(Context &ctx) override
    {   
        canLoad = ctx.canLoadSession();
        
        optionCount = 0;
        availableOptions[optionCount++] = menuOptions[0]; // Nueva sesion
        if(canLoad){
            Serial.println("Carga de sesión disponible");
            availableOptions[optionCount++] = menuOptions[1]; // Cargar sesion
        } else {
            Serial.println("No hay sesión para cargar");
        }
        availableOptions[optionCount++] = menuOptions[2]; // Captura rapida
        
        selectIndex = 0;
    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);
        
        selectIndex += ctx.components.input.getEncoderDelta();
        selectIndex = constrain(selectIndex, 0, optionCount - 1);
        
        if(ctx.components.input.isButtonPressed()){
            ctx.components.input.resetCounter();
            if(strcmp(availableOptions[selectIndex].label, "Continuar sesion") == 0){
                Serial.println("Cargando sesión guardada...");
                ctx.loadSession();
            }
            SceneManager::setScene(availableOptions[selectIndex].scene, ctx);
        }
        
        ctx.u8g2.setFont(FONT_REGULAR);
        
        for (int i = 0; i < optionCount; i++)
        {   
            bool highlight = (selectIndex == i);
            if(highlight){
                ctx.u8g2.drawBox(-1, 32 + i * 14 - 14, 131, 14);
                ctx.u8g2.setDrawColor(0);
                ctx.u8g2.setFont(FONT_BOLD);
            } else {
                ctx.u8g2.setDrawColor(1);
                ctx.u8g2.setFont(FONT_REGULAR);
            }
            int textWidth = ctx.u8g2.getStrWidth(availableOptions[i].label);
            int x = (128 / 2) - (textWidth / 2);
            ctx.u8g2.drawStr(x, 30 + i * 14, availableOptions[i].label);

            ctx.u8g2.setDrawColor(1);
        }

        ctx.u8g2.sendBuffer();
    }
};
#endif