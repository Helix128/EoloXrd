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
        ctx.u8g2.setFont(u8g2_font_helvR08_tf);

        // Tiempo
        DateTime now = ctx.components.rtc.now();
        int nowMin = now.minute();
        int nowHr = now.hour();
        char timeStr[9];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", nowHr, nowMin);
        ctx.u8g2.drawStr(6, 11, timeStr);

    
        // Estado de la SD - dibujar icono de tarjeta SD
        int iconX = 38;
        int iconY = 3;
        int iconW = 8;
        int iconH = 8;
        int bevelSize = 1;
        
        if (ctx.sdStatus == SD_ERROR) {
            // Outline with X and bevel
            ctx.u8g2.drawLine(iconX, iconY + bevelSize, iconX, iconY + iconH - 1);
            ctx.u8g2.drawLine(iconX, iconY + iconH - 1, iconX + iconW - 1, iconY + iconH - 1);
            ctx.u8g2.drawLine(iconX + iconW - 1, iconY + iconH - 1, iconX + iconW - 1, iconY + bevelSize);
            ctx.u8g2.drawLine(iconX + iconW - 1, iconY + bevelSize, iconX + iconW - 1 - bevelSize, iconY);
            ctx.u8g2.drawLine(iconX + iconW - 1 - bevelSize, iconY, iconX, iconY);
            // X
            ctx.u8g2.drawLine(iconX + 1, iconY + bevelSize + 1, iconX + iconW - 2, iconY + iconH - 2);
            ctx.u8g2.drawLine(iconX + iconW - 2, iconY + bevelSize + 1, iconX + 1, iconY + iconH - 2);
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
        }

        ctx.u8g2.setFont(u8g2_font_helvB08_tf);
        ctx.u8g2.drawStr(51, 11, "EOLO");
        ctx.u8g2.setFont(u8g2_font_helvR08_tf);
        // Iconos de baterías 
        ctx.u8g2.drawRFrame(82, 3, 13, 4, 1);
        ctx.u8g2.drawRFrame(82, 8, 13, 4, 1); 
        ctx.u8g2.drawLine(82,3,82,6);
        ctx.u8g2.drawLine(82,8,82,11);


        #if BAREBONES == true
        int batteryPct = (millis() / 100) % 101; // Simulación de batería para testing
        #else
        int batteryPct = ctx.components.battery.getPct();
        #endif
        
        if (batteryPct > 0) {
            if(batteryPct<50){
            int fillWidth = map(batteryPct, 0, 50, 0, 11);
            ctx.u8g2.drawBox(83, 4, fillWidth, 2);
            }
            else{
            ctx.u8g2.drawBox(83, 4, 11, 2);
            int fillWidth = map(batteryPct, 50, 100, 0, 11);
            ctx.u8g2.drawBox(83, 9, fillWidth, 2);
            }
        }
        String batteryStr = String(batteryPct) + "%";
        ctx.u8g2.drawStr(97, 11, batteryStr.c_str());
        // Línea separadora 
        ctx.u8g2.drawLine(0, 13, 127, 13);
    }
};

#endif
