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

    long targetDuration = 0;

    void enter(Context &ctx) override
    {
        if (!isEndTime)
        {
            DateTime now = ctx.components.rtc.now();
            int nowUnix = now.unixtime();
            targetMinute = now.minute()+1;
            targetHour = now.hour();
            targetDay = 0;
        }
        else
        {
            DateTime now = DateTime(ctx.session.startDate.unixtime());
            int nowUnix = now.unixtime();
            targetMinute = now.minute()+1;
            targetHour = now.hour();
        }
    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        int delta = ctx.components.input.getEncoderDelta(5);
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
            if (newTimeAdjusted.unixtime() <= ctx.session.startDate.unixtime())
            {
                DateTime minTime = DateTime(ctx.session.startDate.unixtime() + 60);
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
            Serial.print("Now Unix: ");
            Serial.println(nowUnix);
            Serial.print("Target Unix: ");
            Serial.println(targetUnix);
            Serial.println("Delta");
            Serial.println(targetUnix - nowUnix);
            if (isEndTime)
            {
                ctx.components.input.resetCounter();
                ctx.components.input.resetButton();
                ctx.session.duration = targetTime.unixtime() - ctx.session.startDate.unixtime();

                Serial.print("Duracion establecida: "); 
                Serial.println(ctx.session.duration);
                Serial.print("Hora de fin establecida:");
                Serial.println(targetTime.timestamp());

                delay(10);
                ctx.saveSession();
                SceneManager::setScene("plantower", ctx);
            }
            else
            {
                ctx.session.startDate = targetTime;
                isEndTime = true;
                enter(ctx);
            }
        }
        
        ctx.u8g2.setFont(u8g2_font_helvB12_tf);
        const char* title = isEndTime ? "Hora (fin)" : "Hora (inicio)";
        int titleWidth = ctx.u8g2.getStrWidth(title);
        int titleX = (128 - titleWidth) / 2;
        ctx.u8g2.drawStr(titleX, 30, title);

        ctx.u8g2.setFont(u8g2_font_helvR08_tf);
        char timeBuffer[6];
        sprintf(timeBuffer, "%02d:%02d", targetHour, targetMinute);
        int timeWidth = ctx.u8g2.getStrWidth(timeBuffer);
        int timeX = (128 - timeWidth) / 2;
        ctx.u8g2.drawStr(timeX, 43, timeBuffer);

        char dayText[15];
        if (targetDay == 0)
        {
            strcpy(dayText, "hoy");
        }
        else if (targetDay == 1)
        {
            strcpy(dayText, "en 1 dia");
        }
        else
        {
            sprintf(dayText, "en %i dias", targetDay);
        }
        int dayWidth = ctx.u8g2.getStrWidth(dayText);
        int dayX = (128 - dayWidth) / 2;
        ctx.u8g2.drawStr(dayX, 52, dayText);

        if (isEndTime)
        {
            DateTime now = ctx.components.rtc.now();
            DateTime targetTime(now.year(), now.month(), now.day() + targetDay, targetHour, targetMinute, 0);
            long duration = targetTime.unixtime() - ctx.session.startDate.unixtime();
            
            int durationHours = duration / 3600;
            int durationMinutes = (duration % 3600) / 60;
            
            char durationBuffer[20];
            sprintf(durationBuffer, "Duracion: %dh %dm", durationHours, durationMinutes);
            int durationWidth = ctx.u8g2.getStrWidth(durationBuffer);
            int durationX = (128 - durationWidth) / 2;
            ctx.u8g2.drawStr(durationX, 62, durationBuffer);
        }

        ctx.u8g2.sendBuffer();
    }
};
#endif