#ifndef GUI_H
#define GUI_H

#include <U8g2lib.h>
#include "../Data/Context.h" 
#include "../Config.h"

// Helper para dibujar elementos comunes en la UI
class GUI
{
public:
    // Header con batería, hora, SD, nombre del dispositivo
    static void displayHeader(Context &ctx)
    {
        ctx.u8g2.setFont(FONT_REGULAR_S);

        // Tiempo
        DateTime now = ctx.components.rtc.now();
        int nowMin = now.minute();
        int nowHr = now.hour();
        char timeStr[9];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", nowHr, nowMin);
        ctx.u8g2.drawStr(6, 11, timeStr);

    
        // Estado de la SD - dibujar icono de tarjeta SD
        int iconX = 35;
        int iconY = 3;
        int iconW = 13;
        int iconH = 8;
        int bevelSize = 1;
        
        ctx.u8g2.setFont(u8g2_font_tiny5_tf);        
        if (ctx.sdStatus == SD_ERROR) {
            // Outline with X and bevel
            ctx.u8g2.drawLine(iconX, iconY + bevelSize, iconX, iconY + iconH - 1);
            ctx.u8g2.drawLine(iconX, iconY + iconH - 1, iconX + iconW - 1, iconY + iconH - 1);
            ctx.u8g2.drawLine(iconX + iconW - 1, iconY + iconH - 1, iconX + iconW - 1, iconY + bevelSize);
            ctx.u8g2.drawLine(iconX + iconW - 1, iconY + bevelSize, iconX + iconW - 1 - bevelSize, iconY);
            ctx.u8g2.drawLine(iconX + iconW - 1 - bevelSize, iconY, iconX, iconY);
            // Centered X inside the SD icon
            ctx.u8g2.drawLine(iconX + 4, iconY + 1, iconX + 8, iconY + 6);
            ctx.u8g2.drawLine(iconX + 8, iconY + 1, iconX + 4, iconY + 6);
        } else if (ctx.sdStatus == SD_WRITING) {
            // Filled with bevel cutout
            ctx.u8g2.drawBox(iconX, iconY + bevelSize, iconW, iconH - bevelSize);
            ctx.u8g2.drawBox(iconX, iconY, iconW - bevelSize, bevelSize);
        } else {
            // OK: Outline with bevel
            ctx.u8g2.drawLine(iconX, iconY + bevelSize, iconX, iconY + iconH - 1);
            ctx.u8g2.drawLine(iconX, iconY + iconH - 1, iconX + iconW - 1, iconY + iconH - 1);
            ctx.u8g2.drawLine(iconX + iconW - 1, iconY + iconH - 1, iconX + iconW - 1, iconY + bevelSize);
            ctx.u8g2.drawLine(iconX + iconW - 1, iconY + bevelSize, iconX + iconW - 1 - bevelSize, iconY);
            ctx.u8g2.drawLine(iconX + iconW - 1 - bevelSize, iconY, iconX, iconY);
            ctx.u8g2.drawStr(iconX + 2, iconY + 7, "OK");
        }

        ctx.u8g2.setFont(FONT_BOLD_S);
        ctx.u8g2.drawStr(51, 11, "EOLO");
        ctx.u8g2.setFont(FONT_REGULAR_S);



        // Batería única (altura combinada de las dos anteriores)
        ctx.u8g2.drawRFrame(81, 3, 13, 9, 1); // x, y, w, h (h = 9 para cubrir desde y=3 hasta y=11)
        ctx.u8g2.drawVLine(81,3,9);

        #if BAREBONES == true
        int batteryPct = (millis() / 100) % 101; // Simulación de batería para testing
        #else
        int batteryPct = ctx.components.battery.getPct();
        #endif

        // Relleno vertical dentro del marco (interior height = 9 - 2 = 7)
        {
            int pct = batteryPct;
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            int interiorX = 81 + 1; // 82
            int interiorY = 3 + 1;  // 4
            int interiorWidth = 13 - 2; // 11
            int interiorHeight = 9 - 2; // 7
            int fillWidth = map(pct, 0, 100, 0, interiorWidth);
            if (fillWidth > 0) {
                // Rellenar horizontalmente de izquierda a derecha
                ctx.u8g2.drawBox(interiorX, interiorY, fillWidth, interiorHeight);
            }
        }

        // (Opcional) pequeño terminal de batería a la derecha
        ctx.u8g2.drawBox(95, 6, 1, 3);
        String batteryStr = String(batteryPct) + "%";
        ctx.u8g2.drawStr(97, 11, batteryStr.c_str());
        // Línea separadora 
        ctx.u8g2.drawLine(0, 13, 127, 13);
    }
};

#endif
