#ifndef WAIT_SCENE_H
#define WAIT_SCENE_H

#include "IScene.h"
#include "../Data/Context.h"
#include "../Drawing/SceneManager.h"
#include "../Drawing/GUI.h"

class WaitScene : public IScene
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

        DateTime now = ctx.components.rtc.now();
        int nowUnix = ctx.components.rtc.now().unixtime();
        if(nowUnix >= ctx.session.startTime){
            SceneManager::setScene("captura",ctx);
        }
        
        ctx.u8g2.setFont(u8g2_font_helvB08_tf);
        ctx.u8g2.drawStr(10, 25, "Esperando...");
        ctx.u8g2.drawStr(10, 34, "Hora de inicio:");
        char timeStr[20];
        DateTime startTime = DateTime(ctx.session.startTime);
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d %02d/%02d/%04d",
            startTime.hour(),
            startTime.minute(),
            startTime.second(),
            startTime.day(),
            startTime.month(),
            startTime.year()
        );
        ctx.u8g2.drawStr(10, 43, timeStr);
        ctx.u8g2.drawStr(10, 52, "Hora actual:");
        char nowStr[20];
        snprintf(nowStr, sizeof(nowStr), "%02d:%02d:%02d %02d/%02d/%04d",
            now.hour(),
            now.minute(),
            now.second(),
            now.day(),
            now.month(),
            now.year()
        );
        ctx.u8g2.drawStr(10, 61, nowStr);
        ctx.u8g2.sendBuffer();
    }
};
#endif