#ifndef BASE_MENU_SCENE_H
#define BASE_MENU_SCENE_H

#include "../Drawing/Fonts.h"
#include "IScene.h"
#include "../../Common/Data/Context.h"
#include "../Drawing/GUI.h"
#include "../Drawing/SceneManager.h"
#include "../Drawing/Renderer.h"

struct MenuOptionAction {
    const char* label;
    void (*action)(Context&);
};

class BaseMenuScene : public IScene {
protected:
    static const int MAX_OPTIONS = 8;
    MenuOptionAction _options[MAX_OPTIONS];
    int _optionCount = 0;
    int _selectIndex = 0;

    void addOption(const char* label, void (*action)(Context&)) {
        if (_optionCount < MAX_OPTIONS) {
            _options[_optionCount++] = {label, action};
        }
    }

    void clearOptions() {
        _optionCount = 0;
        _selectIndex = 0;
    }

    // Hook para que las clases hijas dibujen algo antes o después del menú
    virtual void preDraw(Context& ctx) {}
    virtual void postDraw(Context& ctx) {}

public:
    uint16_t frameIntervalMs() const override { return 180; }

    void update(Context &ctx) override {
        // 1. Manejo de entrada
        int delta = ctx.components.input.getEncoderDelta();
        if (delta != 0) {
            _selectIndex += delta;
            _selectIndex = constrain(_selectIndex, 0, _optionCount - 1);
            LOG_F("Opcion seleccionada: %d\n", _selectIndex);
        }

        if (ctx.components.input.isButtonPressed()) {
            if (_selectIndex >= 0 && _selectIndex < _optionCount) {
                if (_options[_selectIndex].action)
                    _options[_selectIndex].action(ctx);
                return; // Salir por si la acción cambia de escena
            }
        }

        // 2. Dibujado
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);
        
        preDraw(ctx);

        Renderer::menuListOf(ctx.u8g2, _options, _optionCount, _selectIndex);

        postDraw(ctx);
        ctx.u8g2.sendBuffer();
    }
};

#endif
