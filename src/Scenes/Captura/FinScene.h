#ifndef END_SCENE_H
#define END_SCENE_H

#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/GUI.h"


class FinScene : public IScene
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

        ctx.u8g2.setFont(u8g2_font_helvB08_tf);
        ctx.u8g2.drawStr(10, 24, "Fin de captura");

        DateTime startTime = ctx.session.startDate;
        DateTime endTime = DateTime(ctx.session.startDate.unixtime() + ctx.session.duration);
        float targetFlow = ctx.session.targetFlow;
        float capturedVolume = ctx.session.capturedVolume;

        ctx.u8g2.setFont(u8g2_font_helvB08_tf);
        char startStr[20];
        snprintf(startStr, sizeof(startStr), "%02d:%02d",
            startTime.hour(),
            startTime.minute()
        );
        startTime.toString(startStr);
   
        ctx.u8g2.drawStr(10, 34, "Inicio: ");
        ctx.u8g2.drawStr(60, 34, startStr);

        char endStr[20];
        snprintf(endStr, sizeof(endStr), "%02d:%02d",
            endTime.hour(),
            endTime.minute()
        );
        endTime.toString(endStr);
        
        ctx.u8g2.drawStr(10, 44, "Fin: ");
        ctx.u8g2.drawStr(60, 44, endStr);

        char targetStr[20];
        snprintf(targetStr, sizeof(targetStr), "%.1f L/h", targetFlow);
        ctx.u8g2.drawStr(10, 54, "Flujo: ");
        ctx.u8g2.drawStr(60, 54, targetStr);

        char capturedStr[20];
        snprintf(capturedStr, sizeof(capturedStr), "%.1f L", capturedVolume);
        ctx.u8g2.drawStr(10, 64, "Vol. capturado: ");
        ctx.u8g2.drawStr(85, 64, capturedStr);

        ctx.u8g2.sendBuffer();

        if (ctx.components.input.isButtonPressed())
        {    
            ctx.components.input.resetCounter();
            ctx.resetCapture();
            ctx.clearSession();
            SceneManager::setScene("end_menu",ctx);
        }
        
    }
};
#endif