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
    static const unsigned long SPLASH_DURATION = 3000; // 3 segundos

public:
    void enter(Context &ctx) override
    {
        startTime = millis();

        // Dibujar el logo una vez al entrar en la escena
        ctx.u8g2.clearBuffer();
        ctx.u8g2.drawXBM(32, 0, 128, 64, cmas);
        ctx.u8g2.sendBuffer();
    }

    void update(Context &ctx) override
    {
        unsigned long currentTime = millis();
        unsigned long elapsedTime = currentTime - startTime;

        // Cambiar de escena después de la duración del splash
        if (elapsedTime > SPLASH_DURATION)
        {
            SceneManager::setScene("inicio", ctx);
        }
    }
};
#endif