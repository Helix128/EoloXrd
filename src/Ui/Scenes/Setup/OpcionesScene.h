#ifndef OPCIONES_SCENE_H
#define OPCIONES_SCENE_H

#include "../BaseMenuScene.h"

// Menú de opciones del equipo, accesible desde el inicio y desde la pausa de
// captura. «Volver» regresa a la escena que lo abrió (ctx.optionsReturnScene).
class OpcionesScene : public BaseMenuScene
{
public:
    static constexpr const char *Name = "opciones";

    void enter(Context &ctx) override
    {
        (void)ctx;
        clearOptions();
        addOption("Pantalla", [](Context &ctx) { SceneManager::setScene("pantalla", ctx); });
        addOption("Reloj", [](Context &ctx) {
            ctx.rtcAdjustReturnScene = OpcionesScene::Name;
            SceneManager::setScene("rtc_adjust", ctx);
        });
        addOption("Volver", [](Context &ctx) { SceneManager::setScene(ctx.optionsReturnScene, ctx); });
    }
};
#endif
