#ifndef EOLO_CORE_CALIBRATION_PUMP_ORDER_MODEL_H
#define EOLO_CORE_CALIBRATION_PUMP_ORDER_MODEL_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

// Orden de arranque de las dos bombas: la que da menos caudal a tope (la
// "debil") se llena primero y la fuerte entra solo con la debil saturada. Este
// modelo decide cual es la debil a partir de una prueba rapida (cada bomba a
// tope por separado) y define el registro que se guarda en flash.
class PumpOrderModel
{
public:
    static constexpr uint32_t Magic = 0x454F5030UL; // "EOP0"
    static constexpr uint16_t Version = 1;
    // La bomba fuerte debe mover al menos esto para fiarse de la medicion.
    static constexpr float MinStrongFlow = 0.2f;

    struct Record
    {
        uint32_t magic;
        uint16_t version;
        int16_t weak;
        float flow0;
        float flow1;
        uint32_t checksum;
    };

    // Caudal medido con la bomba 0 sola y con la 1 sola. Devuelve false si la
    // prueba no es concluyente (valores no finitos, negativos o sin caudal).
    static bool decide(float flow0, float flow1, int &weak)
    {
        if (!isfinite(flow0) || !isfinite(flow1) || flow0 < 0.0f || flow1 < 0.0f)
            return false;
        if ((flow0 > flow1 ? flow0 : flow1) < MinStrongFlow)
            return false;
        weak = flow0 <= flow1 ? 0 : 1; // empate: bomba 0, igual que la calibracion
        return true;
    }

    static uint32_t checksum(const Record &record)
    {
        const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
        uint32_t value = 2166136261UL;
        for (size_t i = 0; i < offsetof(Record, checksum); ++i)
        {
            value ^= bytes[i];
            value *= 16777619UL;
        }
        return value;
    }

    static Record make(int weak, float flow0, float flow1)
    {
        Record record = {};
        record.magic = Magic;
        record.version = Version;
        record.weak = static_cast<int16_t>(weak);
        record.flow0 = flow0;
        record.flow1 = flow1;
        record.checksum = checksum(record);
        return record;
    }

    static bool valid(const Record &record)
    {
        return record.magic == Magic && record.version == Version &&
               (record.weak == 0 || record.weak == 1) &&
               isfinite(record.flow0) && isfinite(record.flow1) &&
               record.checksum == checksum(record);
    }
};

#endif
