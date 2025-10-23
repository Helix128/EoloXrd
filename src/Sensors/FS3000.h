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
            sensor.setRange(AIRFLOW_RANGE_7_MPS);
            isReady = true;
#if CHECK_SENSORS
            testSensor();
#endif
        }
    }
    
    void testSensor()
    {
        Serial.println("Probando FS3000...");
        for(int i=0; i<5; i++)
        {
            readData();
            Serial.print("Velocidad: ");
            Serial.print(velocity);
            Serial.print(" m/s, Flujo: ");
            Serial.print(flow);
            Serial.println(" L/min");
            delay(125);
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
        flow = velocity * 10; // TEMPORAL: HAY QUE CALCULAR VALOR REAL??
    }

private:
    FS3000 sensor;
};

#endif