#ifndef BMESENSOR_H
#define BMESENSOR_H

#include <math.h>
#include "Adafruit_BME280.h"
#include "../Config/Legacy.h"
#include "../Board/I2CBus.h"
#include "DirectBME280.h"
#include <Eolo/Types/BME280Data.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>

// Clase para manejar el sensor BME280 (temperatura, humedad, presión)
class BME280
{
private:
    Adafruit_BME280 bme;
    DirectBME280 directBme;
    SemaphoreHandle_t _dataMutex = nullptr;
    bool _hasData = false;

public:
    using InitFailure = DirectBME280::InitFailure;
    float temperature = 0.0;
    float humidity = 0.0;
    float pressure = 0.0;
    std::atomic_bool isReady{false};
    std::atomic<InitFailure> lastInitFailure{InitFailure::None};

    BME280() : _dataMutex(xSemaphoreCreateMutex()) {}

    bool begin()
    {
        if (isReady.load())
            return true;
        bool sensorReady = false;
#if EOLO_I2C_DIRECT_DRIVERS
        sensorReady = directBme.begin();
        lastInitFailure = directBme.lastFailure();
#else
        {
            I2CBus::Guard guard;
            if (guard.acquired())
                sensorReady = bme.begin(0x76) || bme.begin(0x77);
        }
        I2CBus::getInstance().applyProfile();
        if (!sensorReady) {
            I2CBus::AddressStats a76 = I2CBus::getInstance().getAddressStats(0x76);
            I2CBus::AddressStats a77 = I2CBus::getInstance().getAddressStats(0x77);
            lastInitFailure = (a76.lastResult == I2CBus::Result::Timeout || a77.lastResult == I2CBus::Result::Timeout)
                                  ? InitFailure::Timeout : InitFailure::Nack7677;
        }
#endif
        if (!sensorReady)
        {
            isReady = false;
            return false;
        }

        lastInitFailure = InitFailure::None;
        isReady = true;
#if CHECK_SENSORS
        testSensor();
#endif
        return true;
    }

    const char *lastInitFailureName() const {
        return DirectBME280::failureName(lastInitFailure.load());
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
        float nextTemperature = NAN;
        float nextHumidity = NAN;
        float nextPressure = NAN;
#if EOLO_I2C_DIRECT_DRIVERS
        if (!directBme.readData(nextTemperature, nextHumidity, nextPressure)) {
            isReady = false;
            return false;
        }
#else
        I2CBus::Guard guard;
        if (!guard.acquired())
            return false;
        nextTemperature = bme.readTemperature();
        nextHumidity = bme.readHumidity();
        nextPressure = bme.readPressure() / 100.0F; // Pa -> hPa
#endif
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
