#ifndef EOLO_BOARD_I2C_DISPLAY_TRANSPORT_H
#define EOLO_BOARD_I2C_DISPLAY_TRANSPORT_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "I2CBus.h"

// Transporte de la OLED I2C sobre I2CBus. Un frame son decenas de
// transferencias de U8g2; el bus se toma por transferencia (~3 ms a 100 kHz) y
// no por frame (>100 ms), que dejaba sin turno al worker de sensores. Tras
// soltarlo se cede un tick: el mutex no es justo, y sin ceder la UI lo retoma
// antes de que el worker del otro core despierte.
namespace I2CDisplayTransport
{
constexpr uint32_t LockTimeoutMs = 20;

// Solo dibuja la tarea de UI, por lo que el estado no necesita protección.
inline bool skipping = false;
inline bool dropReported = false;
// Transferencias descartadas por bus ocupado. Quien envia un comando que no
// admite perderse (encender, contraste) compara el contador antes y despues.
inline uint32_t dropCount = 0;

inline uint8_t byteCallback(u8x8_t *u8x8, uint8_t msg, uint8_t arg, void *ptr)
{
    switch (msg)
    {
    case U8X8_MSG_BYTE_START_TRANSFER:
        if (!I2CBus::getInstance().lock(LockTimeoutMs))
        {
            // Se descarta la transferencia; el siguiente frame repinta todo.
            skipping = true;
            dropCount++;
            if (!dropReported)
            {
                dropReported = true;
                LOG_LN("I2C: transferencia de OLED descartada por bus ocupado");
            }
            return 1;
        }
        skipping = false;
        return u8x8_byte_arduino_hw_i2c(u8x8, msg, arg, ptr);
    case U8X8_MSG_BYTE_SEND:
        return skipping ? 1 : u8x8_byte_arduino_hw_i2c(u8x8, msg, arg, ptr);
    case U8X8_MSG_BYTE_END_TRANSFER:
    {
        if (skipping)
        {
            skipping = false;
            return 1;
        }
        const uint8_t result = u8x8_byte_arduino_hw_i2c(u8x8, msg, arg, ptr);
        I2CBus::getInstance().unlock();
        vTaskDelay(1);
        return result;
    }
    default:
        return u8x8_byte_arduino_hw_i2c(u8x8, msg, arg, ptr);
    }
}

// Debe llamarse después de u8g2.begin(): begin() ejecuta Wire.begin() por el
// callback original.
inline void install(u8x8_t *u8x8)
{
    u8x8->byte_cb = byteCallback;
}
}

#endif
