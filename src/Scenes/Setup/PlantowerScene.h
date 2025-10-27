#ifndef PLANTOWER_SCENE_H
#define PLANTOWER_SCENE_H

#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/SceneManager.h"

class PlantowerScene : public IScene
{
private:
    bool enablePM = true;
public:
    void enter(Context &ctx) override
    {

    }

    void update(Context &ctx) override
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        int delta = ctx.components.input.getEncoderDelta();
        if(delta!=0){
            enablePM = !enablePM;
        }

        if(ctx.components.input.isButtonPressed()){      
            ctx.components.input.resetCounter();
            ctx.session.usePlantower = enablePM;
            ctx.saveSession();
            SceneManager::setScene("wait",ctx);
        }
        
        ctx.u8g2.setFont(FONT_BOLD);
        char* text = "Usar sensor PM";
        int textWidth = ctx.u8g2.getStrWidth(text);
        int textX = (128 - textWidth) / 2;
        ctx.u8g2.drawStr(textX,28, text);
      

        
        const int boxWidth = 40;
        const int boxHeight = 15;
        const int gap = 10;
        const int totalWidth = boxWidth * 2 + gap;
        const int startX = (128 - totalWidth) / 2;
        const int leftBoxX = startX;
        const int rightBoxX = startX + boxWidth + gap;
        const int boxY = 35;
        const int textY = 47;

        if (enablePM) {
            ctx.u8g2.setFont(FONT_BOLD_S);
            ctx.u8g2.drawBox(leftBoxX, boxY, boxWidth, boxHeight);  
            ctx.u8g2.setDrawColor(0);  
        } else {
            ctx.u8g2.setFont(FONT_REGULAR_S);
            ctx.u8g2.setDrawColor(1);
            ctx.u8g2.drawFrame(leftBoxX, boxY, boxWidth, boxHeight);  
            ctx.u8g2.setDrawColor(1);  
        }
        char* siStr = "Si";
        int siWidth = ctx.u8g2.getStrWidth(siStr);
        int siX = leftBoxX + (boxWidth - siWidth) / 2;
        ctx.u8g2.drawStr(siX, textY, siStr);

        if (!enablePM) {
              ctx.u8g2.setFont(FONT_BOLD_S);
            ctx.u8g2.setDrawColor(1);
            ctx.u8g2.drawBox(rightBoxX, boxY, boxWidth, boxHeight);  
            ctx.u8g2.setDrawColor(0); 
        } else {
            ctx.u8g2.setFont(FONT_REGULAR_S);
            ctx.u8g2.setDrawColor(1);
            ctx.u8g2.drawFrame(rightBoxX, boxY, boxWidth, boxHeight);  
            ctx.u8g2.setDrawColor(1);  
        }
        char* noStr = "No";
        int noWidth = ctx.u8g2.getStrWidth(noStr);
        int noX = rightBoxX + (boxWidth - noWidth) / 2;
        ctx.u8g2.drawStr(noX, textY, noStr);

        ctx.u8g2.setFont(FONT_REGULAR_S);
        ctx.u8g2.setDrawColor(1); 
        char* infoStr = "+ consumo de energia";
        int infoWidth = ctx.u8g2.getStrWidth(infoStr);
        int infoX = (128 - infoWidth) / 2;
        ctx.u8g2.drawStr(infoX, 60, infoStr);
        ctx.u8g2.sendBuffer();
    }
};
#endif