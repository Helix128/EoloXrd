#ifndef FS3K_HPP
#define FS3K_HPP

#include <math.h>
#include <SparkFun_FS3000_Arduino_Library.h>
#include "../Config/Legacy.h"
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
        if (isReady.load()) {
            LOG_LN("FS3000 ya inicializado, skipping...");
            return true;
        }

        bool sensorReady = false;
        {
            I2CBus::Guard guard;
            sensorReady = guard.acquired() && sensor.begin();
        }
        I2CBus::getInstance().applyProfile();
        if(!sensorReady)
        {
            LOG_LN("Fallo al inicializar FS3000");
            isReady = false;
            return false;
        }
        else
        {
            LOG_LN("FS3000 inicializado");
            bool isConnected = false;
            {
                I2CBus::Guard guard;
                isConnected = guard.acquired() && sensor.isConnected();
            }
            if (!isConnected)
            {
                LOG_LN("FS3000 no conectado");
                isReady = false;
                return false;
            }
            
            {
                I2CBus::Guard guard;
                if (guard.acquired())
                    sensor.setRange(AIRFLOW_RANGE_15_MPS);
            }
#if CHECK_SENSORS
            // check extra
            // Inicializar buffer de promedio y tomar lecturas para estabilizar
            for (int i = 0; i < MAX_AVG_VALUES; i++) {
                flowBuffer[i] = 0.0f;
            }
            bufferIndex = 0;
            // Tomar varias lecturas rápidas para llenar el buffer
            for (int i = 0; i < MAX_AVG_VALUES * 2; i++) {
                readData();
                delay(50);
            }
#endif
            isReady = true;
#if CHECK_SENSORS
            testSensor();
#endif
            return true;
        }
    }
    
    void testSensor()
    {
        LOG_LN("Probando FS3000...");
        for (int i = 0; i < 20; i++)
        {
            readData();
            LOG_OUT("Velocidad: ");
            LOG_OUT(velocity);
            LOG_OUT(" m/s, Flujo actual: ");
            LOG_OUT(currentFlow);
            LOG_OUT(" L/min, Flujo promedio: ");
            LOG_OUT(flow);
            LOG_LN(" L/min");
            delay(500);
        }
    }
    
    bool readData()
    {      
        if(!isReady.load())
            return false;
        I2CBus::Guard guard;
        if (!guard.acquired())
            return false;
        float nextVelocity = sensor.readMetersPerSecond();
        if (!isfinite(nextVelocity) || nextVelocity < 0.0f) {
            isReady = false;
            return false;
        }
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
        if (_dataMutex)
            xSemaphoreGive(_dataMutex);
        return isfinite(nextFlow) && nextFlow >= 0.0f;
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
    
    FS3000 sensor;
};

#endif
