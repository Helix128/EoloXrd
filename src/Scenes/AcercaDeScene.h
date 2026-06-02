#ifndef ACERCA_DE_SCENE_H
#define ACERCA_DE_SCENE_H

#include "IScene.h"
#include "../Data/Context.h"
#include "../Drawing/SceneManager.h"
#include "../Drawing/GUI.h"

class AcercaDeScene : public IScene
{
public:
    static constexpr const char *Name = "acerca_de";

    uint16_t frameIntervalMs() const override { return 100; }

    void enter(Context &ctx) override
    {
        ctx.components.input.resetCounter();
    }

    void update(Context &ctx) override
    {
        if (ctx.components.input.isButtonPressed())
        {
            ctx.components.input.resetCounter();
            SceneManager::setScene("inicio", ctx);
            return;
        }

        ctx.u8g2.clearBuffer();

        // Logo "C+" en la parte izquierda
        ctx.u8g2.setFont(u8g2_font_helvB24_tf);
        ctx.u8g2.drawStr(10, 42, "C+");

        // Línea divisoria vertical
        ctx.u8g2.drawVLine(54, 10, 44);

        // Texto informativo a la derecha
        ctx.u8g2.setFont(FONT_BOLD_S);
        ctx.u8g2.drawStr(62, 20, "Firmware");
        ctx.u8g2.setFont(FONT_REGULAR_S);
        ctx.u8g2.drawStr(62, 32, "desarrollado por:");
        ctx.u8g2.setFont(FONT_BOLD_S);
        ctx.u8g2.drawStr(62, 45, "Diego Muñoz");
        ctx.u8g2.setFont(FONT_REGULAR_S);
        ctx.u8g2.drawStr(62, 55, "| Helix128");

        ctx.u8g2.sendBuffer();
    }
};

#endif
