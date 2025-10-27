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

            int screenWidth = ctx.u8g2.getDisplayWidth();
            int centerX = screenWidth / 2;
            int padding = 2;

            // Center align title
            ctx.u8g2.setFont(FONT_BOLD_S);
            const char* title = "Fin de captura";
            int titleWidth = ctx.u8g2.getStrWidth(title);
            int titleX = (screenWidth - titleWidth) / 2;
            ctx.u8g2.drawStr(titleX, 24, title);

            DateTime startTime = ctx.session.startDate;
            DateTime endTime = DateTime(ctx.session.startDate.unixtime() + ctx.session.duration);
            float targetFlow = ctx.session.targetFlow;
            float capturedVolume = ctx.session.capturedVolume;

            // Row 1: Inicio
            ctx.u8g2.setFont(FONT_REGULAR_S);
            const char* label1 = "Inicio: ";
            int label1Width = ctx.u8g2.getStrWidth(label1);
            int label1X = centerX - padding - label1Width;
            ctx.u8g2.drawStr(label1X, 34, label1);
            ctx.u8g2.setFont(FONT_BOLD_S);
            char startStr[20];
            snprintf(startStr, sizeof(startStr), "%02d:%02d",
                startTime.hour(),
                startTime.minute()
            );
            int value1X = centerX + padding;
            ctx.u8g2.drawStr(value1X, 34, startStr);

            // Row 2: Fin
            ctx.u8g2.setFont(FONT_REGULAR_S);
            const char* label2 = "Fin: ";
            int label2Width = ctx.u8g2.getStrWidth(label2);
            int label2X = centerX - padding - label2Width;
            ctx.u8g2.drawStr(label2X, 44, label2);
            ctx.u8g2.setFont(FONT_BOLD_S);
            char endStr[20];
            snprintf(endStr, sizeof(endStr), "%02d:%02d",
                endTime.hour(),
                endTime.minute()
            );
            int value2X = centerX + padding;
            ctx.u8g2.drawStr(value2X, 44, endStr);

            // Row 3: Flujo
            ctx.u8g2.setFont(FONT_REGULAR_S);
            const char* label3 = "Flujo: ";
            int label3Width = ctx.u8g2.getStrWidth(label3);
            int label3X = centerX - padding - label3Width;
            ctx.u8g2.drawStr(label3X, 54, label3);
            ctx.u8g2.setFont(FONT_BOLD_S);
            char targetStr[20];
            snprintf(targetStr, sizeof(targetStr), "%.1f L/h", targetFlow);
            int value3X = centerX + padding;
            ctx.u8g2.drawStr(value3X, 54, targetStr);

            // Row 4: Vol. capturado
            ctx.u8g2.setFont(FONT_REGULAR_S);
            const char* label4 = "Vol. captura: ";
            int label4Width = ctx.u8g2.getStrWidth(label4);
            int label4X = centerX - padding - label4Width;
            ctx.u8g2.drawStr(label4X, 64, label4);
            ctx.u8g2.setFont(FONT_BOLD_S);
            char capturedStr[20];
            snprintf(capturedStr, sizeof(capturedStr), "%.1f m3", capturedVolume*0.001);
            int value4X = centerX + padding;
            ctx.u8g2.drawStr(value4X, 64, capturedStr);

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