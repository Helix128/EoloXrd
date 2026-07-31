#ifndef EOLO_SPI_BUS_H
#define EOLO_SPI_BUS_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// El OLED HW-SPI y la SD comparten VSPI. El guard recursivo evita que dos
// tareas cambien simultáneamente CS/clock/transferencias del mismo periférico.
class SPIBus
{
    SemaphoreHandle_t _mutex = nullptr;

    SPIBus() : _mutex(xSemaphoreCreateRecursiveMutex()) {}
    SPIBus(const SPIBus &) = delete;
    SPIBus &operator=(const SPIBus &) = delete;

public:
    static SPIBus &getInstance()
    {
        static SPIBus instance;
        return instance;
    }

    bool lock(TickType_t timeoutTicks = portMAX_DELAY)
    {
        if (_mutex == nullptr)
            return true;
        return xSemaphoreTakeRecursive(_mutex, timeoutTicks) == pdTRUE;
    }

    void unlock()
    {
        if (_mutex != nullptr)
            xSemaphoreGiveRecursive(_mutex);
    }

    class Guard
    {
        bool _acquired = false;

    public:
        explicit Guard(TickType_t timeoutTicks = portMAX_DELAY)
            : _acquired(SPIBus::getInstance().lock(timeoutTicks)) {}
        ~Guard()
        {
            if (_acquired)
                SPIBus::getInstance().unlock();
        }
        Guard(const Guard &) = delete;
        Guard &operator=(const Guard &) = delete;

        bool acquired() const { return _acquired; }
    };
};

#endif
