#ifndef GUI_H
#define GUI_H

#include <U8g2lib.h>
#include "../Data/Context.h" 


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
        DateTime now = ctx.components.rtc.now();
        int nowSec = now.second();
        int nowMin = now.minute();
        int nowHr = now.hour();
        char timeStr[9];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", nowHr, nowMin, nowSec);
        ctx.u8g2.drawStr(10, 9, timeStr);

        // Nombre del dispositivo
        ctx.u8g2.setFont(u8g2_font_TimesNewPixel_tr);
        ctx.u8g2.drawStr(51, 11, "eolo");
        ctx.u8g2.setFont(u8g2_font_fivepx_tr);

        // Estado de la SD
        const char* sdStatus = "OK!";
      //  if (ctx.components.logger.status == SD_ERROR)
      //      sdStatus = "ERR!";
      //  else if (ctx.components.logger.status == SD_WRITING)
      //      sdStatus = "IO";
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
