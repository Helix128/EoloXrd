#ifndef GUI_H
#define GUI_H

#include "Fonts.h"
#include <U8g2lib.h>
#include "../../Common/Data/Context.h"
#include "../../Variants/Legacy.h"
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

        ctx.u8g2.setFont(FONT_VALUE);
        ctx.u8g2.drawStr(Layout::Header::Time.x,
                          Layout::Header::Time.y + 9, timeStr);
        drawSdIcon(ctx, Layout::Header::Sd.x, Layout::Header::Sd.y,
                   snapshot.status.sdStatus);
#ifdef FEATURE_DUAL_BATTERY
        const bool isDC = snapshot.power.poweredByDc;
        const uint8_t activeMosfet = snapshot.power.activeBattery;
        const bool batteryDataValid = snapshot.power.valid;

        drawBattery(ctx, Layout::Header::Battery0,
                    batteryDataValid ? static_cast<int>(snapshot.power.batteryPct0) : -1,
                    batteryDataValid && !isDC && activeMosfet == 2, false);
        drawBattery(ctx, Layout::Header::Battery1,
                    batteryDataValid ? static_cast<int>(snapshot.power.batteryPct1) : -1,
                    batteryDataValid && !isDC && activeMosfet == 3, false);
        if (isDC)
            drawDcText(ctx, Layout::Header::Dc.x, Layout::Header::Dc.y);

#ifdef FEATURE_MODEM
        drawModemSignalIcon(ctx, Layout::Header::Modem.x, Layout::Header::Modem.y,
                            snapshot.status.modemPowered, snapshot.status.modemActive,
                            snapshot.status.modemError, snapshot.status.modemSignalKnown,
                            snapshot.status.modemSignalBars);
#endif
#else
        ctx.u8g2.setFont(FONT_LABEL);
        constexpr const char *brand = "EOLO";
        ctx.u8g2.drawStr(Layout::CenterX - ctx.u8g2.getStrWidth(brand) / 2,
                          Layout::Header::Brand.y + 9, brand);
#ifdef FEATURE_MODEM
        drawModemSignalIcon(ctx, Layout::Header::Modem.x, Layout::Header::Modem.y,
                            snapshot.status.modemPowered, snapshot.status.modemActive,
                            snapshot.status.modemError, snapshot.status.modemSignalKnown,
                            snapshot.status.modemSignalBars);
#endif
        drawBattery(ctx, Layout::Header::Battery,
                    static_cast<int>(snapshot.power.batteryPct), false, true);
#endif

        ctx.u8g2.drawLine(0, Layout::HeaderBottom, Layout::ScreenW - 1,
                           Layout::HeaderBottom);
        // Conserva la fuente que esperaban las escenas despues del header.
        ctx.u8g2.setFont(FONT_VALUE);
    }

private:
    // Tarjeta SD 10x9: silueta con esquina cortada. Contactos = lista, caja = escribiendo,
    // "!" = error, X = sin tarjeta.
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

        if (status == SD_MISSING)
        {
            ctx.u8g2.drawLine(x + 3, y + 2, x + 6, y + 6);
            ctx.u8g2.drawLine(x + 6, y + 2, x + 3, y + 6);
        }
        else if (status == SD_ERROR)
        {
            ctx.u8g2.drawVLine(x + 4, y + 2, 3);
            ctx.u8g2.drawPixel(x + 4, y + 6);
        }
        else if (status == SD_WRITING)
        {
            ctx.u8g2.drawBox(x + 2, y + 3, w - 5, 4);
        }
        else
        {
            ctx.u8g2.drawVLine(x + 2, y + 1, 3);
            ctx.u8g2.drawVLine(x + 4, y + 1, 3);
            ctx.u8g2.drawVLine(x + 6, y + 1, 3);
        }
    }

    // Senal celular 16x9: cuatro barras crecientes (llenas = nivel, rayitas = vacias),
    // cuadro = actividad, X = error, tachado = modem apagado.
    static void drawModemSignalIcon(Context &ctx, int x, int y, bool powered,
                                    bool active, bool error, bool known, uint8_t bars)
    {
        constexpr uint8_t maxBars = 4;
        if (bars > maxBars) bars = maxBars;
        const bool showLevel = powered && !error && known;

        for (uint8_t i = 0; i < maxBars; ++i)
        {
            const int h = 2 + i * 2;
            const int bx = x + i * 3;
            if (showLevel && i < bars)
                ctx.u8g2.drawBox(bx, y + 8 - h + 1, 2, h);
            else
                ctx.u8g2.drawHLine(bx, y + 8, 2);
        }

        if (!powered)
        {
            ctx.u8g2.drawLine(x, y + 8, x + 10, y);
        }
        else if (error)
        {
            ctx.u8g2.drawLine(x + 12, y + 2, x + 15, y + 5);
            ctx.u8g2.drawLine(x + 15, y + 2, x + 12, y + 5);
        }
        else if (active)
        {
            ctx.u8g2.drawBox(x + 13, y + 3, 3, 3);
        }
    }

    static constexpr int BatteryLowPct = 15;

    // Bateria: icono vertical 5x9 (borne arriba, se llena de abajo hacia
    // arriba) + porcentaje. Sin dato = "--". Baja = "!" dentro del icono.
    // Activa = subrayado bajo icono y numero.
    static void drawBattery(Context &ctx, const Layout::Header::Slot &slot, int pct,
                            bool active, bool percentSign)
    {
        constexpr int iconW = 5;
        constexpr int iconH = 9;
        constexpr int fillH = iconH - 3;
        const bool unknown = pct < 0;
        if (pct > 100) pct = 100;

        char text[5];
        if (unknown)
            snprintf(text, sizeof(text), "--");
        else
            snprintf(text, sizeof(text), percentSign ? "%d%%" : "%d", pct);

        ctx.u8g2.setFont(FONT_VALUE);
        const int x = slot.x + 1;
        const int y = slot.y + 1;
        const int textX = x + iconW + 2;
        const int contentW = textX + ctx.u8g2.getStrWidth(text) - x;
        ctx.u8g2.drawHLine(x + 1, y, iconW - 2);
        ctx.u8g2.drawFrame(x, y + 1, iconW, iconH - 1);
        if (!unknown && pct <= BatteryLowPct)
        {
            ctx.u8g2.drawVLine(x + 2, y + 2, 3);
            ctx.u8g2.drawPixel(x + 2, y + 6);
        }
        else if (!unknown)
        {
            int fill = (pct * fillH + 50) / 100;
            if (pct > 0 && fill < 1) fill = 1;
            if (fill > 0)
                ctx.u8g2.drawBox(x + 1, y + 2 + fillH - fill, iconW - 2, fill);
        }

        ctx.u8g2.drawStr(textX, y + 8, text);

        if (active)
            ctx.u8g2.drawHLine(slot.x, Layout::Header::ActiveSourceY, contentW + 2);
    }

    // Rayo (alimentacion externa) en 6x9.
    static void drawDcText(Context &ctx, int x, int y)
    {
        ctx.u8g2.drawLine(x + 5, y, x + 1, y + 5);
        ctx.u8g2.drawHLine(x + 1, y + 5, 5);
        ctx.u8g2.drawLine(x + 5, y + 5, x + 2, y + 8);
        ctx.u8g2.drawHLine(x, Layout::Header::ActiveSourceY, 8);
    }
};

#endif
