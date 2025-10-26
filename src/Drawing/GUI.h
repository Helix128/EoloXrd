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
        ctx.u8g2.setFont(u8g2_font_helvB08_tf);

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
        ctx.u8g2.drawStr(6, 11, timeStr);

        ctx.u8g2.drawStr(51, 11, "EOLO");
        // Iconos de baterías 
        ctx.u8g2.drawFrame(80, 1, 6, 11);
        ctx.u8g2.drawFrame(87, 1, 6, 11);

        # if BAREBONES == true
        int batteryPct = (millis() / 50) % 101; // Simulación de batería para testing
        # else
        int batteryPct = ctx.components.battery.getPct();
        #endif
        if(batteryPct<=50){
            ctx.u8g2.drawBox(81,1,5,map(batteryPct,0,50,0,11));
        }
        else{
            ctx.u8g2.drawBox(81,1,5,11);
            ctx.u8g2.drawBox(88,1,5,map(batteryPct,50,100,0,11));
        }
        // Estado de la SD
        const char* sdStatus = "OK";
        if (ctx.sdStatus == SD_ERROR)
            sdStatus = "ERR";
        else if (ctx.sdStatus == SD_WRITING)
            sdStatus = "IO";
        ctx.u8g2.drawStr(95, 11, "SD ");
        ctx.u8g2.drawStr(111, 11, sdStatus);
        
        // Línea separadora 
        ctx.u8g2.drawLine(0, 13, 127, 13);
    }
};

#endif
