#ifndef BMESENSOR_H
#define BMESENSOR_H

#include "Adafruit_BME280.h"
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
    bool isReady = false;

    void begin()
    {
        if (isReady)
        {
            LOG_LN("BME280 ya inicializado, skipping...");
            return;
        }

        LOG_LN("Iniciando BME280...");
        if (!bme.begin(0x76) && !bme.begin(0x77))
        {
            LOG_LN("Fallo al inicializar BME280");
            isReady = false;
            return;
        }

        LOG_LN("BME280 inicializado");
        isReady = true;
#if CHECK_SENSORS
        testSensor();
#endif
    }

    void testSensor()
    {
        for (int i = 0; i < 5; i++)
        {
            readData();
            LOG_OUT("Temperature: ");
            LOG_OUT(temperature);
            LOG_LN(" °C");
            LOG_OUT("Humidity: ");
            LOG_OUT(humidity);
            LOG_LN(" %");
            LOG_OUT("Pressure: ");
            LOG_OUT(pressure);
            LOG_LN(" hPa");
            delay(100);
        }
    }
    void readData()
    {
        if (!isReady)
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