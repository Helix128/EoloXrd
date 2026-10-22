#ifndef EOLO_UI_DRAWING_UNITS_H
#define EOLO_UI_DRAWING_UNITS_H

#include <stddef.h>
#include <stdio.h>

// Unidades en Latin-1 (las fuentes u8g2 *_tf lo soportan con drawStr).
// Tras un escape \xNN se corta el literal si sigue un digito hexadecimal.
#define UNIT_FLOW "L/min"
#define UNIT_VOLUME "m\xB3"
#define UNIT_PM "\xB5g/m\xB3"
#define UNIT_TEMP "\xB0" "C"
#define UNIT_PCT "%"
#define UNIT_PRESSURE "hPa"
#define UNIT_WIND "m/s"
#define UNIT_DEG "\xB0"

// "HH:MM:SS"
inline void formatHms(char *out, size_t size, unsigned long seconds)
{
    snprintf(out, size, "%02lu:%02lu:%02lu", seconds / 3600UL, (seconds % 3600UL) / 60UL, seconds % 60UL);
}

// "HH:MM"
inline void formatHm(char *out, size_t size, unsigned long seconds)
{
    snprintf(out, size, "%02lu:%02lu", seconds / 3600UL, (seconds % 3600UL) / 60UL);
}

#endif
