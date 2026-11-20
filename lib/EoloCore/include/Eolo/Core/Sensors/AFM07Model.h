#ifndef EOLO_CORE_SENSORS_AFM07_MODEL_H
#define EOLO_CORE_SENSORS_AFM07_MODEL_H

#include <math.h>
#include <stdint.h>
#include <Eolo/Services/SettingsStore.h>
#include <Eolo/Types/FlowData.h>

class AFM07Model
{
public:
    // El registro 0x0000 trae el caudal escalado; la escala varía entre
    // unidades del mismo modelo, así que se guarda por equipo (NVS).
    static constexpr const char *DivisorKey = "divisor";
    static constexpr float SupportedDivisors[] = {10.0f, 100.0f, 1000.0f};
    static constexpr float MinPlausibleLpm = 1.5f;

    static bool isSupportedDivisor(float divisor)
    {
        for (float d : SupportedDivisors)
            if (divisor == d)
                return true;
        return false;
    }

    // Siguiente divisor soportado (cíclico). Un valor desconocido pasa al primero.
    static float nextDivisor(float divisor)
    {
        const size_t n = sizeof(SupportedDivisors) / sizeof(SupportedDivisors[0]);
        for (size_t i = 0; i < n; ++i)
            if (SupportedDivisors[i] == divisor)
                return SupportedDivisors[(i + 1) % n];
        return SupportedDivisors[0];
    }

    // Elige el divisor que deja el caudal en [1,5; 15) L/min: la bomba a media
    // potencia entrega ese orden y la ventana abarca una década, así que hay
    // exactamente un candidato. Solo vale con la bomba en marcha: un caudal real
    // bajo se confundiría con un divisor demasiado grande. Devuelve 0 si no hay
    // lectura (raw 0) o queda fuera de rango.
    static float detectDivisor(uint16_t raw)
    {
        for (float d : SupportedDivisors)
        {
            const float lpm = (float)raw / d;
            if (lpm >= MinPlausibleLpm && lpm < MinPlausibleLpm * 10.0f)
                return d;
        }
        return 0.0f;
    }

    static float loadDivisor(SettingsStore &store, float defaultDivisor)
    {
        const float stored = (float)store.getUInt(DivisorKey, (uint32_t)defaultDivisor);
        return isSupportedDivisor(stored) ? stored : defaultDivisor;
    }

    static bool saveDivisor(SettingsStore &store, float divisor)
    {
        return isSupportedDivisor(divisor) && store.putUInt(DivisorKey, (uint32_t)divisor);
    }

    static float flowFromRaw(uint16_t raw, float divisor)
    {
        if (!isfinite(divisor) || divisor <= 0.0f)
            return -1.0f;
        return (float)raw / divisor;
    }

    static void applyReadSuccess(FlowData &data, uint32_t &lastSuccessMs, uint16_t raw, uint32_t nowMs, float divisor)
    {
        float flow = flowFromRaw(raw, divisor);
        if (flow < 0.0f || !isfinite(flow))
        {
            data.valid = false;
            data.fresh = false;
            data.stale = true;
            data.ageMs = 0;
            return;
        }
        data.flow = flow;
        data.velocity = flow;
        data.valid = true;
        data.fresh = true;
        data.stale = false;
        data.ageMs = 0;
        // sampleId, no lastSuccessMs, es el sentinel durable de "hubo muestra".
        // millis() puede valer cero legítimamente al arrancar y tras el wraparound.
        if (++data.sampleId == 0)
            ++data.sampleId;
        lastSuccessMs = nowMs;
    }

    static void applyReadFailure(FlowData &data, uint32_t lastSuccessMs, uint32_t nowMs, uint32_t freshDataMs, uint32_t staleDataMs)
    {
        const bool hasSample = data.sampleId != 0;
        uint32_t ageMs = hasSample ? nowMs - lastSuccessMs : staleDataMs + 1;
        data.ageMs = ageMs;
        data.fresh = hasSample && ageMs <= freshDataMs;
        data.stale = hasSample && ageMs > freshDataMs;
        data.valid = hasSample && ageMs <= staleDataMs;
    }

    static bool refreshAge(FlowData &data, uint32_t lastSuccessMs, uint32_t nowMs, uint32_t freshDataMs, uint32_t staleDataMs)
    {
        const bool hasSample = data.sampleId != 0;
        uint32_t ageMs = hasSample ? nowMs - lastSuccessMs : staleDataMs + 1;
        data.ageMs = ageMs;
        data.fresh = hasSample && ageMs <= freshDataMs;
        data.stale = hasSample && ageMs > freshDataMs;
        if (!hasSample || ageMs > staleDataMs)
        {
            data.valid = false;
            data.fresh = false;
        }
        return data.valid;
    }
};

#endif // EOLO_CORE_SENSORS_AFM07_MODEL_H
