#ifndef INICIO_SCENE_H
#define INICIO_SCENE_H

#include "BaseMenuScene.h"

class InicioScene : public BaseMenuScene
{
public:
    static constexpr const char *Name = "inicio";

    void enter(Context &ctx) override
    {
        bool canLoad = ctx.canLoadSession();
        clearOptions();

        addOption("Nueva sesion", [](Context& ctx) { SceneManager::setScene("flujo", ctx); });
        
        if (canLoad) {
            addOption("Continuar sesion", [](Context& ctx) {
                LOG_LN("Cargando sesión guardada...");
                ctx.loadSession();
                SceneManager::setScene("wait", ctx);
            });
        }

        addOption("Captura rapida", [](Context& ctx) { SceneManager::setScene("flujo_now", ctx); });
        addOption("Ajustar reloj", [](Context& ctx) {
            ctx.rtcAdjustReturnScene = InicioScene::Name;
            SceneManager::setScene("rtc_adjust", ctx);
        });
        addOption("Calibrar bombas", [](Context& ctx) { SceneManager::setScene("calibration", ctx); });
        addOption("Acerca de", [](Context& ctx) { SceneManager::setScene("acerca_de", ctx); });
    }
};
#endif
