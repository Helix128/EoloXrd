#ifndef CONTEXT_H
#define CONTEXT_H

#include "Session.h"
#include "Components.h"
#include "../Config.h"
#include "../Drawing/SceneManager.h"
#include <SD.h>

enum SDStatus
{
    SD_OK,
    SD_WRITING,
    SD_ERROR
};

typedef struct Context
{
    DisplayModel &u8g2;
    Components components;
    Session session;

    const int CAPTURE_INTERVAL = 10;

    bool isCapturing = false;

    bool isPaused = false;
    unsigned long remainingTime = 0;

    bool isEnd = false;

    SDStatus sdStatus = SD_OK;

    const char *eoloDir = "/eolo";
    const char *logsDir = "/eolo/logs";

public:
    Context(DisplayModel &display) : u8g2(display) {}

    void begin()
    {
        u8g2.begin();
        components.begin();
        initSD();
        Serial.println("Contexto inicializado");
    }

    bool initSD()
    {
        if (!SD.begin(SD_CS_PIN))
        {
            Serial.println("Fallo al inicializar SD");
            sdStatus = SD_ERROR;
            return false;
        }

        if (!SD.exists(eoloDir))
        {
            sdStatus = SD_WRITING;
            SD.mkdir(eoloDir);
            Serial.println("Directorio /eolo creado en SD");
            sdStatus = SD_OK;
        }
        else
        {
            Serial.println("Directorio /eolo ya existe en SD");
        }

        if (!SD.exists(logsDir))
        {
            sdStatus = SD_WRITING;
            SD.mkdir(logsDir);
            Serial.println("Directorio /eolo/logs creado en SD");
            sdStatus = SD_OK;
        }
        else
        {
            Serial.println("Directorio /eolo/logs ya existe en SD");
        }

        Serial.println("SD inicializada");
        sdStatus = SD_OK;
        return true;
    }

    void saveSession(){

        String filename = String(eoloDir) + "/session.txt";

        String startStr = String(session.startTime);
        String endStr = String(session.endTime);
        String targetFlowStr = String(session.targetFlow);
        String usePlantowerStr = session.usePlantower ? "1" : "0";

        File file = SD.open(filename.c_str(), FILE_WRITE);
        if (!file)
        {
            Serial.println("No se pudo abrir el archivo de sesión para escribir");
            return;
        }

        file.println(startStr);
        file.println(endStr);
        file.println(targetFlowStr);
        file.println(usePlantowerStr);
        file.close();

        Serial.println("Sesión guardada en SD");

    }

    bool loadSession(){

        String filename = String(eoloDir) + "/session.txt";

        File file = SD.open(filename.c_str(), FILE_READ);
        if (!file)
        {
            Serial.println("No se pudo abrir el archivo de sesión para leer");
            return false;
        }

        String startStr = file.readStringUntil('\n');
        String endStr = file.readStringUntil('\n');
        String targetFlowStr = file.readStringUntil('\n');
        String usePlantowerStr = file.readStringUntil('\n');
        file.close();

        session.startTime = startStr.toInt();
        session.endTime = endStr.toInt();
        session.targetFlow = targetFlowStr.toInt();
        session.usePlantower = (usePlantowerStr == "1");

        Serial.println("Sesión cargada desde SD");
        return true;

    }

    void clearSession(){
        String filename = String(eoloDir) + "/session.txt";
        if (SD.exists(filename.c_str()))
        {
            SD.remove(filename.c_str());
            Serial.println("Archivo de sesión eliminado de SD");
        }
    }

    void logData()
    {
        if (sdStatus != SD_OK)
        {
            Serial.println("No se puede registrar datos en SD: estado no OK");
            return;
        }
        sdStatus = SD_WRITING;

        char dateStr[20];
        session.startDate.toString(dateStr);
        String filename = String(logsDir) + "/log_" + String(dateStr) + ".csv";
        bool fileExists = SD.exists(filename.c_str());

        if (!fileExists)
        {
            File file = SD.open(filename.c_str(), FILE_WRITE);
            if (!file)
            {
                sdStatus = SD_ERROR;
                Serial.println("No se pudo abrir el archivo para escribir");
                return;
            }

            file.println("time,flow,flow_target,temperature,humidity,pressure,pm1,pm25,pm10,battery_pct");
            file.close();

            Serial.println("Archivo de log creado: " + filename);
        }
        else
        {
            Serial.println("Archivo de log ya existe: " + filename);

            File file = SD.open(filename.c_str(), FILE_WRITE);
            if (!file)
            {
                sdStatus = SD_ERROR;
                Serial.println("No se pudo abrir el archivo para escribir");
                return;
            }

            // time
            char dateStr[20];
            components.rtc.now().toString(dateStr);
            file.print(dateStr);
            file.print(",");

            // flow
            file.print(components.flowSensor.flow);
            file.print(",");

            // flow_target
            file.print(session.targetFlow);
            file.print(",");

            // temperature
            file.print(components.bme.temperature);
            file.print(",");

            // humidity
            file.print(components.bme.humidity);
            file.print(",");

            // pressure
            file.print(components.bme.pressure);
            file.print(",");

            // pm1
            file.print(components.plantower.pm1);
            file.print(",");

            // pm25
            file.print(components.plantower.pm25);
            file.print(",");

            // pm10
            file.print(components.plantower.pm10);
            file.print(",");

            // battery_pct
            file.print(components.battery.getPct());
            file.println();
            file.close();

            Serial.println("Archivo de log escrito!");
        }
        sdStatus = SD_OK;
    }

    void update()
    {
        components.input.poll();
    }

    uint32_t getCurrentSeconds()
    {
        if (components.rtc.ok == false)
            return 0;
        return components.rtc.now().unixtime();
    }

    void beginCapture()
    {
        session.elapsedTime = 0;
        isCapturing = true;
        session.capturedVolume = 0.0;
        Serial.println("Iniciando captura...");
        SceneManager::setScene("captura", *this);
    }

    void pauseCapture()
    {
        components.input.resetCounter();
        if (!isCapturing || isPaused)
            return;
        Serial.println("Pausando captura...");
        isPaused = true;
        unsigned long now = getCurrentSeconds();
        remainingTime = session.endTime - now;
    }

    void resumeCapture()
    {
        components.input.resetCounter();
        if (!isCapturing || !isPaused)
            return;
        Serial.println("Resumiendo captura...");
        isPaused = false;
        unsigned long now = getCurrentSeconds();
        session.endTime = now + remainingTime;
    }

    void endCapture()
    {
        isCapturing = false;
        isEnd = true;
        resetCapture();
        Serial.println("Captura finalizada.");
        components.input.resetCounter();
        SceneManager::setScene("end", *this);
    }

    void resetCapture()
    {   
        isCapturing = false;
        isPaused = false;
        remainingTime = 0;
        session = Session();
        Serial.println("Estado de captura reiniciado.");
    }

    void updateMotors()
    {
        float currentFlow = components.flowSensor.flow;
        float targetFlow = session.targetFlow;
        float error = targetFlow - currentFlow;
        components.motor.setPowerPct(components.motor.getPowerPct() + error * 0.1f);
    }

    void updateCapture()
    {
        Context &ctx = *this;

        if (!ctx.isCapturing || ctx.isPaused)
            return;

        unsigned long now = ctx.getCurrentSeconds();
        ctx.session.elapsedTime = now - ctx.session.startTime;

        if (now > ctx.session.endTime)
        {
            endCapture();
            return;
        }

#if !BAREBONES
        ctx.components.flowSensor.readData();
        updateMotors();
#else
        ctx.components.flowSensor.flow = ctx.session.targetFlow + millis() % 2;
#endif

        if (now - ctx.session.lastLog >= ctx.CAPTURE_INTERVAL)
        {
            ctx.session.lastLog = now;

#if !BAREBONES
            ctx.components.bme.readData();

            ctx.components.plantower.setPower(ctx.session.usePlantower);
            ctx.components.plantower.readData();

#else
            Serial.println("Capturando datos... (modo barebones, solo serial)");
#endif
        }
    }

} Context;

#endif