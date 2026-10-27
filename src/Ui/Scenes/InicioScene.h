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

        addOption("Nueva sesi\xF3n", [](Context& ctx) { SceneManager::setScene("flujo", ctx); });
        
        if (canLoad) {
            addOption("Continuar sesi\xF3n", [](Context& ctx) {
                LOG_LN("Cargando sesión guardada...");
                ctx.loadSession();
                SceneManager::setScene("wait", ctx);
            });
        }

        addOption("Captura r\xE1pida", [](Context& ctx) { SceneManager::setScene("flujo_now", ctx); });
        addOption("Opciones", [](Context& ctx) {
            ctx.optionsReturnScene = InicioScene::Name;
            SceneManager::setScene("opciones", ctx);
        });
        addOption("Calibrar bombas", [](Context& ctx) { SceneManager::setScene("calibration", ctx); });
        addOption("Componentes", [](Context& ctx) { SceneManager::setScene("test_components", ctx); });
       // addOption("Acerca de", [](Context& ctx) { SceneManager::setScene("acerca_de", ctx); });
    }
};
#endif
