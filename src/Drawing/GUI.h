#ifndef GUI_H
#define GUI_H

#include <U8g2lib.h>
#include "../Data/Context.h" 
#include "../Config/Legacy.h"
#include "Profiler.h" 
// Helper para dibujar elementos comunes en la UI
class GUI
{
public:
    // Header con batería, hora, SD, nombre del dispositivo
    static void displayHeader(Context &ctx)
    {   
        //Profiler p("GUI displayHeader");
        const UiSnapshot &snapshot = ctx.getUiSnapshot();

        // Zona izquierda: hora y SD.
        ctx.u8g2.setFont(FONT_REGULAR_S);
        int nowMin = snapshot.status.minute;
        int nowHr = snapshot.status.hour;
        char timeStr[9];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", nowHr, nowMin);
        ctx.u8g2.drawStr(6, 11, timeStr);
        drawSdIcon(ctx, 35, 3, snapshot.status.sdStatus);

        ctx.u8g2.setFont(FONT_BOLD_S);
        ctx.u8g2.drawStr(51, 11, "EOLO");
        ctx.u8g2.setFont(FONT_REGULAR_S);

        // Zona derecha: señal módem y energía.
#ifdef FEATURE_DUAL_BATTERY
        int cursorX = 126;
        bool isDC = snapshot.power.poweredByDc;
        uint8_t activeMosfet = snapshot.power.activeBattery;
        bool batteryDataValid = snapshot.power.valid;
        drawBatteryIcon(ctx, cursorX - 9, 3, batteryDataValid ? (int)snapshot.power.batteryPct1 : -1,
                        batteryDataValid && !isDC && activeMosfet == 3);
        drawBatteryIcon(ctx, cursorX - 20, 3, batteryDataValid ? (int)snapshot.power.batteryPct0 : -1,
                        batteryDataValid && !isDC && activeMosfet == 2);
        if (isDC)
            drawDcText(ctx, cursorX - 27, 11);
#ifdef FEATURE_MODEM
        drawModemSignalIcon(ctx,
                            84,
                            3,
                            snapshot.status.modemPowered,
                            snapshot.status.modemActive,
                            snapshot.status.modemError,
                            snapshot.status.modemSignalKnown,
                            snapshot.status.modemSignalBars);
#endif
#else
#ifdef FEATURE_MODEM
        drawModemSignalIcon(ctx,
                            100,
                            3,
                            snapshot.status.modemPowered,
                            snapshot.status.modemActive,
                            snapshot.status.modemError,
                            snapshot.status.modemSignalKnown,
                            snapshot.status.modemSignalBars);
#endif
        drawBatteryIcon(ctx, 116, 3, (int)snapshot.power.batteryPct, false);
#endif
        // Línea separadora 
        ctx.u8g2.drawLine(0, 13, 127, 13);
    }

private:
    static void drawSdIcon(Context &ctx, int x, int y, int status)
    {
        const int w = 13;
        const int h = 9;
        const int bevel = 2;

        ctx.u8g2.drawVLine(x, y, h);
        ctx.u8g2.drawHLine(x, y + h - 1, w);
        ctx.u8g2.drawVLine(x + w - 1, y + bevel, h - bevel);
        ctx.u8g2.drawLine(x + w - 1, y + bevel, x + w - bevel - 1, y);
        ctx.u8g2.drawHLine(x, y, w - bevel - 1);

        if (status == SD_ERROR || status == SD_MISSING)
        {
            ctx.u8g2.drawLine(x + 4, y + 2, x + 8, y + 6);
            ctx.u8g2.drawLine(x + 8, y + 2, x + 4, y + 6);
        }
        else if (status == SD_WRITING)
        {
            ctx.u8g2.drawBox(x + 2, y + 2, w - 4, h - 4);
        }
        else
        {
            ctx.u8g2.setFont(u8g2_font_tiny5_tf);
            ctx.u8g2.drawStr(x + 2, y + 7, "SD");
            ctx.u8g2.setFont(FONT_REGULAR_S);
        }
    }

    static void drawModemSignalIcon(Context &ctx,
                                    int x,
                                    int y,
                                    bool powered,
                                    bool active,
                                    bool error,
                                    bool known,
                                    uint8_t bars)
    {
        const uint8_t maxBars = 4;
        if (bars > maxBars) bars = maxBars;

        ctx.u8g2.drawVLine(x, y + 4, 5);
        ctx.u8g2.drawLine(x, y + 4, x + 3, y + 1);
        ctx.u8g2.drawLine(x, y + 4, x + 3, y + 7);

        if (!powered)
        {
            ctx.u8g2.drawLine(x - 1, y + 8, x + 13, y);
            ctx.u8g2.drawPixel(x + 1, y + 8);
            return;
        }

        if (active)
            ctx.u8g2.drawBox(x + 1, y + 3, 3, 3);
        else
            ctx.u8g2.drawFrame(x + 1, y + 3, 3, 3);

        if (error)
        {
            ctx.u8g2.drawLine(x + 8, y + 2, x + 12, y + 6);
            ctx.u8g2.drawLine(x + 12, y + 2, x + 8, y + 6);
            return;
        }

        for (uint8_t i = 0; i < maxBars; ++i)
        {
            int h = 2 + i * 2;
            int bx = x + 5 + i * 3;
            int by = y + 8 - h;
            if (known && i < bars)
                ctx.u8g2.drawBox(bx, by, 2, h);
            else
                ctx.u8g2.drawFrame(bx, by, 2, h);
        }
    }

    static void drawBatteryIcon(Context &ctx, int x, int y, int pct, bool active)
    {
        bool unknown = pct < 0;
        if (unknown)
        {
            const int w = 8;
            const int h = 9;
            ctx.u8g2.drawFrame(x, y, w, h);
            ctx.u8g2.drawBox(x + w, y + 3, 1, 3);
            ctx.u8g2.drawLine(x + 1, y + 1, x + w - 2, y + h - 2);
            ctx.u8g2.drawLine(x + w - 2, y + 1, x + 1, y + h - 2);
            if (active)
                ctx.u8g2.drawHLine(x, y + h + 1, w);
            return;
        }
        if (pct > 100) pct = 100;

        const int w = 8;
        const int h = 9;
        ctx.u8g2.drawFrame(x, y, w, h);
        ctx.u8g2.drawBox(x + w, y + 3, 1, 3);

        int fillH = map(pct, 0, 100, 0, h - 2);
        if (fillH > 0)
            ctx.u8g2.drawBox(x + 1, y + h - 1 - fillH, w - 2, fillH);

        if (active)
            ctx.u8g2.drawHLine(x, y + h + 1, w);
    }

    static void drawDcText(Context &ctx, int x, int y)
    {
        ctx.u8g2.setFont(u8g2_font_tiny5_tf);
        ctx.u8g2.drawStr(x, y, "DC");
        ctx.u8g2.setFont(FONT_REGULAR_S);
    }
};

#endif
