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
    
    void begin()
    {
        if(!sensor.begin())
        {
            Serial.println("Fallo al inicializar FS3000");
        }
        else
        {
            Serial.println("FS3000 inicializado");
#if CHECK_SENSORS
            testSensor();
#endif
        }
        sensor.setRange(AIRFLOW_RANGE_7_MPS);
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
        velocity = sensor.readMetersPerSecond();
      //  float volume = velocity * 3.1416f * TUBE_RADIUS * TUBE_RADIUS; // m^3/s
      //  flow = volume * 60.0f * 1000.0f; // m^3/s to L/min

     //   flow = constrain(flow, 0.0, 20.0);
     flow = velocity*10;
   }
    
private:
    FS3000 sensor;
};

#endif