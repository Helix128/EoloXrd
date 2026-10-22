#ifndef END_SCENE_H
#define END_SCENE_H

#include "../../Drawing/Fonts.h"
#include "../IScene.h"
#include "../../../Common/Data/Context.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/Renderer.h"
#include "../../Drawing/Units.h"


class FinScene : public IScene
{
private:

public:
    static constexpr const char *Name = "end";

    void enter(Context &ctx) override
    {

    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        const char* title = ctx.captureEndReason() == CaptureEndReason::PumpFailureZeroFlow
                                ? "Falla bomba"
                                : "Fin de captura";
        Renderer::centeredText(ctx.u8g2, title, Layout::CenterX, Layout::KvFirstY, FONT_TITLE);

        DateTime startTime(ctx.session.startUnix);
        DateTime endTime = DateTime(ctx.session.startUnix + ctx.session.duration);

        char startStr[8];
        snprintf(startStr, sizeof(startStr), "%02d:%02d", startTime.hour(), startTime.minute());
        char endStr[8];
        snprintf(endStr, sizeof(endStr), "%02d:%02d", endTime.hour(), endTime.minute());
        char flowStr[16];
        snprintf(flowStr, sizeof(flowStr), "%.1f " UNIT_FLOW, ctx.session.targetFlow);
        char volumeStr[16];
        snprintf(volumeStr, sizeof(volumeStr), "%.3f " UNIT_VOLUME, ctx.session.capturedVolume * 0.001);

        const Renderer::KeyValue rows[] = {
            {"Inicio:", startStr},
            {"Fin:", endStr},
            {"Flujo:", flowStr},
            {"Vol. captura:", volumeStr},
        };
        Renderer::keyValueRows(ctx.u8g2, rows, 4, 1);

            ctx.u8g2.sendBuffer();

            if (ctx.components.input.isButtonPressed())
            {    
                ctx.resetCapture();
                ctx.clearSession();
                SceneManager::setScene("end_menu",ctx);
            }
            
    }
};
#endif
