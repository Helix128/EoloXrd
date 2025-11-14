#ifndef LOGO_SCENE_H
#define LOGO_SCENE_H

#include "IScene.h"
#include "../Data/Context.h"
#include "../Drawing/Logos.h"
#include "../Drawing/SceneManager.h"
#include "../Utility/I2CUtil.h"

class LogoScene : public IScene
{
private:
    enum SceneState
    {
        FADING_IN,
        INITIALIZING,
        FADING_OUT,
        DONE
    };

    SceneState currentState;
    unsigned long phaseStartTime;

    static const unsigned long ANIM_DURATION = 600;
    static const unsigned long FADE_DURATION = 600;
    static const unsigned long FADE_OUT_DURATION = 600;

public:
    void enter(Context &ctx) override
    {
        phaseStartTime = millis();
        currentState = FADING_IN;
    }

    int smoothstep(int start, int end, unsigned long t)
    {
        if (t < 0)
            t = 0;
        if (t > 1000)
            t = 1000;
        int delta = end - start;
        int val = -delta * t * (t - 2000) / 1000000 + start;
        return val;
    }

    // Nueva función: smootherstep para suavizar t en [0,1] usando 6t^5 - 15t^4 + 10t^3
    float smootherstep(float t)
    {
        t = constrain(t, 0.0f, 1.0f);
        // 6t^5 - 15t^4 + 10t^3
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    void drawAnimatedFill(Context &ctx, float t_norm)
    {
        float t = constrain(t_norm, 0.0f, 1.0f);
        t *= 16;

        static const uint8_t bayerPattern[4][4] = {
            {0, 8, 2, 10},
            {12, 4, 14, 6},
            {3, 11, 1, 9},
            {15, 7, 13, 5}};

        ctx.u8g2.setDrawColor(0);
        for (int x = 0; x < 128; x++)
        {
            for (int y = 0; y < 64; y++)
            {
                uint8_t threshold = bayerPattern[x % 4][y % 4];

                if (threshold > t)
                {
                    ctx.u8g2.drawPixel(x, y);
                }
            }
        }
        ctx.u8g2.setDrawColor(1);
    }

    void drawFrame(Context &ctx, int xPos, float t_dither_norm, float t_layout_norm)
    {
        ctx.u8g2.clearBuffer();

        ctx.u8g2.setBitmapMode(1);
        ctx.u8g2.drawXBM((int)xPos - 34, 0, 128, 64, cmas);
        ctx.u8g2.drawXBM(30 - (int)xPos, 0, 128, 64, cmas);

        // Posiciones Y finales
        static const int FINAL_Y_TITLE = 30;
        static const int FINAL_Y_SUB1 = 45;
        static const int FINAL_Y_SUB2 = 55;
        static const int CENTER_Y = 32;
        
        int yTitle = (int)(CENTER_Y + (FINAL_Y_TITLE - CENTER_Y));
        int ySub1 = (int)(CENTER_Y + (FINAL_Y_SUB1 - CENTER_Y));
        int ySub2 = (int)(CENTER_Y + (FINAL_Y_SUB2 - CENTER_Y));

        char* title = "C+";
        char* subtitle1 = "Universidad";
        char* subtitle2 = "del Desarrollo";
        ctx.u8g2.setFont(u8g2_font_helvB24_tf);
        ctx.u8g2.drawStr(64 - xPos + 32, yTitle, title);
        ctx.u8g2.setFont(FONT_REGULAR_S);
        ctx.u8g2.drawStr(64 - xPos + 32+32*(1-t_layout_norm), ySub1, subtitle1);
        ctx.u8g2.drawStr(64 - xPos + 32+64*(1-t_layout_norm), ySub2, subtitle2);

        // línea vertical centrada que crece desde el centro (1px) hasta 48px
        {
            int centerX = 128-32 - xPos-4;
            int minH = 1;
            int maxH = 48;
            int h = (int)(minH + (maxH - minH) * t_layout_norm);
            int y = (64 - h) / 2;
            ctx.u8g2.drawVLine(centerX, y, h);
        }

        drawAnimatedFill(ctx, t_dither_norm);

        ctx.u8g2.sendBuffer();
    }

    void update(Context &ctx) override
    {
        unsigned long currentTime = millis();
        unsigned long elapsedTime = currentTime - phaseStartTime;

        float t_dither_norm = 0.0f;
        float t_layout_norm = 0.0f;
        int xPos = 32;

        switch (currentState)
        {
        case FADING_IN:
        {
            t_dither_norm = (float)elapsedTime / (float)FADE_DURATION;
            // calcular t_layout crudo y aplicar smootherstep para un movimiento más suave
            float t_layout_raw = (float)elapsedTime / (float)ANIM_DURATION;
            t_layout_norm = smootherstep(t_layout_raw);
             xPos = smoothstep(-32, 32, (elapsedTime * 1000) / ANIM_DURATION);

             if (elapsedTime > FADE_DURATION)
             {
                 t_dither_norm = 1.0f;
                t_layout_norm = 1.0f; // ya está en 1 tras easing si elapsedTime grande
                 xPos = 32;
                 currentState = INITIALIZING;
             }
             break;
        }

        case INITIALIZING:
        {

            Serial.println("Reinicializando I2C y componentes...");
            ctx.components.input.resetCounter();

            I2CUtility::scan();
            delay(150);

            ctx.begin();
            delay(150);

            currentState = FADING_OUT;
            phaseStartTime = millis();

            return;
        }

        case FADING_OUT:
        {
            xPos = 32;
            t_layout_norm = 1.0f;
            float t_fade_norm = (float)elapsedTime / (float)FADE_OUT_DURATION;
            t_dither_norm = 1.0f - t_fade_norm;

            if (elapsedTime > FADE_OUT_DURATION)
            {
                t_dither_norm = 0.0f;
                currentState = DONE;
            }
            break;
        }

        case DONE:
        {
            t_dither_norm = 0.0f;
            t_layout_norm = 1.0f;
            xPos = 32;
            
            ctx.u8g2.clearBuffer();
            ctx.u8g2.sendBuffer();
            
            delay(1);

            SceneManager::setScene("inicio", ctx);
            return;
        }
        }

        t_dither_norm = constrain(t_dither_norm, 0.0f, 1.0f);
        t_layout_norm = constrain(t_layout_norm, 0.0f, 1.0f);

        drawFrame(ctx, xPos, t_dither_norm, t_layout_norm);
    }
};
#endif
