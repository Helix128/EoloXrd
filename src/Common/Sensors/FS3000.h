#ifndef FS3K_HPP
#define FS3K_HPP

#include <math.h>
#include "../../Variants/Legacy.h"
#include "../Board/I2CBus.h"
#include <Eolo/Core/Sensors/FS3000FlowModel.h>
#include <Eolo/Types/FlowData.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>
class FS3K
{
public:
    FS3K() : _dataMutex(xSemaphoreCreateMutex()) {}

    float velocity = 0.0; // m/s
    float flow = 0.0;     // L/min (promedio)
    float currentFlow = 0.0; // L/min (valor más reciente)
    std::atomic_bool isReady{false};
    
    bool begin()
    {
        if (isReady.load())
            return true;

        I2CBus &bus = I2CBus::getInstance();
        if (bus.probe(FS3000FlowModel::I2cAddress, true) != I2CBus::Result::Ok)
        {
            LOG_LN("FS3000 no responde");
            return false;
        }
        LOG_LN("FS3000 inicializado");
        isReady = true;
        return true;
    }

    // false = fallo de lectura (I2C, trama corrupta). Sin flujo no es fallo:
    // devuelve true con flow 0.
    bool readData()
    {
        if(!isReady.load())
            return false;
        uint8_t frame[FS3000FlowModel::FrameSize];
        uint16_t raw = 0;
        if (!I2CBus::getInstance().readBytes(FS3000FlowModel::I2cAddress, frame, sizeof(frame)) ||
            !FS3000FlowModel::parseFrame(frame, raw))
            return false;
        float nextVelocity = FS3000FlowModel::velocityFromRaw(raw);
        float nextCurrentFlow = FS3000FlowModel::flowFromVelocity(nextVelocity);

        // Guardar en el buffer circular
        flowBuffer[bufferIndex] = nextCurrentFlow;
        bufferIndex = (bufferIndex + 1) % MAX_AVG_VALUES;
        
        // Calcular promedio
        float sum = 0.0;
        for (int i = 0; i < MAX_AVG_VALUES; i++) {
            sum += flowBuffer[i];
        }
        float nextFlow = sum / MAX_AVG_VALUES;
        bool locked = !_dataMutex ||
                      xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(5)) == pdTRUE;
        if (!locked)
            return false;
        velocity = nextVelocity;
        currentFlow = nextCurrentFlow;
        flow = nextFlow;
        _hasData = true;
        _lastSuccessMs = millis();
        ++_sampleId;
        if (_dataMutex)
            xSemaphoreGive(_dataMutex);
        return true;
    }

    bool poll() { return readData(); }

    bool getData(FlowData &output)
    {
        bool locked = !_dataMutex ||
                      xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(5)) == pdTRUE;
        if (!locked)
            return false;
        output.flow = flow;
        output.velocity = velocity;
        output.valid = _hasData && flow >= 0.0f;
        output.ageMs = _hasData ? millis() - _lastSuccessMs : 0;
        output.fresh = output.valid && output.ageMs <= 50UL;
        output.stale = output.valid && output.ageMs > 5000UL;
        output.sampleId = _sampleId;
        if (_dataMutex)
            xSemaphoreGive(_dataMutex);
        return output.valid;
    }

private:
    static const int MAX_AVG_VALUES = 16;
    float flowBuffer[MAX_AVG_VALUES] = {0};
    int bufferIndex = 0;
    SemaphoreHandle_t _dataMutex = nullptr;
    bool _hasData = false;
    uint32_t _lastSuccessMs = 0;
    uint32_t _sampleId = 0;
};

#endif
