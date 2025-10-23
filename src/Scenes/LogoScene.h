#ifndef LOGO_SCENE_H
#define LOGO_SCENE_H

#include "IScene.h"
#include "../Data/Context.h"
#include "../Drawing/Logos.h"
#include "../Drawing/SceneManager.h"
#include "../Testing/I2CUtil.h"

// Escena de logo/splash al iniciar la app
class LogoScene : public IScene
{
private:
    unsigned long startTime;
    static const unsigned long ANIM_DURATION = 450;   // 0.45 segundos
    static const unsigned long FADE_DURATION = 400;   // 0.40 segundos
    static const unsigned long SPLASH_DURATION = 950; // 0.95 segundos

public:
    void enter(Context &ctx) override
    {
        startTime = millis();
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

    void drawAnimatedFill(Context &ctx, unsigned long elapsedTime)
    {
        float t = constrain((elapsedTime) / ((float)FADE_DURATION), 0.0f, 1.0f);
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

    void update(Context &ctx) override
    {
        unsigned long currentTime = millis();
        unsigned long elapsedTime = currentTime - startTime;

        // Dibujar el logo una vez al entrar en la escena
        ctx.u8g2.clearBuffer();

        int xPos = smoothstep(-32, 32, (elapsedTime * 1000) / ANIM_DURATION);
        xPos = constrain(xPos, -32, 32);
        ctx.u8g2.setBitmapMode(1);
        ctx.u8g2.drawXBM((int)xPos, 0, 128, 64, cmas);
        ctx.u8g2.drawXBM(64 - (int)xPos, 0, 128, 64, cmas);
        drawAnimatedFill(ctx, elapsedTime);
        ctx.u8g2.sendBuffer();
        // Cambiar de escena después de la duración del splash
        if (elapsedTime > SPLASH_DURATION + ANIM_DURATION)
        {
            Serial.println("Reinicializando I2C y componentes...");
            ctx.components.input.resetCounter();
            I2CUtility i2c;
            i2c.begin();
            i2c.scan(); 
            
            delay(150);

            ctx.begin();

            delay(150);
            SceneManager::setScene("inicio", ctx);
            
            
            
        }
    }
};
#endif