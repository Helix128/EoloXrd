#ifndef CAPTURE_MENU_SCENE_H
#define CAPTURE_MENU_SCENE_H

#include "../BaseMenuScene.h"

class MenuCapturaScene : public BaseMenuScene
{
protected:
    void preDraw(Context& ctx) override {
        ctx.updateCapture();
    }

public:
    static constexpr const char *Name = "captura_menu";

    void enter(Context &ctx) override
    {
        ctx.pauseCapture();
        ctx.saveSession();

        clearOptions();
        addOption("Continuar", [](Context &ctx) { SceneManager::setScene("captura", ctx); });
        addOption("Finalizar", [](Context &ctx) { ctx.endCapture(); });
        addOption("Probar bombas", [](Context &ctx) { SceneManager::setScene("captura_bombas", ctx); });
        addOption("Ajustar hora fin", [](Context &ctx) { SceneManager::setScene("time_end", ctx); });
        addOption("Ajustar reloj", [](Context &ctx) {
            ctx.rtcAdjustReturnScene = MenuCapturaScene::Name;
            SceneManager::setScene("rtc_adjust", ctx);
        });
        addOption("Ajustar flujo", [](Context &ctx) { SceneManager::setScene("captura_flujo", ctx); });
    }
};
#endif
