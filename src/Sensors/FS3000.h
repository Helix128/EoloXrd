#ifndef FS3K_HPP
#define FS3K_HPP

#include <SparkFun_FS3000_Arduino_Library.h>
#include "../Config.h"
#include <Eolo/Types/FlowData.h>
class FS3K
{
public:
    FS3K() {}

    float velocity = 0.0; // m/s
    float flow = 0.0;     // L/min (promedio)
    float currentFlow = 0.0; // L/min (valor más reciente)
    bool isReady = false;    
    
    void begin()
    {
        if (isReady) {
            LOG_LN("FS3000 ya inicializado, skipping...");
            return;
        }

        if(!sensor.begin())
        {
            LOG_LN("Fallo al inicializar FS3000");
            isReady = false;
        }
        else
        {
            LOG_LN("FS3000 inicializado");
            bool isConnected = sensor.isConnected();
            if (!isConnected)
            {
                LOG_LN("FS3000 no conectado");
                isReady = false;
                return;
            }
            
            sensor.setRange(AIRFLOW_RANGE_15_MPS);
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
    
    void readData()
    {      
        if(!isReady)
        {
            velocity = -1;
            flow = -1;
            currentFlow = -1;
            return;
        }
        velocity = sensor.readMetersPerSecond();
        currentFlow = getFlow(velocity);
        
        // Guardar en el buffer circular
        flowBuffer[bufferIndex] = currentFlow;
        bufferIndex = (bufferIndex + 1) % MAX_AVG_VALUES;
        
        // Calcular promedio
        float sum = 0.0;
        for (int i = 0; i < MAX_AVG_VALUES; i++) {
            sum += flowBuffer[i];
        }
        flow = sum / MAX_AVG_VALUES;
    }

    bool getData(FlowData &output)
    {
        readData();
        output.flow = flow;
        output.velocity = velocity;
        output.valid = isReady && flow >= 0.0f;
        output.fresh = output.valid;
        output.stale = false;
        output.ageMs = 0;
        return output.valid;
    }

private:
    static const int MAX_AVG_VALUES = 16;
    float flowBuffer[MAX_AVG_VALUES] = {0};
    int bufferIndex = 0;
    
    float getFlow(float speed) const
    {   
        if(speed<=0) {
            return -1.0f; // Indica error en la lectura
        }
        
        static const float velocityPoints[] = {0.06, 0.34, 0.7, 1.06, 1.42, 1.74, 2.02, 2.30, 2.60, 2.80, 3.00, 3.33};
        static const float flowPoints[] = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0};
        static const int numPoints = 12;

        if (speed < velocityPoints[0]) {
            return 0.0;
        }

        for (int i = 0; i < numPoints - 1; i++) {
            if (speed < velocityPoints[i + 1]) {
                float slope = (flowPoints[i + 1] - flowPoints[i]) / (velocityPoints[i + 1] - velocityPoints[i]);
                return flowPoints[i] + slope * (speed - velocityPoints[i]);
            }
        }

        float slope = (flowPoints[numPoints - 1] - flowPoints[numPoints - 2]) / 
                     (velocityPoints[numPoints - 1] - velocityPoints[numPoints - 2]);
        return flowPoints[numPoints - 1] + slope * (speed - velocityPoints[numPoints - 1]);
    }
    FS3000 sensor;
};

#endif
