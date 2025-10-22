#ifndef BMESENSOR_H
#define BMESENSOR_H

#include <Adafruit_BME280.h>
#include "../Config.h"

// Clase para manejar el sensor BME280 (temperatura, humedad, presión)
class BME280
{
private:
    Adafruit_BME280 bme;

public:
    float temperature = 0.0;
    float humidity = 0.0;
    float pressure = 0.0;

    bool ok = false;
    void begin()
    {
        if(!bme.begin(0x76)){
            Serial.println("Fallo al inicializar BME280");
            ok = false;
            return;
        }
#if CHECK_SENSORS
        testSensor();
#endif
    }

    void testSensor(){
        for (int i = 0; i < 5; i++)
        {
            readData();
            Serial.print("Temperature: ");
            Serial.print(temperature);
            Serial.println(" °C");
            Serial.print("Humidity: ");
            Serial.print(humidity);
            Serial.println(" %");
            Serial.print("Pressure: ");
            Serial.print(pressure);
            Serial.println(" hPa");
            delay(100);
        }
    }
    void readData()
    {   
        if(!ok)
        {
            temperature = -1000;
            humidity = -1;
            pressure = -1;
            return;
        }
        temperature = bme.readTemperature();
        humidity = bme.readHumidity();
        pressure = bme.readPressure() / 100.0F; // Convertir Pa a hPa
    }
};

#endif