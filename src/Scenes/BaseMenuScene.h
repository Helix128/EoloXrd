#ifndef BASE_MENU_SCENE_H
#define BASE_MENU_SCENE_H

#include "IScene.h"
#include "../Data/Context.h"
#include "../Drawing/GUI.h"
#include "../Drawing/SceneManager.h"

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
            ctx.components.input.resetCounter();
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

        if (_optionCount > 0) {
            // Calcular ventana de 3 elementos
            int offset = 0;
            if (_optionCount > 3) {
                offset = _selectIndex - 1;
                if (offset < 0) offset = 0;
                if (offset > _optionCount - 3) offset = _optionCount - 3;
            }

            for (int i = offset; i < offset + min(_optionCount - offset, 3); i++) {
                int e = i - offset;
                bool highlight = (_selectIndex == i);
                
                if (highlight) {
                    ctx.u8g2.drawBox(2, 32 + e * 14 - 14, 131, 14);
                    ctx.u8g2.setDrawColor(0);
                    ctx.u8g2.setFont(FONT_BOLD);
                } else {
                    ctx.u8g2.setDrawColor(1);
                    ctx.u8g2.setFont(FONT_REGULAR);
                }

                int textWidth = ctx.u8g2.getStrWidth(_options[i].label);
                int x = (128 / 2) - (textWidth / 2);
                ctx.u8g2.drawStr(x, 30 + e * 14, _options[i].label);
                ctx.u8g2.setDrawColor(1);
            }

            // Triángulos de scroll
            unsigned long currentTime = millis();
            int animOffset = ((currentTime / 200) % 2) * 2;
            if (offset > 0) {
                ctx.u8g2.drawTriangle(120, 18 + animOffset, 128, 18 + animOffset, 124, 14 + animOffset);
            }
            if (offset + 3 < _optionCount) {
                ctx.u8g2.drawTriangle(120, 60 - animOffset, 128, 60 - animOffset, 124, 64 - animOffset);
            }

            // Barra lateral
            int trackTop = 18;
            int trackHeight = 48;
            int knobHeight = trackHeight / _optionCount;
            if (knobHeight < 4) knobHeight = 4;
            int knobY = trackTop;
            if (_optionCount > 1) {
                knobY = trackTop + ((trackHeight - knobHeight) * _selectIndex) / (_optionCount - 1);
            }
            ctx.u8g2.drawVLine(0, knobY, knobHeight);
        }

        postDraw(ctx);
        ctx.u8g2.sendBuffer();
    }
};

#endif
