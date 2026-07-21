#ifndef BMESENSOR_H
#define BMESENSOR_H

#include <math.h>
#include "Adafruit_BME280.h"
#include "../Config/Legacy.h"
#include "../Board/I2CBus.h"
#include <Eolo/Types/BME280Data.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>

// Clase para manejar el sensor BME280 (temperatura, humedad, presión)
class BME280
{
private:
    Adafruit_BME280 bme;
    SemaphoreHandle_t _dataMutex = nullptr;
    bool _hasData = false;

public:
    float temperature = 0.0;
    float humidity = 0.0;
    float pressure = 0.0;
    std::atomic_bool isReady{false};

    BME280() : _dataMutex(xSemaphoreCreateMutex()) {}

    bool begin()
    {
        if (isReady.load())
        {
            LOG_LN("BME280 ya inicializado, skipping...");
            return true;
        }

        LOG_LN("Iniciando BME280...");
        bool sensorReady = false;
        {
            I2CBus::Guard guard;
            if (guard.acquired())
                sensorReady = bme.begin(0x76) || bme.begin(0x77);
        }
        I2CBus::getInstance().applyProfile();
        if (!sensorReady)
        {
            LOG_LN("Fallo al inicializar BME280");
            isReady = false;
            return false;
        }

        LOG_LN("BME280 inicializado");
        isReady = true;
#if CHECK_SENSORS
        testSensor();
#endif
        return true;
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
    bool readData()
    {
        if (!isReady.load())
            return false;
        I2CBus::Guard guard;
        if (!guard.acquired())
            return false;
        float nextTemperature = bme.readTemperature();
        float nextHumidity = bme.readHumidity();
        float nextPressure = bme.readPressure() / 100.0F; // Pa -> hPa
        bool valid = isfinite(nextTemperature) && isfinite(nextHumidity) &&
                     isfinite(nextPressure);
        if (!valid) {
            isReady = false;
            return false;
        }

        bool locked = !_dataMutex ||
                      xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(5)) == pdTRUE;
        if (!locked)
            return false;
        temperature = nextTemperature;
        humidity = nextHumidity;
        pressure = nextPressure;
        _hasData = true;
        if (_dataMutex)
            xSemaphoreGive(_dataMutex);
        return true;
    }

    void poll() { (void)readData(); }

    bool getData(BME280Data &output)
    {
        bool locked = !_dataMutex ||
                      xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(5)) == pdTRUE;
        if (!locked)
            return false;
        output.temperature = temperature;
        output.humidity = humidity;
        output.pressure = pressure;
        output.valid = _hasData && isfinite(temperature) && isfinite(humidity) && isfinite(pressure);
        if (_dataMutex)
            xSemaphoreGive(_dataMutex);
        return output.valid;
    }
};

#endif
