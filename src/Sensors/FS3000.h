#ifndef FS3K_HPP
#define FS3K_HPP

#include <SparkFun_FS3000_Arduino_Library.h>
#include "../Config.h"
class FS3K
{
public:
    FS3K() {}

    float velocity = 0.0; // m/s
    float flow = 0.0;     // L/min
    bool isReady = false;    
    
    void begin()
    {
        if (isReady) {
            Serial.println("FS3000 ya inicializado, skipping...");
            return;
        }

        if(!sensor.begin())
        {
            Serial.println("Fallo al inicializar FS3000");
            isReady = false;
        }
        else
        {
            Serial.println("FS3000 inicializado");
            bool isConnected = sensor.isConnected();
            if (!isConnected)
            {
                Serial.println("FS3000 no conectado");
                isReady = false;
                return;
            }
            
            sensor.setRange(AIRFLOW_RANGE_15_MPS);
            isReady = true;
#if CHECK_SENSORS
            testSensor();
#endif
        }
    }
    
    void testSensor()
    {
        Serial.println("Probando FS3000...");
        for (int i = 0; i < 20; i++)
        {
            readData();
            Serial.print("Velocidad: ");
            Serial.print(velocity);
            Serial.print(" m/s, Flujo: ");
            Serial.print(flow);
            Serial.println(" L/min");
            delay(500);
        }
    }
    void readData()
    {      
        if(!isReady)
        {
            velocity = -1;
            flow = -1;
            return;
        }
        velocity = sensor.readMetersPerSecond();
        flow = getFlow(velocity);
    }

private:
    float getFlow(float speed) const
    {
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