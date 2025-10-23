#ifndef TIME_SCENE_H
#define TIME_SCENE_H

#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/SceneManager.h"

class TimeScene : public IScene
{
public:
    bool isEndTime = false;
    int targetMinute = 30;
    int targetHour = 12;
    int targetDay = 0;

    void enter(Context &ctx) override
    {
        DateTime now = ctx.components.rtc.now();
        int nowUnix = ctx.components.rtc.now().unixtime();
        targetMinute = now.minute();
        targetHour = now.hour();
        targetDay = 0;
    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        int delta = ctx.components.input.encoderDelta;
        if (delta != 0)
        {
            Serial.println("Delta tiempo: " + String(delta));
            Serial.println("Hora objetivo: " + String(targetHour) + ":" + String(targetMinute) + " Dia offset: " + String(targetDay));
            DateTime now = ctx.components.rtc.now();
            DateTime targetTime(
                now.year(),
                now.month(),
                now.day() + targetDay,
                targetHour,
                targetMinute,
                0);
            if (targetTime.unixtime() < now.unixtime())
            {
                Serial.println("La hora objetivo debe ser mayor o igual a la hora actual.");
                targetMinute = now.minute();
                targetHour = now.hour();
                targetDay = 0;
            }
            if (isEndTime)
            {
                DateTime startTime = DateTime(ctx.session.startTime);
                if (targetTime.unixtime() <= startTime.unixtime())
                {
                    Serial.println("La hora de fin debe ser mayor a la de inicio.");
                    targetMinute = startTime.minute();
                    targetHour = startTime.hour();
                    targetDay = 0;
                }
            }
        }

        targetMinute += delta;
        if (targetMinute >= 60)
        {
            targetMinute = 0;
            targetHour++;
        }
        else if (targetMinute < 0)
        {
            targetMinute = 59;
            targetHour--;
        }

        if (targetHour >= 24)
        {
            targetHour = 0;
            targetDay++;
        }
        else if (targetHour < 0)
        {
            targetHour = 23;
            targetDay--;
        }

        if (ctx.components.input.buttonPressed)
        {
            ctx.components.input.resetButton();
            DateTime now = ctx.components.rtc.now();
            DateTime targetTime(
                now.year(),
                now.month(),
                now.day() + targetDay,
                targetHour,
                targetMinute,
                0);
            unsigned long int nowUnix = now.unixtime();
            unsigned long int targetUnix = targetTime.unixtime();
            Serial.print("Now Unix: ");
            Serial.println(nowUnix);
            Serial.print("Target Unix: ");
            Serial.println(targetUnix);
            Serial.println("Delta");
            Serial.println(targetUnix - nowUnix);
            if (isEndTime)
            {
                ctx.session.endTime = targetTime.unixtime();
                SceneManager::setScene("plantower", ctx);
            }
            else
            {
                ctx.session.startTime = targetTime.unixtime();
                isEndTime = true;
                enter(ctx);
            }
        }
        ctx.u8g2.setFont(u8g2_font_helvB12_tf);
        ctx.u8g2.drawStr(10, 30, isEndTime ? "Hora (fin)" : "Hora (inicio)");

        ctx.u8g2.setFont(u8g2_font_helvB08_tf);
        char timeBuffer[6];
        sprintf(timeBuffer, "%02d:%02d", targetHour, targetMinute);
        ctx.u8g2.drawStr(10, 50, timeBuffer);
        const char *dayText = "";
        if (targetDay == 0)
        {
            dayText = "(hoy)";
        }
        else if (targetDay == 1)
        {
            dayText = "(manana)";
            ctx.u8g2.drawLine(65, 41, 68, 41);
        }
        ctx.u8g2.drawStr(45, 50, dayText);

        ctx.u8g2.sendBuffer();
    }
};
#endif