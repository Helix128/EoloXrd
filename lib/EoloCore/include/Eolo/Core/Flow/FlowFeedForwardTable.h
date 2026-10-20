#ifndef EOLO_CORE_FLOW_FLOW_FEED_FORWARD_TABLE_H
#define EOLO_CORE_FLOW_FLOW_FEED_FORWARD_TABLE_H

#include <stdint.h>

// Punto medido de la curva estática bomba→flujo (PWM, L/min en régimen).
struct FlowFeedForwardPoint
{
    int pwm;
    float flowLpm;
};

// Vista de solo lectura sobre una tabla PWM→flujo monótona. Los puntos con
// flujo 0 al inicio representan el umbral de arranque de la bomba.
struct FlowFeedForwardTable
{
    const FlowFeedForwardPoint *points = nullptr;
    uint8_t size = 0;

    constexpr bool available() const { return points != nullptr && size >= 2; }

    // PWM y flujo estrictamente crecientes, salvo los ceros iniciales (umbral
    // de arranque), y sin exceder maxPwm.
    constexpr bool valid(int maxPwm) const
    {
        if (!available())
            return false;
        for (uint8_t i = 0; i < size; ++i)
        {
            if (points[i].pwm < 0 || points[i].pwm > maxPwm || !(points[i].flowLpm >= 0.0f))
                return false;
            if (i == 0)
                continue;
            if (points[i].pwm <= points[i - 1].pwm)
                return false;
            if (points[i].flowLpm <= points[i - 1].flowLpm && points[i].flowLpm > 0.0f)
                return false;
        }
        return points[size - 1].flowLpm > 0.0f;
    }

    // Primer PWM de la tabla que ya produce flujo.
    int startPwm() const
    {
        for (uint8_t i = 0; i < size; ++i)
            if (points[i].flowLpm > 0.0f)
                return points[i].pwm;
        return points[size - 1].pwm;
    }

    // PWM estimado para un flujo, acotado a los extremos de la tabla.
    int pwmForFlow(float flowLpm) const
    {
        const uint8_t i = segmentForFlow(flowLpm);
        const FlowFeedForwardPoint &a = points[i];
        const FlowFeedForwardPoint &b = points[i + 1];
        if (flowLpm <= a.flowLpm) return a.pwm;
        if (flowLpm >= b.flowLpm) return b.pwm;
        const float t = (flowLpm - a.flowLpm) / (b.flowLpm - a.flowLpm);
        return a.pwm + static_cast<int>(t * static_cast<float>(b.pwm - a.pwm) + 0.5f);
    }

    // Flujo previsto por la tabla para un PWM (interpolación directa).
    float flowForPwm(int pwm) const
    {
        if (pwm <= points[0].pwm) return points[0].flowLpm;
        for (uint8_t i = 1; i < size; ++i)
        {
            if (pwm <= points[i].pwm)
            {
                const float t = static_cast<float>(pwm - points[i - 1].pwm) /
                                static_cast<float>(points[i].pwm - points[i - 1].pwm);
                return points[i - 1].flowLpm + t * (points[i].flowLpm - points[i - 1].flowLpm);
            }
        }
        return points[size - 1].flowLpm;
    }

private:
    // Tramo [i, i+1] con flujo creciente que contiene flowLpm (o el extremo).
    uint8_t segmentForFlow(float flowLpm) const
    {
        uint8_t first = 0;
        while (first + 2 < size && points[first + 1].flowLpm <= 0.0f)
            ++first;
        uint8_t i = first;
        while (i + 2 < size && flowLpm > points[i + 1].flowLpm)
            ++i;
        return i;
    }
};

#endif
