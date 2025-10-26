#ifndef CAPTURE_MENU_SCENE_H
#define CAPTURE_MENU_SCENE_H

#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/GUI.h"
#include "../../Config.h"

typedef struct CaptureMenuOption{
    const char* label;
    std::function<void(Context&)> action;
} CaptureMenuOption;

// Escena de logo/splash al iniciar la app
class MenuCapturaScene : public IScene
{
private:
    int selectIndex = 0;
    CaptureMenuOption menuOptions[8] = {
        {"Continuar", [](Context &ctx) {
            SceneManager::setScene("captura", ctx);
        }},
        {"Finalizar", [](Context &ctx) {
            ctx.endCapture();
        }},
        {"Probar bombas", [](Context &ctx) {
            SceneManager::setScene("captura_bombas", ctx);
        }},
        {"Ajustar flujo", [](Context &ctx) {
            SceneManager::setScene("captura_flujo", ctx);
        }},
        {"Ajustar hora fin", [](Context &ctx) {
            SceneManager::setScene("time_end", ctx);
        }},
        {"Usar sensor PM", [](Context &ctx) {
            SceneManager::setScene("plantower_fin", ctx);
        }}
    };
public:
    void enter(Context &ctx) override
    {
        ctx.pauseCapture();
        ctx.saveSession();
    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);
        ctx.updateCapture();

        int delta = ctx.components.input.encoderDelta;
        bool button = ctx.components.input.isButtonPressed();

        if (delta != 0)
        {
            selectIndex += delta;
            selectIndex = constrain(selectIndex, 0, 5);
        }

        if(button){
            ctx.components.input.resetCounter();
            menuOptions[selectIndex].action(ctx);
        }

        ctx.u8g2.setFont(u8g2_font_helvB10_tf);


        int offset = selectIndex > 2 ? 3:0;
        for (int i = offset; i < 3+offset; i++)
        {   
            int e = i-offset;
            if(selectIndex==i){
                ctx.u8g2.drawBox(2, 32 + e * 14 - 14, 131, 14);
                ctx.u8g2.setDrawColor(0);
            } else {
                ctx.u8g2.setDrawColor(1);
            }
            ctx.u8g2.drawStr(2, 30 + e * 14, menuOptions[i].label);
            ctx.u8g2.setDrawColor(1);
        }

        int scrollHeight = 48 / 12; 
        int scrollY = selectIndex * scrollHeight*2 + 18; 
        ctx.u8g2.drawVLine(0, scrollY, scrollHeight);

        ctx.u8g2.sendBuffer();
    }
};
#endif