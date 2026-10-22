// Se incluye antes que cualquier codigo de la UI (-include prelude.h).
// Reemplaza Arduino/U8g2 hardware/Context por stubs para renderizar en el PC.
#ifndef PREVIEW_PRELUDE_H
#define PREVIEW_PRELUDE_H

#include "Arduino.h"

// Evita cargar los perfiles/Legacy reales.
#define EOLO_CONFIG_LEGACY_H
#define EOLO_CONFIG_ACTIVE_PROFILE_H
#define LOG_OUT(x) do {} while (0)
#define LOG_OUT_LN(x) do {} while (0)
#define LOG_LN(x) do {} while (0)
#define LOG_F(...) do {} while (0)

#include <U8g2lib.h>

// Pantalla "nula": renderiza al buffer de u8g2 sin hablar con hardware.
struct PreviewDisplay : public U8G2
{
    PreviewDisplay()
    {
        u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_empty, u8x8_dummy_cb);
    }
};
#define EOLO_DISPLAY_MODEL PreviewDisplay

#endif
