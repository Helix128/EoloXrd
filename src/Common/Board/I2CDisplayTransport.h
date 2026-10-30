#ifndef EOLO_BOARD_I2C_DISPLAY_TRANSPORT_H
#define EOLO_BOARD_I2C_DISPLAY_TRANSPORT_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "I2CBus.h"

// Transporte de la OLED I2C sobre I2CBus, en lugar del callback HW-I2C de
// U8g2, que fija Wire.setClock() en cada transferencia (el bus es compartido
// con los sensores) e ignora el resultado de endTransmission(). Aquí el panel
// usa el reloj y timeout del perfil y cada NACK queda en las estadísticas del
// bus.
//
// Un frame son decenas de transferencias; el bus se toma por transferencia
// (~3 ms a 100 kHz) y no por frame (>100 ms), que dejaba sin turno al worker de
// sensores. Tras soltarlo se cede un tick: el mutex no es justo, y sin ceder la
// UI lo retoma antes de que el worker del otro core despierte.
namespace I2CDisplayTransport
{
constexpr uint32_t LockTimeoutMs = 20;
// Fallos seguidos que se interpretan como panel ausente o reiniciado.
constexpr uint8_t LostAfterFailures = 3;

// Solo la tarea de UI usa el panel, por lo que el estado no necesita
// protección.
inline bool skipping = false;
inline bool dropReported = false;
// Transferencias descartadas por bus ocupado o fallidas en el panel. Quien
// envía un comando que no admite perderse (encender, contraste) compara el
// contador antes y después.
inline uint32_t dropCount = 0;
inline uint8_t consecutiveFailures = 0;
// Con el panel perdido no se gasta bus en frames: Context lo sondea y lo
// reinicializa cuando vuelve a responder.
inline bool lost = false;
inline uint32_t transferStartMs = 0;

inline void recordFailure()
{
    dropCount++;
    if (consecutiveFailures < 255)
        consecutiveFailures++;
    if (!lost && consecutiveFailures >= LostAfterFailures)
    {
        lost = true;
        LOG_LN("I2C: OLED sin respuesta; se reintentará su inicialización");
    }
}

inline uint8_t byteCallback(u8x8_t *u8x8, uint8_t msg, uint8_t arg, void *ptr)
{
    switch (msg)
    {
    case U8X8_MSG_BYTE_INIT:
        I2CBus::getInstance().begin();
        return 1;
    case U8X8_MSG_BYTE_SET_DC:
        return 1;
    case U8X8_MSG_BYTE_START_TRANSFER:
        if (lost)
        {
            skipping = true;
            return 1;
        }
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
        transferStartMs = millis();
        Wire.beginTransmission(u8x8_GetI2CAddress(u8x8) >> 1);
        return 1;
    case U8X8_MSG_BYTE_SEND:
        if (!skipping)
            Wire.write(static_cast<const uint8_t *>(ptr), arg);
        return 1;
    case U8X8_MSG_BYTE_END_TRANSFER:
    {
        if (skipping)
        {
            skipping = false;
            return 1;
        }
        const uint8_t error = Wire.endTransmission();
        const uint32_t elapsedMs = millis() - transferStartMs;
        I2CBus &bus = I2CBus::getInstance();
        const I2CBus::Result result = I2CBus::resultFromTransmissionError(error, elapsedMs);
        bus.recordResult(u8x8_GetI2CAddress(u8x8) >> 1, result, elapsedMs);
        bus.unlock();
        if (result == I2CBus::Result::Ok)
            consecutiveFailures = 0;
        else
            recordFailure();
        vTaskDelay(1);
        return 1;
    }
    default:
        return 0;
    }
}

// Debe llamarse antes de u8g2.begin(): así la inicialización del panel ya
// pasa por I2CBus.
inline void install(u8x8_t *u8x8)
{
    u8x8->byte_cb = byteCallback;
}

// Vuelve a habilitar los frames tras reinicializar el panel.
inline void markRecovered()
{
    lost = false;
    consecutiveFailures = 0;
}
}

#endif
