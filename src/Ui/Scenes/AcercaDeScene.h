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
        Renderer::scaledBitmap(ctx.u8g2, cmas, 128, 6, 7, 53, 48, 4, 18, 44, 40);

        // Linea divisoria vertical
        ctx.u8g2.drawVLine(60, 14, 48);

        // Texto informativo a la derecha (Latin-1: \xF1 = n con tilde)
        const Renderer::KeyValue rows[] = {
            {"", "Firmware"},
            {"", "desarrollado por:"},
            {"", "Diego Mu\xF1oz"},
            {"", "| Helix128"},
        };
        Renderer::keyValueRows(ctx.u8g2, rows, 4);

        ctx.u8g2.sendBuffer();
    }
};

#endif
