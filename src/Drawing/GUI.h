#ifndef GUI_H
#define GUI_H

#include <U8g2lib.h>
#include "../Data/Context.h" 
#include "../Board/Logger.h"

struct AppContext;

// Helper para dibujar elementos comunes en la UI
class GUI
{
public:
    // Header con batería, hora, SD, nombre del dispositivo
    static void displayHeader(Context &ctx)
    {
        ctx.u8g2.setFont(u8g2_font_fivepx_tr);

        // Bateria
        char batteryStr[5];
        snprintf(batteryStr, sizeof(batteryStr), "%d%%", ctx.components.battery.getPct());
        //ctx.u8g2.drawStr(108, 8, batteryStr);

        // Tiempo
        String timeStr = ctx.components.rtc.getTimeString();
        //ctx.u8g2.drawStr(x, y, timeStr.c_str());

        // Nombre del dispositivo
        ctx.u8g2.setFont(u8g2_font_TimesNewPixel_tr);
        ctx.u8g2.drawStr(51, 11, "eolo");
        ctx.u8g2.setFont(u8g2_font_fivepx_tr);

        // Estado de la SD
        char* sdStatus = "OK!";
        if (ctx.components.logger.status == SD_ERROR)
            sdStatus = "ERR!";
        else if (ctx.components.logger.status == SD_WRITING)
            sdStatus = "IO";
        ctx.u8g2.drawStr(98, 9, "SD:");
        ctx.u8g2.drawStr(111, 9, sdStatus);

        // Iconos de baterías 
        ctx.u8g2.drawFrame(89, 1, 6, 11);
        ctx.u8g2.drawFrame(82, 1, 6, 11);

        // Línea separadora 
        ctx.u8g2.drawLine(0, 13, 127, 13);
    }
};

#endif
