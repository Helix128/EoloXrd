#ifndef EOLO_CORE_SENSORS_FS3000_FLOW_MODEL_H
#define EOLO_CORE_SENSORS_FS3000_FLOW_MODEL_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

class FS3000FlowModel
{
public:
    static constexpr uint8_t I2cAddress = 0x28;
    static constexpr size_t FrameSize = 5;

    // Trama: [0] checksum, [1..2] dato de 12 bits, [3..4] relleno de checksum.
    // Falla si la suma de los 5 bytes no es 0 mod 256 o si el dato es 0: el
    // reposo del sensor es ~409, así que 0 indica bus sin respuesta real.
    static bool parseFrame(const uint8_t (&frame)[FrameSize], uint16_t &raw)
    {
        uint8_t sum = 0;
        for (size_t i = 0; i < FrameSize; i++)
            sum = static_cast<uint8_t>(sum + frame[i]);
        if (sum != 0)
            return false;
        raw = static_cast<uint16_t>(((frame[1] & 0x0F) << 8) | frame[2]);
        return raw != 0;
    }

    // Curva FS3000-1015 (0-15 m/s) del datasheet. Raw <= 409 es "sin flujo".
    static float velocityFromRaw(uint16_t raw)
    {
        static constexpr uint16_t rawPoints[] = {409, 1203, 1597, 1908, 2187, 2400, 2629, 2801, 3006, 3178, 3309, 3563, 3686};
        static constexpr float mpsPoints[] = {0, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 13.0f, 15.0f};
        static constexpr int numPoints = 13;

        if (raw <= rawPoints[0])
            return 0.0f;
        if (raw >= rawPoints[numPoints - 1])
            return mpsPoints[numPoints - 1];
        int i = 0;
        while (raw >= rawPoints[i + 1])
            i++;
        return interpolate(raw, rawPoints[i], rawPoints[i + 1], mpsPoints[i], mpsPoints[i + 1]);
    }

    // Velocidad 0 es "sin flujo" válido (0 L/min); solo un valor no finito o
    // negativo es inválido (-1).
    static float flowFromVelocity(float speed)
    {
        if (!isfinite(speed) || speed < 0.0f)
            return -1.0f;

        static constexpr float velocityPoints[] = {0.06f, 0.34f, 0.7f, 1.06f, 1.42f, 1.74f, 2.02f, 2.30f, 2.60f, 2.80f, 3.00f, 3.33f};
        static constexpr float flowPoints[] = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f};
        static constexpr int numPoints = 12;

        if (speed < velocityPoints[0])
            return 0.0f;

        for (int i = 0; i < numPoints - 1; i++)
        {
            if (speed < velocityPoints[i + 1])
                return interpolate(speed, velocityPoints[i], velocityPoints[i + 1], flowPoints[i], flowPoints[i + 1]);
        }

        return interpolate(speed,
                           velocityPoints[numPoints - 2],
                           velocityPoints[numPoints - 1],
                           flowPoints[numPoints - 2],
                           flowPoints[numPoints - 1]);
    }

private:
    static float interpolate(float x, float x0, float x1, float y0, float y1)
    {
        float slope = (y1 - y0) / (x1 - x0);
        return y0 + slope * (x - x0);
    }
};

#endif // EOLO_CORE_SENSORS_FS3000_FLOW_MODEL_H
