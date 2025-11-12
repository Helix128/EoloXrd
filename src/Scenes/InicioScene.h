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
    MenuOption menuOptions[4] = {
        {"Nueva sesion", "flujo"},
        {"Continuar sesion", "wait"},
        {"Captura rapida", "flujo_now"},
        {"Calibrar bombas", "calibration"}
    };
    MenuOption availableOptions[4];
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
        availableOptions[optionCount++] = menuOptions[3]; // Calibrar motor
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
        
        // Calcular ventana de 3 elementos (offset)
        int offset = 0;
        if (optionCount > 3) {
            offset = selectIndex - 1;
            if (offset < 0) offset = 0;
            if (offset > optionCount - 3) offset = optionCount - 3;
        }

        for (int i = offset; i < offset + min(optionCount - offset, 3); i++)
        {   
            int e = i - offset;
            bool highlight = (selectIndex == i);
            if(highlight){
                ctx.u8g2.drawBox(2, 32 + e * 14 - 14, 131, 14);
                ctx.u8g2.setDrawColor(0);
                ctx.u8g2.setFont(FONT_BOLD);
            } else {
                ctx.u8g2.setDrawColor(1);
                ctx.u8g2.setFont(FONT_REGULAR);
            }
            int textWidth = ctx.u8g2.getStrWidth(availableOptions[i].label);
            int x = (128 / 2) - (textWidth / 2);
            ctx.u8g2.drawStr(x, 30 + e * 14, availableOptions[i].label);

            ctx.u8g2.setDrawColor(1);
        }

        // Indicador de si hay más arriba/abajo (triángulos animados)
        ctx.u8g2.setDrawColor(1);
        unsigned long currentTime = millis();
        int animOffset = ((currentTime / 200) % 2) * 2; 

        if (offset > 0) {
            // Hay elementos arriba -> dibujar triángulo arriba
            ctx.u8g2.drawTriangle(120, 18 + animOffset, 128, 18 + animOffset, 124, 14 + animOffset);
        }
        if (offset + 3 < optionCount) {
            // Hay elementos abajo -> dibujar triángulo abajo
            ctx.u8g2.drawTriangle(120, 60 - animOffset, 128, 60 - animOffset, 124, 64 - animOffset);
        }

        // Scroll knob lateral proporcional
        int trackTop = 18;
        int trackHeight = 48;
        int knobHeight = optionCount > 0 ? trackHeight / optionCount : trackHeight;
        if (knobHeight < 3) knobHeight = 3;
        int knobY = trackTop;
        if (optionCount > 1) {
            knobY = trackTop + ((trackHeight - knobHeight) * selectIndex) / (optionCount - 1);
        }
        ctx.u8g2.drawVLine(0, knobY, knobHeight);

        ctx.u8g2.sendBuffer();
    }
};
#endif