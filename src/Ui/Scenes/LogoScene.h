#ifndef LOGO_SCENE_H
#define LOGO_SCENE_H

#include "../Drawing/Fonts.h"
#include "IScene.h"
#include "../../Common/Data/Context.h"
#include "../Drawing/Logos.h"
#include "../Drawing/SceneManager.h"
#include "../Drawing/Renderer.h"
#include "../../Common/Board/I2CBus.h"

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

    void drawLogo(Context &ctx)
    {
        // cmas es un bitmap 128x64. El isotipo completo ocupa x=6..58,
        // y=7..54 (las tres formas y el signo +); no hay que tomar solo la
        // primera franja vertical porque eso deja el logo recortado.
        Renderer::scaledBitmap(ctx.u8g2, cmas, 128, 6, 7, 53, 48, 3, 0, 28, 25);

        ctx.u8g2.setFont(FONT_LABEL);
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

        Renderer::centeredText(ctx.u8g2, detail, Layout::CenterX, 54, FONT_VALUE);
    }

    void drawBootScreen(Context &ctx)
    {
        ctx.u8g2.clearBuffer();

        const Context::BootPhase phase = ctx.bootPhase.load();
        ctx.u8g2.drawHLine(4, 26, 120); 
        Renderer::centeredText(ctx.u8g2, bootPhaseText(phase), Layout::CenterX, 39, FONT_LABEL);
        drawDetail(ctx, phase);
        ctx.u8g2.setBitmapMode(1);
        drawLogo(ctx);
        if (phase != Context::BootPhase::Ready)
            Renderer::spinner(ctx.u8g2, 116, 11, (uint8_t)((millis() / 120UL) & 7U));
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
