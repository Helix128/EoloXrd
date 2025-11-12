#ifndef CALIBRATION_SCENE_H
#define CALIBRATION_SCENE_H

#include "../../Config.h"
#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/SceneManager.h"

// Escena de calibración del motor
class CalibrationScene : public IScene
{
private:

public:
    void enter(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);
        ctx.u8g2.setFont(FONT_BOLD_S);
        const char* title = "Calibrando bombas...";
        int titleWidth = ctx.u8g2.getStrWidth(title);
        int titleX = (128 - titleWidth) / 2;
        ctx.u8g2.drawStr(titleX, 30, title);
        
        ctx.u8g2.setFont(FONT_REGULAR_S);
        const char* subtitle = "(espere unos minutos)";
        int subtitleWidth = ctx.u8g2.getStrWidth(subtitle);
        int subtitleX = (128 - subtitleWidth) / 2;
        ctx.u8g2.drawStr(subtitleX, 50, subtitle);
        ctx.u8g2.sendBuffer();
        ctx.runCalibration();
        ctx.testCalibration();
        ESP.restart();
    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        ctx.u8g2.sendBuffer();
    }
};
#endif