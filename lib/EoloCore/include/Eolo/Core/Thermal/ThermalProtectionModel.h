#ifndef EOLO_CORE_THERMAL_PROTECTION_MODEL_H
#define EOLO_CORE_THERMAL_PROTECTION_MODEL_H

#include <math.h>
#include <Eolo/Core/Sensors/NtcThermistor.h>

// DTO de entrada/salida para que el adaptador de placa pueda aplicar la
// decisión térmica al motor, la interfaz y el registro sin llevar Context al
// núcleo.
struct ThermalProtectionInput
{
    bool latched = false;
    bool sensorValid = false;
    float temperature = -99.0f;
    float highThreshold = 70.0f;
    float lowThreshold = 60.0f;
};

struct ThermalProtectionOutput
{
    bool latched = false;
    bool changed = false;
    bool motorAllowed = true;
};

class ThermalProtectionModel
{
public:
    static ThermalProtectionOutput update(const ThermalProtectionInput &input)
    {
        ThermalProtectionOutput output;
        output.latched = NtcThermistor::motorOverheatLatched(
            input.latched,
            input.sensorValid,
            input.temperature,
            input.highThreshold,
            input.lowThreshold);
        output.changed = output.latched != input.latched;
        output.motorAllowed = !output.latched;
        return output;
    }
};

#endif // EOLO_CORE_THERMAL_PROTECTION_MODEL_H
