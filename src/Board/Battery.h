#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#define BATTERY_PIN 34

// Clase helper para leer el nivel de la batería
class Battery
{
public:

    static const int ADC_MAX = 4095;

    static int getLevel()
    {
        return analogRead(BATTERY_PIN);
    }

    static float getPct()
    {
        int level = getLevel();
        int pct = map(level, 0, ADC_MAX, 0, 100);
        return pct;
    }
};

#endif