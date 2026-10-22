#ifndef WAIT_SCENE_H
#define WAIT_SCENE_H

#include "../../Drawing/Fonts.h"
#include "../IScene.h"
#include "../../../Common/Data/Context.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/Renderer.h"
#include "../../Drawing/Units.h"
#include "../../../Variants/Legacy.h"

class WaitScene : public IScene
{
public:
    static constexpr const char *Name = "wait";

    uint16_t frameIntervalMs() const override { return 250; }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        if(ctx.components.input.isButtonPressed()){
            ctx.saveSession();
            SceneManager::setScene("inicio",ctx);
            return;
        }
        unsigned long int nowUnix = ctx.getUnixTime();
        if(nowUnix >= ctx.session.startUnix){
             
            SceneManager::setScene("captura",ctx);
            return;
        }

        FlowData flowData;
        if (!ctx.components.flowSensor.getData(flowData) || !flowData.valid)
        {
            flowData.flow = -1.0;
        }

        char flowStr[10];
        if (flowData.flow < 0) snprintf(flowStr, sizeof(flowStr), "--");
        else snprintf(flowStr, sizeof(flowStr), "%.1f", flowData.flow);
        char timeStr[8];
        DateTime startTime(ctx.session.startUnix);
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", startTime.hour(), startTime.minute());

        const unsigned long secondsLeft = ctx.session.startUnix - nowUnix;
        char timeLeftStr[16];
        formatHms(timeLeftStr, sizeof(timeLeftStr), secondsLeft);

        Renderer::centeredText(ctx.u8g2, "Inicio en", Layout::CenterX, 24, FONT_LABEL);
        // Esperas largas conservan todas las cifras sin salir del OLED.
        ctx.u8g2.setFont(FONT_DISPLAY);
        const uint8_t *timeFont = ctx.u8g2.getStrWidth(timeLeftStr) <= Layout::ScreenW - 8
                                     ? FONT_DISPLAY : FONT_HERO;
        Renderer::centeredText(ctx.u8g2, timeLeftStr, Layout::CenterX, 40, timeFont);

        ctx.u8g2.setFont(FONT_VALUE);
        char startStr[16];
        snprintf(startStr, sizeof(startStr), "Inicio %s", timeStr);
        ctx.u8g2.drawStr(4, 51, startStr);
        char flowDetail[24];
        snprintf(flowDetail, sizeof(flowDetail), "%s " UNIT_FLOW, flowStr);
        ctx.u8g2.drawStr(Layout::ScreenW - 4 - ctx.u8g2.getStrWidth(flowDetail), 51, flowDetail);
        ctx.u8g2.drawHLine(4, 54, 120);
        Renderer::centeredText(ctx.u8g2, "Pulsar: volver", Layout::CenterX, 63, FONT_VALUE);

        ctx.u8g2.sendBuffer();
    }
};
#endif
