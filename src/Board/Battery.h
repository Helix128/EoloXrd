#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#define BATTERY_PIN 34

// Clase helper para leer el nivel de la batería
class Battery
{
public:
    static int getLevel()
    {
        return analogRead(BATTERY_PIN);
    }

    static float getPct()
    {
        int level = getLevel();
        int pct = map(level, 0, 1023, 0, 100);
        pct = constrain(pct, 0, 100);
        return pct;
    }
};

#endif