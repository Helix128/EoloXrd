#ifndef PLANTOWER_SCENE_H
#define PLANTOWER_SCENE_H

#include "IScene.h"
#include "../Data/Context.h"
#include "../Drawing/SceneManager.h"
#include "../Drawing/GUI.h"

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

        int delta = ctx.components.input.encoderDelta;
        if(delta!=0){
            enablePM = !enablePM;
        }

        if(ctx.components.input.buttonPressed){
            ctx.components.input.resetButton();
            ctx.session.usePlantower = enablePM;
            SceneManager::setScene("wait",ctx);
        }
        
        ctx.u8g2.setFont(u8g2_font_helvB10_tf);
        ctx.u8g2.drawStr(10,28, "Plantower (MP)");
        ctx.u8g2.setFont(u8g2_font_helvB08_tf);

        
        if (enablePM) {
            ctx.u8g2.drawBox(10, 35, 40, 15);  
            ctx.u8g2.setDrawColor(0);  
        } else {
            ctx.u8g2.setDrawColor(1);
            ctx.u8g2.drawFrame(10, 35, 40, 15);  
            ctx.u8g2.setDrawColor(1);  
        }
        ctx.u8g2.drawStr(15, 45, "Si");

        if (!enablePM) {
            ctx.u8g2.setDrawColor(1);
            ctx.u8g2.drawBox(70, 35, 40, 15);  
            ctx.u8g2.setDrawColor(0); 
        } else {
            ctx.u8g2.setDrawColor(1);
            ctx.u8g2.drawFrame(70, 35, 40, 15);  
            ctx.u8g2.setDrawColor(1);  
        }
        ctx.u8g2.drawStr(75, 45, "No");

        ctx.u8g2.setDrawColor(1); 
        ctx.u8g2.drawStr(10, 60, "+ consumo de energia");
        ctx.u8g2.sendBuffer();
    }
};
#endif