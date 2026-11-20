#ifndef ACERCA_DE_SCENE_H
#define ACERCA_DE_SCENE_H

#include <cstdio>
#include <Eolo/Core/Version.h>
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
        Renderer::scaledBitmap(ctx.u8g2, cmas, 128, 6, 7, 53, 48, 4, 10, 36, 32);

        // Linea divisoria vertical
        ctx.u8g2.drawVLine(46, 8, 36);

        // Version del firmware a la derecha
        char versionLabel[24];
        std::snprintf(versionLabel, sizeof(versionLabel), "v%s", EoloCore::kFirmwareVersion);

        const int textX = 52;
        ctx.u8g2.setFont(FONT_LABEL);
        ctx.u8g2.drawStr(textX, 21, "Firmware");
        ctx.u8g2.setFont(FONT_HERO);
        ctx.u8g2.drawStr(textX, 39, versionLabel);

        // Modelo centrado en la parte inferior
        ctx.u8g2.drawHLine(4, 48, 120);
        Renderer::centeredText(ctx.u8g2, EoloConfig::kModelName, Layout::CenterX, 60, FONT_VALUE);

        ctx.u8g2.sendBuffer();
    }
};

#endif
