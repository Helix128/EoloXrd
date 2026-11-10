#ifndef CONFIRMAR_FIN_CAPTURA_SCENE_H
#define CONFIRMAR_FIN_CAPTURA_SCENE_H

#include "../IScene.h"
#include "../../../Common/Data/Context.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/Renderer.h"

class ConfirmarFinCapturaScene : public IScene
{
public:
    static constexpr const char *Name = "confirmar_fin_captura";

    // Cancelar es la opcion predeterminada para evitar cierres accidentales.
    int selectedOption = 0;

    void enter(Context &) override { selectedOption = 0; }

    void update(Context &ctx) override
    {
        Renderer::beginFrame(ctx.u8g2);
        GUI::displayHeader(ctx);

        const int delta = ctx.components.input.getEncoderDelta();
        if (delta != 0)
        {
            selectedOption = (selectedOption + delta) % 2;
            if (selectedOption < 0)
                selectedOption += 2;
        }

        Renderer::yesNoChoice(ctx.u8g2, "Finalizar captura?", selectedOption == 1, "", "Finalizar", "Cancelar");
        Renderer::endFrame(ctx.u8g2);

        if (!ctx.components.input.isButtonPressed())
            return;

        if (selectedOption == 1)
            ctx.endCapture();
        else
            SceneManager::setScene("captura_menu", ctx);
    }
};

#endif
