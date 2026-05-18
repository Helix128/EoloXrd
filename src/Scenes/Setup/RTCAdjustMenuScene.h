#ifndef RTC_ADJUST_MENU_SCENE_H
#define RTC_ADJUST_MENU_SCENE_H

#include "../IScene.h"
#include "../../Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/SceneManager.h"

class RTCAdjustMenuScene : public IScene
{
public:
    static constexpr const char *Name = "rtc_adjust";

    uint16_t frameIntervalMs() const override { return 120; }

    void enter(Context &ctx) override
    {
        (void)ctx;
        state = MainMenu;
        selected = 0;
        field = 0;
        spinnerFrame = 0;
        lastStatus = ctx.getRTCNetworkSyncStatus();
        statusShownAtMs = millis();
    }

    void update(Context &ctx) override
    {
        Context::RTCNetworkSyncStatus status = ctx.getRTCNetworkSyncStatus();
        if (status != lastStatus)
        {
            lastStatus = status;
            statusShownAtMs = millis();
        }

        bool syncRunning = status == Context::RTCNetworkSyncStatus::Running;
        int delta = ctx.components.input.getEncoderDelta(1);
        bool pressed = ctx.components.input.isButtonPressed();

        if (syncRunning)
        {
            spinnerFrame++;
        }
        else if (state == MainMenu)
            updateMain(ctx, delta, pressed);
        else if (state == ManualTime)
            updateManual(ctx, delta, pressed);

        if (pressed || syncRunning)
        {
            ctx.components.input.resetCounter();
            ctx.components.input.resetButton();
        }

        draw(ctx);
    }

private:
    enum State
    {
        MainMenu,
        ManualTime
    };

    State state = MainMenu;
    int selected = 0;
    int field = 0;
    int hour = 0;
    int minute = 0;
    uint8_t spinnerFrame = 0;
    Context::RTCNetworkSyncStatus lastStatus = Context::RTCNetworkSyncStatus::Idle;
    uint32_t statusShownAtMs = 0;

    void updateMain(Context &ctx, int delta, bool pressed)
    {
        selected = constrain(selected + delta, 0, 2);
        if (!pressed)
            return;

        if (selected == 0)
        {
            DateTime now = ctx.components.rtc.now();
            hour = now.hour();
            minute = now.minute();
            field = 0;
            state = ManualTime;
        }
        else if (selected == 1)
        {
            ctx.startRTCNetworkSync();
        }
        else
        {
            SceneManager::setScene(ctx.rtcAdjustReturnScene, ctx);
        }
    }

    void updateManual(Context &ctx, int delta, bool pressed)
    {
        if (delta != 0)
        {
            if (field == 0)
                hour = (hour + delta + 24) % 24;
            else
                minute = (minute + delta + 60) % 60;
        }

        if (!pressed)
            return;

        if (field == 0)
        {
            field = 1;
            return;
        }

        DateTime now = ctx.components.rtc.now();
        ctx.components.rtc.adjust(DateTime(now.year(), now.month(), now.day(), hour, minute, 0));
        SceneManager::setScene(ctx.rtcAdjustReturnScene, ctx);
    }

    void draw(Context &ctx)
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        if (state == MainMenu)
        {
            if (ctx.getRTCNetworkSyncStatus() == Context::RTCNetworkSyncStatus::Running)
            {
                drawSyncWaiting(ctx);
            }
            else
            {
                const char *labels[3] = {"Cambiar hora", "Sincronizar", "Regresar"};
                drawMenu(ctx, "Ajustar reloj", labels, 3);
            }
        }
        else if (state == ManualTime)
            drawManual(ctx);

        ctx.u8g2.sendBuffer();
    }

    void drawMenu(Context &ctx, const char *title, const char *const labels[], int count)
    {
        ctx.u8g2.setFont(FONT_BOLD_S);
        centerText(ctx, title, 24);

        for (int i = 0; i < count; i++)
        {
            bool highlight = selected == i;
            int y = 31 + i * 10;
            if (highlight)
            {
                ctx.u8g2.drawBox(2, y - 8, 124, 10);
                ctx.u8g2.setDrawColor(0);
                ctx.u8g2.setFont(FONT_BOLD_S);
            }
            else
            {
                ctx.u8g2.setDrawColor(1);
                ctx.u8g2.setFont(FONT_REGULAR_S);
            }
            centerText(ctx, labels[i], y);
            ctx.u8g2.setDrawColor(1);
        }

        drawStatusLine(ctx);
    }

    void drawManual(Context &ctx)
    {
        ctx.u8g2.setFont(FONT_BOLD_S);
        centerText(ctx, "Cambiar hora", 23);

        char buffer[8];
        snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, minute);
        ctx.u8g2.setFont(FONT_BOLD_L);
        centerText(ctx, buffer, 44);

        ctx.u8g2.setFont(FONT_REGULAR_S);
        centerText(ctx, field == 0 ? "Hora" : "Minuto", 61);
    }

    void drawStatusLine(Context &ctx)
    {
        Context::RTCNetworkSyncStatus status = ctx.getRTCNetworkSyncStatus();

        char buffer[22];
        if (status == Context::RTCNetworkSyncStatus::Success &&
            millis() - statusShownAtMs < 3000)
        {
            snprintf(buffer, sizeof(buffer), "RTC sincronizado");
        }
        else if (status == Context::RTCNetworkSyncStatus::Failed &&
                 millis() - statusShownAtMs < 3000)
        {
            snprintf(buffer, sizeof(buffer), "Fallo sincroniz.");
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "Bat RTC: %s", ctx.components.rtc.getBackupBatteryStatusText());
        }
        ctx.u8g2.setFont(FONT_REGULAR_S);
        centerText(ctx, buffer, 63);
    }

    void drawSyncWaiting(Context &ctx)
    {
        ctx.u8g2.setFont(FONT_BOLD_S);
        centerText(ctx, "Ajustar reloj", 24);

        ctx.u8g2.setFont(FONT_REGULAR_S);
        centerText(ctx, "Sincronizando RTC", 37);

        drawSnakeSpinner(ctx, 64, 50);
    }

    void drawSnakeSpinner(Context &ctx, int cx, int cy)
    {
        static const int8_t points[8][2] = {
            {0, -6}, {4, -4}, {6, 0}, {4, 4},
            {0, 6}, {-4, 4}, {-6, 0}, {-4, -4}
        };

        uint8_t head = spinnerFrame % 8;
        for (uint8_t i = 0; i < 5; ++i)
        {
            uint8_t idx = (head + 8 - i) % 8;
            uint8_t size = i < 2 ? 2 : 1;
            int x = cx + points[idx][0];
            int y = cy + points[idx][1];
            if (size == 2)
                ctx.u8g2.drawBox(x - 1, y - 1, 3, 3);
            else
                ctx.u8g2.drawPixel(x, y);
        }
    }

    void centerText(Context &ctx, const char *text, int y)
    {
        int width = ctx.u8g2.getStrWidth(text);
        ctx.u8g2.drawStr((128 - width) / 2, y, text);
    }

};

#endif
