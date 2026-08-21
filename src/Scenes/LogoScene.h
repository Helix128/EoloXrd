#ifndef LOGO_SCENE_H
#define LOGO_SCENE_H

#include "IScene.h"
#include "../Data/Context.h"
#include "../Drawing/Logos.h"
#include "../Drawing/SceneManager.h"
#include "../Board/I2CBus.h"

class LogoScene : public IScene
{
private:
    enum SceneState : uint8_t
    {
        PREPARING,
        READY_HOLD,
        DONE
    };

    SceneState currentState = PREPARING;
    uint32_t phaseStartTime = 0;
    bool bootTaskStarted = false;

    static constexpr uint32_t MIN_VISIBLE_MS = 900UL;
    static constexpr uint32_t READY_HOLD_MS = 300UL;

    static const char *bootPhaseText(Context::BootPhase phase)
    {
        switch (phase)
        {
        case Context::BootPhase::InitSD:
            return "Revisando SD...";
        case Context::BootPhase::WaitingI2C:
            return "Estabilizando I2C...";
        case Context::BootPhase::Ready:
            return "Listo";
        case Context::BootPhase::StartingServices:
        case Context::BootPhase::Idle:
        default:
            return "Preparando equipo...";
        }
    }

    void drawSpinner(Context &ctx, int cx, int cy)
    {
        static const int8_t points[8][2] = {
            {0, -5}, {4, -4}, {6, 0}, {4, 4},
            {0, 5}, {-4, 4}, {-6, 0}, {-4, -4}
        };
        uint8_t head = (uint8_t)((millis() / 120UL) & 7U);
        for (uint8_t i = 0; i < 5; ++i)
        {
            uint8_t index = (uint8_t)((head + 8U - i) & 7U);
            int x = cx + points[index][0];
            int y = cy + points[index][1];
            if (i < 2)
                ctx.u8g2.drawBox(x - 1, y - 1, 3, 3);
            else
                ctx.u8g2.drawPixel(x, y);
        }
    }

    void drawCentered(Context &ctx, const char *text, int baseline, const uint8_t *font)
    {
        ctx.u8g2.setFont(font);
        int width = ctx.u8g2.getStrWidth(text);
        ctx.u8g2.drawStr((128 - width) / 2, baseline, text);
    }

    bool bitmapPixel(const unsigned char *bitmap, int width, int x, int y)
    {
        const int bytesPerRow = (width + 7) / 8;
        uint8_t value = pgm_read_byte(bitmap + y * bytesPerRow + x / 8);
        return (value & (1U << (x & 7))) != 0;
    }

    void drawLogo(Context &ctx)
    {
        // cmas es un bitmap 128x64. El isotipo completo ocupa x=6..58,
        // y=7..54 (las tres formas y el signo +); no hay que tomar solo la
        // primera franja vertical porque eso deja el logo recortado.
        static constexpr int SourceX = 6;
        static constexpr int SourceY = 7;
        static constexpr int SourceWidth = 53;
        static constexpr int SourceHeight = 48;
        static constexpr int DestX = 3;
        static constexpr int DestY = 0;
        static constexpr int DestWidth = 28;
        static constexpr int DestHeight = 25;

        for (int dy = 0; dy < DestHeight; ++dy)
        {
            int sourceY = SourceY + (dy * (SourceHeight - 1)) / (DestHeight - 1);
            for (int dx = 0; dx < DestWidth; ++dx)
            {
                int sourceX = SourceX + (dx * (SourceWidth - 1)) / (DestWidth - 1);
                // OR de los cuatro píxeles vecinos evita perder trazos finos
                // al reducir el bitmap y conserva también sus bordes.
                bool on = bitmapPixel(cmas, 128, sourceX, sourceY) ||
                          bitmapPixel(cmas, 128, min(sourceX + 1, SourceX + SourceWidth - 1), sourceY) ||
                          bitmapPixel(cmas, 128, sourceX, min(sourceY + 1, SourceY + SourceHeight - 1)) ||
                          bitmapPixel(cmas, 128, min(sourceX + 1, SourceX + SourceWidth - 1),
                                      min(sourceY + 1, SourceY + SourceHeight - 1));
                if (on)
                    ctx.u8g2.drawPixel(DestX + dx, DestY + dy);
            }
        }

        ctx.u8g2.setFont(FONT_BOLD_S);
        ctx.u8g2.drawStr(39, 16, "EOLO");
    }

    void drawDetail(Context &ctx, Context::BootPhase phase)
    {
        char detail[32];
        switch (phase)
        {
        case Context::BootPhase::InitSD:
            snprintf(detail, sizeof(detail), "Acceso a tarjeta local");
            break;
        case Context::BootPhase::WaitingI2C:
        {
            I2CBus &bus = I2CBus::getInstance();
            uint32_t seconds = (bus.warmupRemainingMs() + 999UL) / 1000UL;
            snprintf(detail, sizeof(detail), "%lus restantes", (unsigned long)seconds);
            break;
        }
        case Context::BootPhase::Ready:
            switch (ctx.sdStatus())
            {
            case SD_MISSING:
                snprintf(detail, sizeof(detail), "SD sin tarjeta");
                break;
            case SD_ERROR:
                snprintf(detail, sizeof(detail), "SD con error");
                break;
            case SD_OK:
                snprintf(detail, sizeof(detail), "SD lista");
                break;
            default:
                snprintf(detail, sizeof(detail), "Continuando...");
                break;
            }
            break;
        case Context::BootPhase::StartingServices:
#ifdef FEATURE_MODEM
            snprintf(detail, sizeof(detail), "Modem en segundo plano");
#else
            snprintf(detail, sizeof(detail), "Iniciando sensores");
#endif
            break;
        case Context::BootPhase::Idle:
        default:
            snprintf(detail, sizeof(detail), "Iniciando...");
            break;
        }

        drawCentered(ctx, detail, 54, FONT_REGULAR_S);
    }

    void drawBootScreen(Context &ctx)
    {
        ctx.u8g2.clearBuffer();

        const Context::BootPhase phase = ctx.bootPhase.load();
        ctx.u8g2.drawHLine(4, 26, 120); 
        drawCentered(ctx, bootPhaseText(phase), 39, FONT_BOLD_S);
        drawDetail(ctx, phase);
        ctx.u8g2.setBitmapMode(1);
        drawLogo(ctx);
        if (phase != Context::BootPhase::Ready)
            drawSpinner(ctx, 116, 11);
        ctx.u8g2.sendBuffer();
        ctx.acknowledgeBootPhaseRendered();
    }

public:
    static constexpr const char *Name = "splash";

    uint16_t frameIntervalMs() const override { return 80; }

    void enter(Context &) override
    {
        currentState = PREPARING;
        phaseStartTime = millis();
        bootTaskStarted = false;
    }

    void update(Context &ctx) override
    {
        uint32_t now = millis();
        uint32_t elapsed = now - phaseStartTime;

        if (currentState == PREPARING)
        {
            drawBootScreen(ctx);

            // Arrancar el worker después del primer frame permite que el
            // usuario vea el estado inicial antes de que SD tome el SPI.
            if (!bootTaskStarted)
            {
                ctx.startBootInitTask();
                bootTaskStarted = true;
            }

            if (ctx.bootInitComplete.load() && elapsed >= MIN_VISIBLE_MS)
            {
                currentState = READY_HOLD;
                phaseStartTime = now;
            }
            return;
        }

        if (currentState == READY_HOLD)
        {
            drawBootScreen(ctx);
            if ((uint32_t)(now - phaseStartTime) >= READY_HOLD_MS)
                currentState = DONE;
            return;
        }

        SceneManager::setScene("inicio", ctx);
    }
};

#endif
