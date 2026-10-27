#ifndef PANTALLA_SCENE_H
#define PANTALLA_SCENE_H

#include "../IScene.h"
#include "../../../Common/Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/Renderer.h"

// Ajustes de pantalla: brillo, tiempo hasta atenuar y tiempo hasta apagar.
// El brillo se ve en vivo mientras se gira el encoder; el clic confirma y guarda.
class PantallaScene : public IScene
{
public:
    static constexpr const char *Name = "pantalla";

    uint16_t frameIntervalMs() const override { return 100; }

    void enter(Context &ctx) override
    {
        (void)ctx;
        selected = 0;
        editing = false;
    }

    void update(Context &ctx) override
    {
        const int delta = ctx.components.input.getEncoderDelta(1);
        const bool pressed = ctx.components.input.isButtonPressed();

        if (editing)
        {
            if (delta != 0)
                adjust(ctx.displayPower, delta);
            if (pressed)
            {
                editing = false;
                ctx.saveDisplayPower();
            }
        }
        else
        {
            selected = constrain(selected + delta, 0, RowBack);
            if (pressed)
            {
                if (selected == RowBack)
                {
                    SceneManager::setScene("opciones", ctx);
                    return;
                }
                editing = true;
            }
        }

        draw(ctx);
    }

private:
    enum Row
    {
        RowBrightness,
        RowDim,
        RowOff,
        RowBack
    };

    int selected = 0;
    bool editing = false;
    char labels[RowBack + 1][24] = {};

    void adjust(DisplayPowerConfig &cfg, int delta)
    {
        switch (selected)
        {
        case RowBrightness:
            cfg.brightnessPct = (uint8_t)constrain((int)cfg.brightnessPct + (delta > 0 ? 10 : -10),
                                                   DisplayPowerConfig::MinBrightnessPct,
                                                   DisplayPowerConfig::MaxBrightnessPct);
            break;
        case RowDim:
            cfg.stepDim(delta);
            break;
        case RowOff:
            cfg.stepOff(delta);
            break;
        }
    }

    static void formatSeconds(char *out, size_t len, const char *name, uint16_t s)
    {
        if (s == DisplayPowerConfig::Never)
            snprintf(out, len, "%s: Nunca", name);
        else if (s >= 60 && s % 60 == 0)
            snprintf(out, len, "%s: %u min", name, s / 60);
        else
            snprintf(out, len, "%s: %u s", name, s);
    }

    void draw(Context &ctx)
    {
        const DisplayPowerConfig &cfg = ctx.displayPower;
        snprintf(labels[RowBrightness], sizeof(labels[0]), "Brillo: %u%%", cfg.brightnessPct);
        formatSeconds(labels[RowDim], sizeof(labels[0]), "Atenuar", cfg.dimAfterS);
        formatSeconds(labels[RowOff], sizeof(labels[0]), "Apagar", cfg.offAfterS);
        snprintf(labels[RowBack], sizeof(labels[0]), "Volver");

        // En edicion la fila lleva flechas para indicar que el giro cambia el valor.
        char editLabel[28];
        const char *shown[RowBack + 1];
        for (int i = 0; i <= RowBack; i++)
            shown[i] = labels[i];
        if (editing)
        {
            snprintf(editLabel, sizeof(editLabel), "< %s >", labels[selected]);
            shown[selected] = editLabel;
        }

        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);
        Renderer::menuList(ctx.u8g2, shown, RowBack + 1, selected);
        ctx.u8g2.sendBuffer();
    }
};

#endif
