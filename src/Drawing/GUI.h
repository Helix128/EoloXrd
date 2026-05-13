#ifndef GUI_H
#define GUI_H

#include <U8g2lib.h>
#include "../Data/Context.h" 
#include "../Config.h"
#include "Profiler.h" 
// Helper para dibujar elementos comunes en la UI
class GUI
{
public:
    // Header con batería, hora, SD, nombre del dispositivo
    static void displayHeader(Context &ctx)
    {   
        //Profiler p("GUI displayHeader");
        ctx.u8g2.setFont(FONT_REGULAR_S);

        // Tiempo
        const UiSnapshot &snapshot = ctx.getUiSnapshot();
        int nowMin = snapshot.status.minute;
        int nowHr = snapshot.status.hour;
        char timeStr[9];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", nowHr, nowMin);
        ctx.u8g2.drawStr(6, 11, timeStr);

    
        // Estado de la SD - dibujar icono de tarjeta SDs
        int iconX = 35;
        int iconY = 3;
        int iconW = 13;
        int iconH = 8;
        int bevelSize = 1;
        
        ctx.u8g2.setFont(u8g2_font_tiny5_tf);        
        if (snapshot.status.sdStatus == SD_ERROR) {
            // Outline with X and bevel
            ctx.u8g2.drawLine(iconX, iconY + bevelSize, iconX, iconY + iconH - 1);
            ctx.u8g2.drawLine(iconX, iconY + iconH - 1, iconX + iconW - 1, iconY + iconH - 1);
            ctx.u8g2.drawLine(iconX + iconW - 1, iconY + iconH - 1, iconX + iconW - 1, iconY + bevelSize);
            ctx.u8g2.drawLine(iconX + iconW - 1, iconY + bevelSize, iconX + iconW - 1 - bevelSize, iconY);
            ctx.u8g2.drawLine(iconX + iconW - 1 - bevelSize, iconY, iconX, iconY);
            // Centered X inside the SD icon
            ctx.u8g2.drawLine(iconX + 4, iconY + 1, iconX + 8, iconY + 6);
            ctx.u8g2.drawLine(iconX + 8, iconY + 1, iconX + 4, iconY + 6);
        } else if (snapshot.status.sdStatus == SD_WRITING) {
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

        // Batería (soporte para FEATURE_DUAL_BATTERY con 2 baterías y DC)
#ifdef FEATURE_DUAL_BATTERY
        ctx.u8g2.setFont(u8g2_font_tiny5_tf);
        
        int cursorX = 126; // Empezamos desde la derecha
        bool isDC = snapshot.power.poweredByDc;
        uint8_t activeMosfet = snapshot.power.activeBattery;

        // Indicador DC (ajustado a la derecha)
        if (isDC) {
            const char* dcStr = "DC";
            int w = ctx.u8g2.getStrWidth(dcStr);
            cursorX -= w;
            ctx.u8g2.drawStr(cursorX, 11, dcStr);
            cursorX -= 3; // Espaciador
        }

        // Dos iconos de batería
        // Renderizado en bucle inverso (Bat 1 a la derecha, Bat 0 a la izquierda de esta)
        for (int i = 1; i >= 0; i--) {
            int pct = (int)(i == 0 ? snapshot.power.batteryPct0 : snapshot.power.batteryPct1);
            if (pct < 0) pct = 0; if (pct > 100) pct = 100;

            // marcos
            int batW = 6; 
            int batH = 8;
            int batY = 4; // y=4 a y=11
            cursorX -= batW; // Reservar espacio para icono

            // pequeño terminal a la derecha (ajustado: mitad superior) -> Ahora arriba
            ctx.u8g2.drawBox(cursorX + 2, batY - 1, 2, 1);
            ctx.u8g2.drawFrame(cursorX, batY, batW, batH);

            // Porcentajes y relleno
            int fillH = map(pct, 0, 100, 0, batH - 2);
            if (fillH > 0) {
                ctx.u8g2.drawBox(cursorX + 1, batY + (batH - 1) - fillH, batW - 2, fillH);
            }

            // Indicar batería activa si no es DC
            if (!isDC && activeMosfet == (i + 1)) {
                ctx.u8g2.drawHLine(cursorX, batY + batH + 1, batW); // Subrayado activo
            }

            // Etiquetas de porcentaje centradas dentro de cada batería
            // (Ahora a la izquierda del icono para ahorrar verticalidad)
            char buf[5];
            snprintf(buf, sizeof(buf), "%d", pct);
            int textW = ctx.u8g2.getStrWidth(buf);
            ctx.u8g2.drawStr(cursorX - textW - 1, 11, buf);
            
            cursorX -= (textW + 3); // Mover cursor para la siguiente batería
        }

#else
        // Batería única (altura combinada de las dos anteriores)
        ctx.u8g2.drawRFrame(81, 3, 13, 9, 1); // x, y, w, h (h = 9 para cubrir desde y=3 hasta y=11)
        ctx.u8g2.drawVLine(81,3,9);

        int batteryPct = (int)snapshot.power.batteryPct;

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
#endif
        // Línea separadora 
        ctx.u8g2.drawLine(0, 13, 127, 13);
    }
};

#endif
