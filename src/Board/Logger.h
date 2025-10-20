#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#define SD_CS_PIN 5
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN 18

#define logFile "/log.csv"

enum SDStatus
{
    SD_OK,
    SD_WRITING,
    SD_ERROR
}; 

class Logger
{
public:
    static SDStatus status;

    Logger() {}

    bool begin()
    {
        if (!SD.begin(SD_CS_PIN))
        {
            Serial.println("Fallo al inicializar SD");
            Logger::status = SD_ERROR;
            return false;
        }

        if (!SD.exists(logFile))
        {
            File header = SD.open(logFile, FILE_WRITE);
            if (header)
            {
                header.println("timestamp,flow,flow_target,temperature,humidity,pressure,pm1,pm25,pm10,battery_pct");
                header.close();
            }
        }

        Serial.println("SD inicializada");
        Logger::status = SD_OK;
        return true;
    }

    void capture(unsigned long timestamp, float flow, float flowTarget, 
                 float temperature, float humidity, float pressure,
                 float pm1, float pm25, float pm10, int batteryPct)
    {   

        if (Logger::status == SD_ERROR)
        {
            Serial.println("Logger: Error de SD, intentando re-inicializar...");
            if (!begin()) return;
        }

        if(Logger::status != SD_OK) {
            Serial.println("Logger: SD no está lista");
            return;
        }

        File dataFile = SD.open(logFile, FILE_APPEND);
        if (!dataFile)
        {
            Serial.println("Error abriendo archivo en SD para append");
            Logger::status = SD_ERROR;
            return;
        }

        Logger::status = SD_WRITING;

        dataFile.print(timestamp);
        dataFile.print(',');
        dataFile.print(flow);
        dataFile.print(',');
        dataFile.print(flowTarget);
        dataFile.print(',');
        dataFile.print(temperature);
        dataFile.print(',');
        dataFile.print(humidity);
        dataFile.print(',');
        dataFile.print(pressure);
        dataFile.print(',');
        dataFile.print(pm1);
        dataFile.print(',');
        dataFile.print(pm25);
        dataFile.print(',');
        dataFile.print(pm10);
        dataFile.print(',');
        dataFile.print(batteryPct);
        dataFile.println();
        dataFile.close();

        Serial.println("Datos guardados en SD");
        Logger::status = SD_OK;
    }
};

inline SDStatus Logger::status = SD_OK;

#endif