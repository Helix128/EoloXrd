#ifndef ACERCA_DE_SCENE_H
#define ACERCA_DE_SCENE_H

#include "../Drawing/Fonts.h"
#include "IScene.h"
#include "../../Common/Data/Context.h"
#include "../Drawing/SceneManager.h"
#include "../Drawing/GUI.h"
#include "../Drawing/Logos.h"
#include "../Drawing/Renderer.h"

class AcercaDeScene : public IScene
{
public:
    static constexpr const char *Name = "acerca_de";

    uint16_t frameIntervalMs() const override { return 100; }

    void enter(Context &ctx) override
    {
    }

    void update(Context &ctx) override
    {
        if (ctx.components.input.isButtonPressed())
        {
            SceneManager::setScene("inicio", ctx);
            return;
        }

        ctx.u8g2.clearBuffer();
        ctx.u8g2.setBitmapMode(1);

        // Isotipo C+ a la izquierda
        Renderer::scaledBitmap(ctx.u8g2, cmas, 128, 6, 7, 53, 48, 4, 22, 36, 32);

        // Linea divisoria vertical
        ctx.u8g2.drawVLine(46, 18, 42);

        // Texto informativo a la derecha (Latin-1: \xF1 = n con tilde)
        const int textX = 52;
        ctx.u8g2.setFont(FONT_LABEL);
        ctx.u8g2.drawStr(textX, 26, "Firmware");
        ctx.u8g2.setFont(FONT_VALUE);
        ctx.u8g2.drawStr(textX, 36, "desarrollado por:");
        ctx.u8g2.setFont(FONT_LABEL);
        ctx.u8g2.drawStr(textX, 48, "Diego Mu\xF1oz");
        ctx.u8g2.setFont(FONT_VALUE);
        ctx.u8g2.drawStr(textX, 58, "| Helix128");

        ctx.u8g2.sendBuffer();
    }
};

#endif
