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

    void lock()
    {
        if (_mutex != nullptr)
            xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
    }

    void unlock()
    {
        if (_mutex != nullptr)
            xSemaphoreGiveRecursive(_mutex);
    }

    class Guard
    {
    public:
        Guard() { SPIBus::getInstance().lock(); }
        ~Guard() { SPIBus::getInstance().unlock(); }
        Guard(const Guard &) = delete;
        Guard &operator=(const Guard &) = delete;
    };
};

#endif
