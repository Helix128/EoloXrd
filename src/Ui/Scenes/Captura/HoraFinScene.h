#ifndef TIME_END_SCENE_H
#define TIME_END_SCENE_H

#include "../../Drawing/Fonts.h"
#include "../IScene.h"
#include "../../../Common/Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/Renderer.h"

class TimeEndScene : public IScene
{
public:
    static constexpr const char *Name = "time_end";

    uint16_t frameIntervalMs() const override { return 100; }

    bool isEndTime = false;
    int targetMinute = 30;
    int targetHour = 12;
    int targetDay = 0;

    long targetDuration = 0;

    void enter(Context &ctx) override
    {   
        isEndTime = true;
        DateTime now = DateTime(ctx.session.startUnix+ctx.session.duration);
        int nowUnix = now.unixtime();
        targetMinute = now.minute();
        targetHour = now.hour();
        targetDay = 0;
    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);
        int delta = ctx.components.input.getEncoderDelta();
        if (delta != 0)
        {
            int newMinute = targetMinute + delta;
            int newHour = targetHour;
            int newDay = targetDay;

            if (newMinute >= 60)
            {
            newMinute = 0;
            newHour++;
            }
            else if (newMinute < 0)
            {
            newMinute = 59;
            newHour--;
            }

            if (newHour >= 24)
            {
            newHour = 0;
            newDay++;
            }
            else if (newHour < 0)
            {
            newHour = 23;
            newDay--;
            }

            DateTime now = ctx.components.rtc.now();
            DateTime newTime(now.year(), now.month(), now.day() + newDay, newHour, newMinute, 0);
            if (newTime.unixtime() <= now.unixtime())
            {
            DateTime minTime = DateTime(now.unixtime() + 60);
            newDay = 0;
            newHour = minTime.hour();
            newMinute = minTime.minute();
            }

            if (isEndTime)
            {
            DateTime newTimeAdjusted(now.year(), now.month(), now.day() + newDay, newHour, newMinute, 0);
            if (newTimeAdjusted.unixtime() <= ctx.session.startUnix)
            {
                DateTime minTime = DateTime(ctx.session.startUnix + 60);
                newDay = minTime.day() - now.day();
                newHour = minTime.hour();
                newMinute = minTime.minute();
            }
            }

            delta = 0;
            targetDay = newDay;
            targetHour = newHour;
            targetMinute = newMinute;
        }
     
        if (ctx.components.input.isButtonPressed())
        {
            DateTime now = ctx.components.rtc.now();
            DateTime targetTime(
                now.year(),
                now.month(),
                now.day() + targetDay,
                targetHour,
                targetMinute,
                0);
            unsigned long int nowUnix = ctx.getUnixTime();
            unsigned long int targetUnix = targetTime.unixtime();
            LOG_OUT("Now Unix: ");
            LOG_LN(nowUnix);
            LOG_OUT("Target Unix: ");
            LOG_LN(targetUnix);
            LOG_LN("Delta");
            LOG_LN(targetUnix - nowUnix);
            if (isEndTime)
            {   
                ctx.session.duration = targetTime.unixtime() - ctx.session.startUnix;
                LOG_OUT("Hora de fin establecida:");
                LOG_LN(targetTime.timestamp());
                ctx.saveSession();
                SceneManager::setScene("captura", ctx);
            }
            else
            {
                ctx.session.startUnix = targetTime.unixtime();
                isEndTime = true;
                enter(ctx);
            }
        }

        static int animCounter = 0;
        animCounter++;
        long durationMinutes = -1;
        if (isEndTime)
        {
            DateTime now = ctx.components.rtc.now();
            DateTime targetTime(now.year(), now.month(), now.day() + targetDay, targetHour, targetMinute, 0);
            durationMinutes = (long)(targetTime.unixtime() - ctx.session.startUnix) / 60;
        }
        Renderer::timeSelector(ctx.u8g2, isEndTime ? "Hora (fin)" : "Hora (inicio)",
                               targetHour, targetMinute, targetDay, durationMinutes, animCounter);
        
        ctx.u8g2.sendBuffer();
    }
};
#endif
