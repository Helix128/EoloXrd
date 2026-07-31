#ifndef GUI_H
#define GUI_H

#include <U8g2lib.h>
#include "../Data/Context.h"
#include "../Config/Legacy.h"
#include "Layout.h"
#include "Profiler.h"

// Elementos comunes de la UI.
class GUI
{
public:
    // Header comun: contenido y=0..12, separador en y=13.
    static void displayHeader(Context &ctx)
    {
        const UiSnapshot &snapshot = ctx.getUiSnapshot();

        int nowMin = snapshot.status.minute;
        int nowHr = snapshot.status.hour;
        char timeStr[9];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", nowHr, nowMin);

        // La hora usa 5x7; la marca usa una fuente 4x6 compacta con métricas reales.
        ctx.u8g2.setFont(u8g2_font_5x7_tf);
        ctx.u8g2.drawStr(Layout::Header::Time.x,
                          Layout::Header::Time.y + 7, timeStr);
        drawSdIcon(ctx, Layout::Header::Sd.x, Layout::Header::Sd.y,
                   snapshot.status.sdStatus);
        ctx.u8g2.setFont(u8g2_font_4x6_tf);
        constexpr const char *brand = "EOLO";
        const int brandWidth = ctx.u8g2.getStrWidth(brand);
        const int brandX = Layout::Header::Brand.x +
                           (Layout::Header::Brand.w - brandWidth) / 2;
        ctx.u8g2.drawStr(brandX+4, Layout::Header::Brand.y + 8, brand);

#ifdef FEATURE_DUAL_BATTERY
        const bool isDC = snapshot.power.poweredByDc;
        const uint8_t activeMosfet = snapshot.power.activeBattery;
        const bool batteryDataValid = snapshot.power.valid;

        drawBatteryIcon(ctx, Layout::Header::Battery0.x, Layout::Header::Battery0.y,
                        batteryDataValid ? static_cast<int>(snapshot.power.batteryPct0) : -1,
                        batteryDataValid && !isDC && activeMosfet == 2);
        drawBatteryIcon(ctx, Layout::Header::Battery1.x, Layout::Header::Battery1.y,
                        batteryDataValid ? static_cast<int>(snapshot.power.batteryPct1) : -1,
                        batteryDataValid && !isDC && activeMosfet == 3);
        if (isDC)
            drawDcText(ctx, Layout::Header::Dc.x, Layout::Header::Dc.y);

#ifdef FEATURE_MODEM
        drawModemSignalIcon(ctx, Layout::Header::Modem.x, Layout::Header::Modem.y,
                            snapshot.status.modemPowered, snapshot.status.modemActive,
                            snapshot.status.modemError, snapshot.status.modemSignalKnown,
                            snapshot.status.modemSignalBars);
#endif
#else
#ifdef FEATURE_MODEM
        drawModemSignalIcon(ctx, Layout::Header::Modem.x, Layout::Header::Modem.y,
                            snapshot.status.modemPowered, snapshot.status.modemActive,
                            snapshot.status.modemError, snapshot.status.modemSignalKnown,
                            snapshot.status.modemSignalBars);
#endif
        drawBatteryIcon(ctx, Layout::Header::Battery1.x, Layout::Header::Battery1.y,
                        static_cast<int>(snapshot.power.batteryPct), false);
#endif

        ctx.u8g2.drawLine(0, Layout::HeaderBottom, Layout::ScreenW - 1,
                           Layout::HeaderBottom);
        // Conserva la fuente que esperaban las escenas despues del header.
        ctx.u8g2.setFont(FONT_REGULAR_S);
    }

private:
    static void drawSdIcon(Context &ctx, int x, int y, int status)
    {
        constexpr int w = 10;
        constexpr int h = 9;
        constexpr int bevel = 2;

        ctx.u8g2.drawVLine(x, y, h);
        ctx.u8g2.drawHLine(x, y + h - 1, w);
        ctx.u8g2.drawVLine(x + w - 1, y + bevel, h - bevel);
        ctx.u8g2.drawLine(x + w - 1, y + bevel, x + w - bevel - 1, y);
        ctx.u8g2.drawHLine(x, y, w - bevel - 1);

        if (status == SD_ERROR || status == SD_MISSING)
        {
            ctx.u8g2.drawLine(x + 3, y + 2, x + 7, y + 6);
            ctx.u8g2.drawLine(x + 7, y + 2, x + 3, y + 6);
        }
        else if (status == SD_WRITING)
        {
            ctx.u8g2.drawBox(x + 2, y + 2, w - 4, h - 4);
        }
        else
        {
            ctx.u8g2.setFont(u8g2_font_tiny5_tf);
            ctx.u8g2.drawStr(x + 1, y + 7, "SD");
        }
    }

    // Antena, estado de actividad y hasta cuatro barras en 16x9 px.
    static void drawModemSignalIcon(Context &ctx, int x, int y, bool powered,
                                    bool active, bool error, bool known, uint8_t bars)
    {
        constexpr uint8_t maxBars = 4;
        if (bars > maxBars) bars = maxBars;

        ctx.u8g2.drawVLine(x, y + 4, 5);
        ctx.u8g2.drawLine(x, y + 4, x + 3, y + 1);
        ctx.u8g2.drawLine(x, y + 4, x + 3, y + 7);

        if (!powered)
        {
            ctx.u8g2.drawLine(x, y + 8, x + 15, y);
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
            const int h = 2 + i * 2;
            const int bx = x + 5 + i * 3;
            const int by = y + 8 - h;
            if (known && i < bars)
                ctx.u8g2.drawBox(bx, by, 2, h);
            else
                ctx.u8g2.drawFrame(bx, by, 2, h);
        }
    }

    // El frame ocupa 8x9 y el terminal completa el slot de 9x9.
    static void drawBatteryIcon(Context &ctx, int x, int y, int pct, bool active)
    {
        constexpr int frameW = 8;
        constexpr int h = 9;
        const bool unknown = pct < 0;

        ctx.u8g2.drawFrame(x, y, frameW, h);
        ctx.u8g2.drawBox(x + frameW, y + 3, 1, 3);

        if (unknown)
        {
            ctx.u8g2.drawLine(x + 1, y + 1, x + frameW - 2, y + h - 2);
            ctx.u8g2.drawLine(x + frameW - 2, y + 1, x + 1, y + h - 2);
        }
        else
        {
            if (pct > 100) pct = 100;
            const int fillH = map(pct, 0, 100, 0, h - 2);
            if (fillH > 0)
                ctx.u8g2.drawBox(x + 1, y + h - 1 - fillH, frameW - 2, fillH);
        }

        if (active)
            ctx.u8g2.drawHLine(x, Layout::Header::ActiveSourceY, frameW);
    }

    static void drawDcText(Context &ctx, int x, int y)
    {
        ctx.u8g2.setFont(u8g2_font_tiny5_tf);
        ctx.u8g2.drawStr(x, y + 7, "DC");
        ctx.u8g2.drawHLine(x, Layout::Header::ActiveSourceY, 8);
    }
};

#endif
