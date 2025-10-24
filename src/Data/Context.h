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
    bool isDisplayOn = true;
    unsigned long int lastInputTime = 0;
    const unsigned long int DISPLAY_TIMEOUT_MS = 60000;

    Components components;
    Session session;

    const int CAPTURE_INTERVAL = 10;

    bool isCapturing = false;
    bool isPaused = false;
    bool isEnd = false;

    SDStatus sdStatus = SD_OK;

    const char *eoloDir = "/eolo";
    const char *logsDir = "/eolo/logs";

public:
    Context(DisplayModel &display) : u8g2(display) {}

    void begin()
    {   
        Serial.println("Inicializando contexto...");
        setDisplayPower(true);
        u8g2.begin();
        components.begin();
        u8g2.setBusClock(150000UL);
        initSD();
        Serial.println("Contexto inicializado");
    }

    void setDisplayPower(bool on)
    {
        isDisplayOn = on;
        if (on)
        {
            u8g2.setPowerSave(0);
        }
        else
        {
            u8g2.setPowerSave(1);
        }
    }

    void enableDisplay()
    {
        lastInputTime = millis();
        components.input.hasChanged = false;
        setDisplayPower(true);
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

        String startStr = String(session.startDate.unixtime());
        String durationStr = String(session.duration);
        String targetFlowStr = String(session.targetFlow);
        String usePlantowerStr = session.usePlantower ? "1" : "0";

        File file = SD.open(filename.c_str(), FILE_WRITE);
        if (!file)
        {
            Serial.println("No se pudo abrir el archivo de sesión para escribir");
            return;
        }

        file.println(startStr);
        file.println(durationStr);
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
        String durationStr = file.readStringUntil('\n');
        String targetFlowStr = file.readStringUntil('\n');
        String usePlantowerStr = file.readStringUntil('\n');
        file.close();

        session.startDate = DateTime(startStr.toInt());
        session.duration = durationStr.toInt();
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
        if (sdStatus == SD_ERROR)
        {
            Serial.println("Aviso: SD anteriormente falló!");
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
            file.print(components.rtc.now().timestamp());
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
        Serial.println("Log completado con éxito.");
    }

    void update()
    {
        components.input.poll();

        if (components.input.hasChanged)
        {   setDisplayPower(true);
            lastInputTime = millis();
            components.input.hasChanged = false;
        }

        if (isDisplayOn && (millis() - lastInputTime > DISPLAY_TIMEOUT_MS))
        {
            setDisplayPower(false);
        }
    }

    uint32_t getUnixTime()
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
        enableDisplay();
    }

    unsigned long int pauseTime = 0;
    void pauseCapture()
    {
        components.input.resetCounter();
        if (!isCapturing || isPaused)
            return;
        Serial.println("Pausando captura...");
        isPaused = true;
        pauseTime = getUnixTime();
    }

    void resumeCapture()
    {
        components.input.resetCounter();
        if (!isCapturing || !isPaused)
            return;
        Serial.println("Resumiendo captura...");
        isPaused = false;
        unsigned long now = getUnixTime();
        unsigned long pauseDelta = now - pauseTime;
        session.duration += pauseDelta;
    }

    void endCapture()
    {
        isCapturing = false;
        isEnd = true;
       
        Serial.println("Captura finalizada.");
        components.input.resetCounter();
        SceneManager::setScene("end", *this);
        enableDisplay();
    }

    void resetCapture()
    {   
        isCapturing = false;
        isPaused = false;
        session = Session();
        session.elapsedTime = 0;
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

        unsigned long now = ctx.getUnixTime();
        ctx.session.elapsedTime = now - ctx.session.startDate.unixtime();

        if (ctx.session.elapsedTime >= ctx.session.duration)
        {
            Serial.println("Duración de captura alcanzada.");
            Serial.print("Tiempo transcurrido: ");
            Serial.println(ctx.session.elapsedTime);

            Serial.print("Duración establecida: ");
            Serial.println(ctx.session.duration);

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

            logData();

#else
            Serial.println("Capturando datos... (modo barebones, solo serial)");
#endif
        }
    }

} Context;

#endif