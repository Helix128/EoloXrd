#ifndef END_SCENE_H
#define END_SCENE_H

#include "IScene.h"
#include "../Data/Context.h"
#include "../Drawing/SceneManager.h"
#include "../Drawing/GUI.h"

class EndScene : public IScene
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

        ctx.u8g2.setFont(u8g2_font_helvB12_tf);
        ctx.u8g2.drawStr(10, 30, "Muestreo terminado");

        DateTime startTime = DateTime(ctx.session.startTime);
        DateTime endTime = DateTime(ctx.session.endTime);
        float targetFlow = ctx.session.targetFlow;
        float capturedVolume = ctx.session.capturedVolume;

        ctx.u8g2.setFont(u8g2_font_helvB08_tf);
        char startStr[20];
        snprintf(startStr, sizeof(startStr), "%02d:%02d",
            startTime.hour(),
            startTime.minute()
        );
        ctx.u8g2.drawStr(10, 45, "Inicio: ");
        ctx.u8g2.drawStr(60, 45, startStr);

        char endStr[20];
        snprintf(endStr, sizeof(endStr), "%02d:%02d",
            endTime.hour(),
            endTime.minute()
        );
        ctx.u8g2.drawStr(10, 60, "Fin: ");
        ctx.u8g2.drawStr(60, 60, endStr);

        char targetStr[20];
        snprintf(targetStr, sizeof(targetStr), "%.1f L/h", targetFlow);
        ctx.u8g2.drawStr(10, 75, "Flujo: ");
        ctx.u8g2.drawStr(60, 75, targetStr);

        char capturedStr[20];
        snprintf(capturedStr, sizeof(capturedStr), "%.1f L", capturedVolume);
        ctx.u8g2.drawStr(10, 90, "Vol. capturado: ");
        ctx.u8g2.drawStr(60, 90, capturedStr);

        ctx.u8g2.sendBuffer();
    }
};
#endif