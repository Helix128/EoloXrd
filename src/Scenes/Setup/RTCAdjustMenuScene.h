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
        syncMessage[0] = '\0';
    }

    void update(Context &ctx) override
    {
        int delta = ctx.components.input.getEncoderDelta(1);
        bool pressed = ctx.components.input.isButtonPressed();

        if (state == MainMenu)
            updateMain(ctx, delta, pressed);
        else if (state == ManualTime)
            updateManual(ctx, delta, pressed);
        else if (state == DstMode)
            updateDst(ctx, delta, pressed);
        else if (state == SyncPrompt)
            updateSyncPrompt(ctx, delta, pressed);

        if (pressed)
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
        ManualTime,
        DstMode,
        SyncPrompt
    };

    State state = MainMenu;
    int selected = 0;
    int field = 0;
    int hour = 0;
    int minute = 0;
    ClockSettings::DstMode dstChoice = ClockSettings::Winter;
    char syncMessage[24] = "";

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
            dstChoice = ctx.clockSettings.getDstMode();
            state = DstMode;
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

    void updateDst(Context &ctx, int delta, bool pressed)
    {
        if (delta != 0)
        {
            dstChoice = dstChoice == ClockSettings::Winter ? ClockSettings::Summer : ClockSettings::Winter;
        }

        if (!pressed)
            return;

        ctx.clockSettings.setDstMode(dstChoice);
        selected = 0;
        syncMessage[0] = '\0';
        state = SyncPrompt;
    }

    void updateSyncPrompt(Context &ctx, int delta, bool pressed)
    {
        selected = constrain(selected + delta, 0, 1);
        if (!pressed)
            return;

        if (selected == 0)
        {
            bool ok = ctx.syncRTCFromNtp();
            strncpy(syncMessage, ok ? "RTC sincronizado" : "Fallo sincroniz.", sizeof(syncMessage) - 1);
            syncMessage[sizeof(syncMessage) - 1] = '\0';
        }
        else
        {
            SceneManager::setScene(ctx.rtcAdjustReturnScene, ctx);
        }
    }

    void draw(Context &ctx)
    {
        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);

        if (state == MainMenu)
        {
            const char *labels[3] = {"Cambiar hora", "Verano/invierno", "Regresar"};
            drawMenu(ctx, "Ajustar reloj", labels, 3);
        }
        else if (state == ManualTime)
            drawManual(ctx);
        else if (state == DstMode)
            drawDst(ctx);
        else
            drawSyncPrompt(ctx);

        ctx.u8g2.sendBuffer();
    }

    void drawMenu(Context &ctx, const char *title, const char *const labels[], int count)
    {
        ctx.u8g2.setFont(FONT_BOLD_S);
        centerText(ctx, title, 22);

        for (int i = 0; i < count; i++)
        {
            bool highlight = selected == i;
            int y = 36 + i * 11;
            if (highlight)
            {
                ctx.u8g2.drawBox(2, y - 9, 124, 11);
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

    void drawDst(Context &ctx)
    {
        ctx.u8g2.setFont(FONT_BOLD_S);
        centerText(ctx, "Horario", 24);
        ctx.u8g2.setFont(FONT_REGULAR_S);
        centerText(ctx, dstChoice == ClockSettings::Summer ? "Verano UTC-3" : "Invierno UTC-4", 43);
        centerText(ctx, "Pulsar guarda", 61);
    }

    void drawSyncPrompt(Context &ctx)
    {
        if (syncMessage[0] != '\0')
        {
            ctx.u8g2.setFont(FONT_BOLD_S);
            centerText(ctx, syncMessage, 27);
        }
        else
        {
            ctx.u8g2.setFont(FONT_BOLD_S);
            centerText(ctx, "Sincronizar ahora?", 24);
        }

        const char *labels[2] = {"Sincronizar ahora", "Regresar"};
        drawMenu(ctx, "", labels, 2);
    }

    void centerText(Context &ctx, const char *text, int y)
    {
        int width = ctx.u8g2.getStrWidth(text);
        ctx.u8g2.drawStr((128 - width) / 2, y, text);
    }

};

#endif
