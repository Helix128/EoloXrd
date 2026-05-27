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
    enum SceneState
    {
        BOOT_WAIT,
        EOLO_SWEEP_UP,
        FADING_IN,
        FADING_OUT,
        DONE
    };

    SceneState currentState;
    unsigned long phaseStartTime;

    static const unsigned long EOLO_SLIDE_DURATION = 1200;
    static const unsigned long SWEEP_UP_DURATION   = 1000;
    static const unsigned long ANIM_DURATION       = 2200;
    static const unsigned long FADE_DURATION       = 2200;
    static const unsigned long FADE_OUT_DURATION   = 900;
    static const int MAX_PIXEL_SIZE = 3;

    static const int EOLO_START_Y = 60;
    static const int EOLO_REST_Y  = 40;

public:
    static constexpr const char *Name = "splash";

    // Render cada tick del loop — tope real lo pone el bus I2C
    uint16_t frameIntervalMs() const override { return 1; }

    void enter(Context &ctx) override
    {
        phaseStartTime = millis();
        currentState = BOOT_WAIT;
        // I2C más rápido durante el splash para maximizar framerate
        ctx.u8g2.setBusClock(400000);
    }

    float smootherstep(float t)
    {
        t = constrain(t, 0.0f, 1.0f);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    // Back ease-out: overshoots ~10% then snaps to rest — no trig
    float springOut(float t)
    {
        t = constrain(t, 0.0f, 1.0f);
        float s = t - 1.0f;
        return s * s * (2.70158f * s + 1.70158f) + 1.0f;
    }

    int smoothstep_pos(int start, int end, unsigned long t)
    {
        if (t > 1000) t = 1000;
        long long tl = t;
        int delta = end - start;
        return (int)(start + (delta * tl * tl * (3000 - 2 * tl)) / 1000000000LL);
    }

    bool getPixelRaw(Context &ctx, int x, int y)
    {
        uint8_t *buf = ctx.u8g2.getBufferPtr();
        if (x < 0 || x >= 128 || y < 0 || y >= 64) return false;
        int index = x + (y / 8) * 128;
        return (buf[index] & (1 << (y % 8))) != 0;
    }

    // t_norm=1 → visible, t_norm=0 → invisible
    void applyDitherMask(Context &ctx, float t_norm)
    {
        float t = constrain(t_norm, 0.0f, 1.0f);
        float thresholdLimit = t * 17.0f;
        static const uint8_t bayer[4][4] = {
            {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}
        };
        ctx.u8g2.setDrawColor(0);
        for (int x = 0; x < 128; x++)
            for (int y = 0; y < 64; y++)
                if (bayer[x % 4][y % 4] >= thresholdLimit)
                    ctx.u8g2.drawPixel(x, y);
        ctx.u8g2.setDrawColor(1);
    }

    void applyPixelation(Context &ctx, int blockSize)
    {
        if (blockSize <= 1) return;
        for (int y = 0; y < 64; y += blockSize)
            for (int x = 0; x < 128; x += blockSize)
            {
                int sx = min(x + blockSize / 2, 127);
                int sy = min(y + blockSize / 2, 63);
                bool on = getPixelRaw(ctx, sx, sy);
                ctx.u8g2.setDrawColor(on ? 1 : 0);
                ctx.u8g2.drawBox(x, y, blockSize, blockSize);
            }
        ctx.u8g2.setDrawColor(1);
    }

    void drawFrame(Context &ctx, int xPos, float t_layout_norm, float t_dither_norm, int pixelSize)
    {
        ctx.u8g2.clearBuffer();
        ctx.u8g2.setBitmapMode(1);
        ctx.u8g2.drawXBM((int)xPos - 34, 0, 128, 64, cmas);
        ctx.u8g2.drawXBM(30 - (int)xPos, 0, 128, 64, cmas);

        ctx.u8g2.setFont(u8g2_font_helvB24_tf);
        ctx.u8g2.drawStr(64 - xPos + 32, 30, "C+");

        ctx.u8g2.setFont(FONT_REGULAR_S);
        ctx.u8g2.drawStr(64 - xPos + 32 + (int)(32*(1-t_layout_norm)), 45, "Universidad");
        ctx.u8g2.drawStr(64 - xPos + 32 + (int)(64*(1-t_layout_norm)), 55, "del Desarrollo");

        {
            int cx = 128 - 32 - xPos - 4;
            int h  = (int)(1 + 47 * t_layout_norm);
            ctx.u8g2.drawBox(cx, (64 - h) / 2, 2, h);
        }

        applyPixelation(ctx, pixelSize);
        applyDitherMask(ctx, t_dither_norm);
        ctx.u8g2.sendBuffer();
    }

    void centerText(Context &ctx, const char *text, int y)
    {
        int w = ctx.u8g2.getStrWidth(text);
        ctx.u8g2.drawStr((128 - w) / 2, y, text);
    }

    void drawSnakeSpinner(Context &ctx, int cx, int cy)
    {
        static const int8_t pts[8][2] = {
            {0,-6},{4,-4},{6,0},{4,4},{0,6},{-4,4},{-6,0},{-4,-4}
        };
        uint8_t head = (uint8_t)((millis() / 120) & 7);
        for (uint8_t i = 0; i < 5; ++i)
        {
            uint8_t idx = (head + 8 - i) % 8;
            bool big = i < 2;
            int x = cx + pts[idx][0];
            int y = cy + pts[idx][1];
            if (big) ctx.u8g2.drawBox(x-1, y-1, 3, 3);
            else     ctx.u8g2.drawPixel(x, y);
        }
    }

    // Normaliza un rango de slideT a [0,1] con smootherstep.
    float rangeT(float slideT, float start, float end)
    {
        return smootherstep(constrain((slideT - start) / (end - start), 0.0f, 1.0f));
    }

    // slideT controla la aparición escalonada del contenido debajo de EOLO.
    // Cada elemento tiene su propio rango para efecto cascada natural.
    // No llama sendBuffer.
    void drawBootWaitContent(Context &ctx, int eoloY, float slideT)
    {
        ctx.u8g2.clearBuffer();

        ctx.u8g2.setFont(u8g2_font_helvB24_tf);
        centerText(ctx, "EOLO", eoloY);

        // Fase: sube suavemente desde 14px abajo
        float textT = rangeT(slideT, 0.70f, 0.92f);
        if (textT < 0.01f) return;

        const char *phaseText = "Iniciando...";
        float progress = 0.0f;
        uint32_t phaseElapsed = millis() - ctx.bootPhaseStartMs;

        switch (ctx.bootPhase)
        {
        case Context::BootPhase::Idle:
            progress = 0.0f;
            break;
#ifdef FEATURE_MODEM
        case Context::BootPhase::StartingModem:
        {
            phaseText = "Iniciando modem...";
            // El modem tarda ~22s; la barra avanza hasta 70% en ese tiempo
            float t  = constrain((float)phaseElapsed / 22000.0f, 0.0f, 1.0f);
            progress = t * 0.70f;
            break;
        }
        case Context::BootPhase::InitSD:
        {
            phaseText = "Leyendo SD...";
            float t  = constrain((float)phaseElapsed / 2000.0f, 0.0f, 1.0f);
            progress = 0.70f + t * 0.28f;
            break;
        }
#else
        case Context::BootPhase::InitSD:
        {
            phaseText = "Leyendo SD...";
            float t  = constrain((float)phaseElapsed / 2000.0f, 0.0f, 1.0f);
            progress = t * 0.95f;
            break;
        }
#endif
        case Context::BootPhase::Done:
            phaseText = "Listo";
            progress  = 1.0f;
            break;
        }

        ctx.u8g2.setFont(FONT_REGULAR_S);
        centerText(ctx, phaseText, 50 + (int)((1.0f - textT) * 14));

        // Barra: arranca un poco después del texto, sube desde 10px abajo
        float barT = rangeT(slideT, 0.76f, 0.96f);
        if (barT > 0.01f)
        {
            const int barW = 90, barH = 3;
            const int barX = (128 - barW) / 2;
            const int barY = 56 + (int)((1.0f - barT) * 10);
            ctx.u8g2.drawFrame(barX, barY, barW, barH);
            int filled = (int)(progress * barT * (float)(barW - 2));
            if (filled > 0)
                ctx.u8g2.drawBox(barX + 1, barY + 1, filled, barH - 2);
        }
    }

    void update(Context &ctx) override
    {
        unsigned long elapsedTime = millis() - phaseStartTime;

        switch (currentState)
        {
        case BOOT_WAIT:
        {
            float rawT   = constrain((float)elapsedTime / EOLO_SLIDE_DURATION, 0.0f, 1.0f);
            float slideT = smootherstep(rawT);
            float springT = springOut(rawT);
            int eoloY    = (int)(EOLO_START_Y + (EOLO_REST_Y - EOLO_START_Y) * springT);

            drawBootWaitContent(ctx, eoloY, slideT);
            ctx.u8g2.sendBuffer();

            if (ctx.bootInitComplete)
            {
                currentState = EOLO_SWEEP_UP;
                phaseStartTime = millis();
            }
            return;
        }

        case EOLO_SWEEP_UP:
        {
            float t  = smootherstep(constrain((float)elapsedTime / SWEEP_UP_DURATION, 0.0f, 1.0f));
            int eoloY = (int)(EOLO_REST_Y + (-40 - EOLO_REST_Y) * t);

            // El contenido baja levemente mientras EOLO sube y se disuelve todo
            float exitSlideT = 1.0f - t * 0.6f;
            drawBootWaitContent(ctx, eoloY, exitSlideT);
            applyDitherMask(ctx, 1.0f - t);
            ctx.u8g2.sendBuffer();

            if (elapsedTime >= SWEEP_UP_DURATION)
            {
                currentState = FADING_IN;
                phaseStartTime = millis();
            }
            return;
        }

        case FADING_IN:
        {
            float t       = smootherstep(constrain((float)elapsedTime / FADE_DURATION, 0.0f, 1.0f));
            float pixFact = 1.0f - t;
            int pixelSize = (int)(1.0f + (MAX_PIXEL_SIZE - 1) * pixFact);
            float t_anim  = constrain((float)elapsedTime / ANIM_DURATION, 0.0f, 1.0f);
            float t_layout = smootherstep(t_anim);
            int xPos      = smoothstep_pos(-32, 32, (elapsedTime * 1000UL) / ANIM_DURATION);

            if (elapsedTime > FADE_DURATION)
            {
                pixelSize = 1; t_layout = 1.0f; t = 1.0f; xPos = 32;
                currentState = FADING_OUT;
                phaseStartTime = millis();
                ctx.components.input.resetCounter();
            }

            drawFrame(ctx, xPos, t_layout, t, pixelSize);
            return;
        }

        case FADING_OUT:
        {
            float t       = smootherstep(constrain((float)elapsedTime / FADE_OUT_DURATION, 0.0f, 1.0f));
            int pixelSize = (int)(1.0f + (MAX_PIXEL_SIZE - 1) * t);

            if (elapsedTime > FADE_OUT_DURATION)
            {
                currentState = DONE;
                return;
            }

            drawFrame(ctx, 32, 1.0f, 1.0f - t, pixelSize);
            return;
        }

        case DONE:
            ctx.u8g2.setBusClock(I2C_CLOCK);
            SceneManager::setScene("inicio", ctx);
            return;
        }
    }
};
#endif
