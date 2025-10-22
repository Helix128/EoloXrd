#ifndef LOGO_SCENE_H
#define LOGO_SCENE_H

#include "IScene.h"
#include "../Data/Context.h"
#include "../Drawing/Logos.h"
#include "../Drawing/SceneManager.h"

// Escena de logo/splash al iniciar la app
class LogoScene : public IScene
{
private:
    unsigned long startTime;
    static const unsigned long ANIM_DURATION = 1250; // 1.5 segundos
    static const unsigned long SPLASH_DURATION = 750; // 0.5 segundos

public:
    void enter(Context &ctx) override
    {
        startTime = millis();
    }

    int smoothstep(int start, int end, unsigned long t)
    {
        if (t < 0) t = 0;
        if (t > 1000) t = 1000;
        int delta = end - start;
        int val = -delta * t * (t - 2000) / 1000000 + start;
        return val;
    }

    void update(Context &ctx) override
    {
        unsigned long currentTime = millis();
        unsigned long elapsedTime = currentTime - startTime;
        
        // Dibujar el logo una vez al entrar en la escena
        ctx.u8g2.clearBuffer();

        int xPos = smoothstep(-64,32, (elapsedTime*1000)/ANIM_DURATION);
        xPos = constrain(xPos,-64,32);
        ctx.u8g2.setBitmapMode(1);
        ctx.u8g2.drawXBM((int)xPos, 0, 128, 64, cmas);
        ctx.u8g2.drawXBM(64-(int)xPos, 0, 128, 64, cmas);
        ctx.u8g2.sendBuffer();
        // Cambiar de escena después de la duración del splash
        if (elapsedTime > SPLASH_DURATION+ANIM_DURATION)
        {
            SceneManager::setScene("inicio", ctx);
        }
    }
};
#endif