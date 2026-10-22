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

    int lastRead = 0;
    const int READ_INTERVAL = 1000;
    void enter(Context &ctx) override
    {   
         ;
        lastRead = millis();
    }

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
            ctx.u8g2.sendBuffer();
            return;
        }
        
        if(millis() - lastRead >= READ_INTERVAL){
            lastRead = millis();
        }

        FlowData flowData;
        if (!ctx.components.flowSensor.getData(flowData) || !flowData.valid)
        {
            flowData.flow = -1.0;
        }

        char flowStr[10];
        if (flowData.flow < 0) snprintf(flowStr, sizeof(flowStr), "--");
        else snprintf(flowStr, sizeof(flowStr), "%.1f", flowData.flow);
        Renderer::metricCell(ctx.u8g2, 0, Layout::ScreenW, "Flujo actual", flowStr, UNIT_FLOW,
                             Layout::HeroLabelY, Layout::HeroValueY, FONT_VALUE, FONT_DISPLAY);

        char timeStr[8];
        DateTime startTime(ctx.session.startUnix);
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", startTime.hour(), startTime.minute());

        long secondsLeft = (long)ctx.session.startUnix - (long)nowUnix;
        char timeLeftStr[12];
        formatHms(timeLeftStr, sizeof(timeLeftStr), secondsLeft > 0 ? secondsLeft : 0);

        Renderer::heroPair(ctx.u8g2, "Inicio", timeStr, "Restante", timeLeftStr);

        ctx.u8g2.sendBuffer();
    }
};
#endif
