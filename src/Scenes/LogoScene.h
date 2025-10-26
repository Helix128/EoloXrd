#ifndef LOGO_SCENE_H
#define LOGO_SCENE_H

#include "IScene.h"
#include "../Data/Context.h"
#include "../Drawing/Logos.h"
#include "../Drawing/SceneManager.h"
#include "../Testing/I2CUtil.h"

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

    void drawFrame(Context &ctx, int xPos, float t_dither_norm)
    {
        ctx.u8g2.clearBuffer();

        ctx.u8g2.setBitmapMode(1);
        ctx.u8g2.drawXBM((int)xPos, 0, 128, 64, cmas);
        ctx.u8g2.drawXBM(64 - (int)xPos, 0, 128, 64, cmas);

        drawAnimatedFill(ctx, t_dither_norm);

        ctx.u8g2.sendBuffer();
    }

    void update(Context &ctx) override
    {
        unsigned long currentTime = millis();
        unsigned long elapsedTime = currentTime - phaseStartTime;

        float t_dither_norm = 0.0f;
        int xPos = 32;

        switch (currentState)
        {
        case FADING_IN:
        {
            t_dither_norm = (float)elapsedTime / (float)FADE_DURATION;
            xPos = smoothstep(-32, 32, (elapsedTime * 1000) / ANIM_DURATION);

            if (elapsedTime > FADE_DURATION)
            {
                t_dither_norm = 1.0f;
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
            xPos = 32;
            
            ctx.u8g2.clearBuffer();
            ctx.u8g2.sendBuffer();
            
            delay(1);

            SceneManager::setScene("inicio", ctx);
            return;
        }
        }

        t_dither_norm = constrain(t_dither_norm, 0.0f, 1.0f);

        drawFrame(ctx, xPos, t_dither_norm);
    }
};
#endif
