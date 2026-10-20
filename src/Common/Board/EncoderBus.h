#ifndef EOLO_BOARD_ENCODER_BUS_H
#define EOLO_BOARD_ENCODER_BUS_H

#include <Arduino.h>
#include "../../Variants/Legacy.h"
#include "I2CBus.h"

#if ENCODER_SDA_PIN >= 0
#include <Wire.h>
#endif

// Transporte del encoder (ATmega/ATtiny esclavo). Los modelos con ENCODER_SDA_PIN
// lo tienen en un bus propio (Wire1, lento) y los demás comparten el I2CBus
// principal. Solo el worker I2C accede al encoder, por lo que Wire1 no necesita
// mutex ni estadísticas.
class EncoderBus
{
public:
    static bool writeCommand(uint8_t addr, uint8_t command)
    {
#if ENCODER_SDA_PIN >= 0
        if (!begin())
            return false;
        Wire1.beginTransmission(addr);
        Wire1.write(command);
        return Wire1.endTransmission() == 0;
#else
        return I2CBus::getInstance().writeCommand(addr, command, false);
#endif
    }

    static bool readBytes(uint8_t addr, uint8_t *buffer, size_t len)
    {
#if ENCODER_SDA_PIN >= 0
        if (!begin())
            return false;
        if (Wire1.requestFrom(addr, len) != len)
        {
            while (Wire1.available())
                Wire1.read();
            return false;
        }
        for (size_t i = 0; i < len; ++i)
            buffer[i] = Wire1.read();
        return true;
#else
        return I2CBus::getInstance().readBytes(addr, buffer, len, false);
#endif
    }

#if ENCODER_SDA_PIN >= 0
private:
    static bool begin()
    {
        static bool ready = false;
        if (!ready)
        {
            ready = Wire1.begin(ENCODER_SDA_PIN, ENCODER_SCL_PIN);
            if (ready)
            {
                Wire1.setClock(EoloConfig::encoderBusClockHz);
                Wire1.setTimeOut(EoloConfig::i2cTransactionTimeoutMs);
                LOG_F("I2C encoder iniciado: SDA=%d, SCL=%d, clock=%lu Hz\n",
                      ENCODER_SDA_PIN, ENCODER_SCL_PIN,
                      (unsigned long)EoloConfig::encoderBusClockHz);
            }
        }
        return ready;
    }
#endif
};

#endif
