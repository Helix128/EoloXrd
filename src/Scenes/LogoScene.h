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
        EOLO_DISSOLVE,  // fade out the "EOLO" text drawn by initDisplay()
        FADING_IN,
        BOOT_WAIT,      // spinner + progress bar while boot task runs
        FADING_OUT,
        DONE
    };

    SceneState currentState;
    unsigned long phaseStartTime;

    static const unsigned long EOLO_DISSOLVE_DURATION = 400;
    static const unsigned long ANIM_DURATION = 1200;
    static const unsigned long FADE_DURATION = 1200;
    static const unsigned long FADE_OUT_DURATION = 600;
    static const int MAX_PIXEL_SIZE = 3;

public:
    static constexpr const char *Name = "splash";

    uint16_t frameIntervalMs() const override { return 16; }

    void enter(Context &ctx) override
    {
        (void)ctx;
        phaseStartTime = millis();
        currentState = EOLO_DISSOLVE;
    }

    float smootherstep(float t)
    {
        t = constrain(t, 0.0f, 1.0f);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    int smoothstep_pos(int start, int end, unsigned long t)
    {
        if (t < 0) t = 0; if (t > 1000) t = 1000;
        long long t_long = t;
        int delta = end - start;
        return (int)(start + (delta * t_long * t_long * (3000 - 2 * t_long)) / 1000000000);
    }

    bool getPixelRaw(Context &ctx, int x, int y)
    {
        uint8_t* buf = ctx.u8g2.getBufferPtr();
        if (x < 0 || x >= 128 || y < 0 || y >= 64) return false;
        int page = y / 8;
        int index = x + (page * 128);
        uint8_t bitMask = 1 << (y % 8);
        return (buf[index] & bitMask) != 0;
    }

    void applyDitherMask(Context &ctx, float t_norm)
    {
        float t = constrain(t_norm, 0.0f, 1.0f);
        float thresholdLimit = t * 17.0f;

        static const uint8_t bayerPattern[4][4] = {
            {0, 8, 2, 10},
            {12, 4, 14, 6},
            {3, 11, 1, 9},
            {15, 7, 13, 5}
        };

        ctx.u8g2.setDrawColor(0);

        for (int x = 0; x < 128; x++)
        {
            for (int y = 0; y < 64; y++)
            {
                uint8_t threshold = bayerPattern[x % 4][y % 4];
                if (threshold >= thresholdLimit)
                {
                    ctx.u8g2.drawPixel(x, y);
                }
            }
        }
        ctx.u8g2.setDrawColor(1);
    }

    void applyPixelation(Context &ctx, int blockSize)
    {
        if (blockSize <= 1) return;

        for (int y = 0; y < 64; y += blockSize)
        {
            for (int x = 0; x < 128; x += blockSize)
            {
                int sampleX = x + blockSize / 2;
                int sampleY = y + blockSize / 2;
                if (sampleX >= 128) sampleX = 127;
                if (sampleY >= 64) sampleY = 63;

                bool isOn = getPixelRaw(ctx, sampleX, sampleY);

                ctx.u8g2.setDrawColor(isOn ? 1 : 0);
                ctx.u8g2.drawBox(x, y, blockSize, blockSize);
            }
        }
        ctx.u8g2.setDrawColor(1);
    }

    void drawFrame(Context &ctx, int xPos, float t_layout_norm, float t_dither_norm, int pixelSize)
    {
        ctx.u8g2.clearBuffer();

        ctx.u8g2.setBitmapMode(1);
        ctx.u8g2.drawXBM((int)xPos - 34, 0, 128, 64, cmas);
        ctx.u8g2.drawXBM(30 - (int)xPos, 0, 128, 64, cmas);

        static const int CENTER_Y = 32;
        int yTitle = (int)(CENTER_Y + (30 - CENTER_Y));
        int ySub1 = (int)(CENTER_Y + (45 - CENTER_Y));
        int ySub2 = (int)(CENTER_Y + (55 - CENTER_Y));

        ctx.u8g2.setFont(u8g2_font_helvB24_tf);
        ctx.u8g2.drawStr(64 - xPos + 32, yTitle, "C+");

        ctx.u8g2.setFont(FONT_REGULAR_S);
        ctx.u8g2.drawStr(64 - xPos + 32+32*(1-t_layout_norm), ySub1, "Universidad");
        ctx.u8g2.drawStr(64 - xPos + 32+64*(1-t_layout_norm), ySub2, "del Desarrollo");

        {
            int centerX = 128-32 - xPos-4;
            int h = (int)(1 + (47) * t_layout_norm);
            int y = (64 - h) / 2;
            ctx.u8g2.drawVLine(centerX, y, h);
        }

        applyPixelation(ctx, pixelSize);
        applyDitherMask(ctx, t_dither_norm);

        ctx.u8g2.sendBuffer();
    }

    void centerText(Context &ctx, const char *text, int y)
    {
        int width = ctx.u8g2.getStrWidth(text);
        ctx.u8g2.drawStr((128 - width) / 2, y, text);
    }

    void drawSnakeSpinner(Context &ctx, int cx, int cy)
    {
        static const int8_t points[8][2] = {
            {0, -6}, {4, -4}, {6, 0}, {4, 4},
            {0, 6}, {-4, 4}, {-6, 0}, {-4, -4}
        };

        uint8_t head = (uint8_t)((millis() / 120) & 7);
        for (uint8_t i = 0; i < 5; ++i)
        {
            uint8_t idx = (head + 8 - i) % 8;
            uint8_t sz = i < 2 ? 2u : 1u;
            int x = cx + points[idx][0];
            int y = cy + points[idx][1];
            if (sz == 2)
                ctx.u8g2.drawBox(x - 1, y - 1, 3, 3);
            else
                ctx.u8g2.drawPixel(x, y);
        }
    }

    void drawBootWaitScreen(Context &ctx)
    {
        ctx.u8g2.clearBuffer();

        ctx.u8g2.setFont(FONT_BOLD_L);
        centerText(ctx, "EOLO", 18);

        ctx.u8g2.drawHLine(14, 22, 100);

        const char *phaseText = "Iniciando...";
        float progress = 0.0f;
        uint32_t phaseElapsed = millis() - ctx.bootPhaseStartMs;

        switch (ctx.bootPhase)
        {
        case Context::BootPhase::Idle:
            phaseText = "Iniciando...";
            progress = 0.0f;
            break;
#ifdef FEATURE_MODEM
        case Context::BootPhase::StartingModem:
        {
            phaseText = "Iniciando modem...";
            // modem takes ~22s (15s settle + AT negotiation); fill 0-70%
            float t = constrain((float)phaseElapsed / 22000.0f, 0.0f, 1.0f);
            progress = t * 0.70f;
            break;
        }
        case Context::BootPhase::InitSD:
        {
            phaseText = "Leyendo SD...";
            float t = constrain((float)phaseElapsed / 2000.0f, 0.0f, 1.0f);
            progress = 0.70f + t * 0.28f;
            break;
        }
#else
        case Context::BootPhase::InitSD:
        {
            phaseText = "Leyendo SD...";
            float t = constrain((float)phaseElapsed / 2000.0f, 0.0f, 1.0f);
            progress = t * 0.95f;
            break;
        }
#endif
        case Context::BootPhase::Done:
            phaseText = "Listo";
            progress = 1.0f;
            break;
        }

        ctx.u8g2.setFont(FONT_REGULAR_S);
        centerText(ctx, phaseText, 33);

        // Progress bar
        const int barW = 90;
        const int barH = 4;
        const int barX = (128 - barW) / 2;
        const int barY = 40;
        ctx.u8g2.drawRect(barX, barY, barW, barH);
        int filled = (int)(progress * (float)(barW - 2));
        if (filled > 0)
            ctx.u8g2.drawBox(barX + 1, barY + 1, filled, barH - 2);

        drawSnakeSpinner(ctx, 64, 56);

        ctx.u8g2.sendBuffer();
    }

    void update(Context &ctx) override
    {
        unsigned long currentTime = millis();
        unsigned long elapsedTime = currentTime - phaseStartTime;

        float t_layout_norm = 0.0f;
        float t_dither_norm = 0.0f;
        int xPos = 32;
        int pixelSize = 1;

        switch (currentState)
        {
        case EOLO_DISSOLVE:
        {
            float t = (float)elapsedTime / (float)EOLO_DISSOLVE_DURATION;
            t = constrain(t, 0.0f, 1.0f);
            float smooth_t = smootherstep(t);

            ctx.u8g2.clearBuffer();
            ctx.u8g2.setFont(FONT_BOLD_L);
            int textWidth = ctx.u8g2.getStrWidth("EOLO");
            int textX = (128 - textWidth) / 2;
            int textY = 32 + ctx.u8g2.getAscent() / 2;
            ctx.u8g2.drawStr(textX, textY, "EOLO");

            int ps = (int)(1.0f + (float)(MAX_PIXEL_SIZE - 1) * smooth_t);
            applyPixelation(ctx, ps);
            applyDitherMask(ctx, 1.0f - smooth_t);

            ctx.u8g2.sendBuffer();

            if (elapsedTime >= EOLO_DISSOLVE_DURATION)
            {
                currentState = FADING_IN;
                phaseStartTime = millis();
            }
            return;
        }

        case FADING_IN:
        {
            float t = (float)elapsedTime / (float)FADE_DURATION;
            t = constrain(t, 0.0f, 1.0f);
            float smooth_t = smootherstep(t);

            float pixelFactor = 1.0f - smooth_t;
            pixelSize = (int)(1.0f + (float)(MAX_PIXEL_SIZE - 1) * pixelFactor);
            t_dither_norm = smooth_t;

            float t_anim = (float)elapsedTime / (float)ANIM_DURATION;
            t_layout_norm = smootherstep(t_anim);
            xPos = smoothstep_pos(-32, 32, (elapsedTime * 1000) / ANIM_DURATION);

            if (elapsedTime > FADE_DURATION)
            {
                pixelSize = 1;
                t_layout_norm = 1.0f;
                t_dither_norm = 1.0f;
                xPos = 32;
                currentState = BOOT_WAIT;
                phaseStartTime = millis();
                ctx.components.input.resetCounter();
            }
            break;
        }

        case BOOT_WAIT:
        {
            drawBootWaitScreen(ctx);
            if (ctx.bootInitComplete)
            {
                currentState = FADING_OUT;
                phaseStartTime = millis();
            }
            return;
        }

        case FADING_OUT:
        {
            xPos = 32;
            t_layout_norm = 1.0f;

            float t = (float)elapsedTime / (float)FADE_OUT_DURATION;
            t = constrain(t, 0.0f, 1.0f);
            float smooth_t = smootherstep(t);

            pixelSize = (int)(1.0f + (float)(MAX_PIXEL_SIZE - 1) * smooth_t);
            t_dither_norm = 1.0f - smooth_t;

            if (elapsedTime > FADE_OUT_DURATION) currentState = DONE;
            break;
        }

        case DONE:
            SceneManager::setScene("inicio", ctx);
            return;
        }

        drawFrame(ctx, xPos, t_layout_norm, t_dither_norm, pixelSize);
    }
};
#endif
