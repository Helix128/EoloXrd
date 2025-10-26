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
    bool isDisplayReady = false;
    unsigned long int lastInputTime = 0;
    const unsigned long int DISPLAY_TIMEOUT_MS = 60000;

    Components components;
    Session session;

    const int CAPTURE_INTERVAL = 10;

    bool isCapturing = false;
    bool isPaused = false;
    bool isEnd = false;

    SDStatus sdStatus = SD_OK;

    const char *eoloDir = "/EOLO";
    const char *logsDir = "/EOLO/logs";

public:
    Context(DisplayModel &display) : u8g2(display) {}

    void begin()
    {
        Serial.println("Inicializando contexto...");
        setDisplayPower(true);
        if (!isDisplayReady)
        {   
            Serial.println("Iniciando pantalla...");
            u8g2.begin();
            u8g2.setBusClock(150000UL); // 150kHz (fix pantalla - para que la señal sea más cuadrada)
            isDisplayReady = true;
            Serial.println("Pantalla iniciada");
        
        }
        components.begin();

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

        sdcard_type_t sdType = SD.cardType();
        if (sdType == CARD_NONE)
        {
            Serial.println("No se detectó tarjeta SD");
            sdStatus = SD_ERROR;
            return false;
        }
        else
        {
            Serial.print("Tipo de tarjeta SD: ");
            switch (sdType)
            {
            case CARD_MMC:
                Serial.println("MMC");
                break;
            case CARD_SD:
                Serial.println("SDSC");
                break;
            case CARD_SDHC:
                Serial.println("SDHC");
                break;
            case CARD_UNKNOWN:
            default:
                Serial.println("Desconocido");
                break;
            }
        }

        uint64_t sdCardSize = SD.cardSize();
        Serial.print("Tamaño de la tarjeta SD: ");
        Serial.print(sdCardSize / (1024 * 1024));
        Serial.println(" MB");

        uint64_t totalBytes = SD.totalBytes();
        uint64_t usedBytes = SD.usedBytes();
        Serial.print("Espacio total: ");
        Serial.print(totalBytes / (1024 * 1024));
        Serial.println(" MB");
        Serial.print("Espacio usado: ");
        Serial.print(usedBytes / (1024 * 1024));
        Serial.print(" MB (");
        Serial.print((usedBytes * 100) / totalBytes);
        Serial.println("%)");

        if (!SD.exists(eoloDir))
        {
            sdStatus = SD_WRITING;
            SD.mkdir(eoloDir);
            Serial.println("Directorio /EOLO creado en SD");
            sdStatus = SD_OK;
        }
        else
        {
            Serial.println("Directorio /EOLO ya existe en SD");
        }

        if (!SD.exists(logsDir))
        {
            sdStatus = SD_WRITING;
            SD.mkdir(logsDir);
            Serial.println("Directorio /EOLO/logs creado en SD");
            sdStatus = SD_OK;
        }
        else
        {
            Serial.println("Directorio /EOLO/logs ya existe en SD");
        }

        Serial.println("SD inicializada");
        sdStatus = SD_OK;
        return true;
    }

    void saveSession()
    {

        String filename = String(eoloDir) + "/session.txt";

        String startStr = session.startDate.timestamp();
        String durationStr = String(session.duration);
        String targetFlowStr = String(session.targetFlow);
        String capturedVolumeStr = String(session.capturedVolume);
        String usePlantowerStr = session.usePlantower ? "1" : "0";

        File file = SD.open(filename.c_str(), "w+");
        if (!file)
        {
            Serial.println("No se pudo abrir el archivo de sesión para escribir");
            return;
        }

        file.println(startStr);
        file.println(durationStr);
        file.println(targetFlowStr);
        file.println(capturedVolumeStr);
        file.println(usePlantowerStr);
        file.close();

        Serial.println("Sesión guardada en SD:");
        Serial.print(" startDate: ");
        Serial.println(startStr);
        Serial.print(" duration: ");
        Serial.println(durationStr);
        Serial.print(" targetFlow: ");
        Serial.println(targetFlowStr);
        Serial.print(" capturedVolume: ");
        Serial.println(capturedVolumeStr);
        Serial.print(" usePlantower: ");
        Serial.println(usePlantowerStr);
    }

    bool canLoadSession()
    {
        String filename = String(eoloDir) + "/session.txt";
        if (!SD.exists(filename.c_str()))
            return false;

        File file = SD.open(filename.c_str(), "r+");
        if (!file)
            return false;

        size_t fileSize = file.size();
        file.close();
        return fileSize > 0;
    }

    bool loadSession()
    {

        String filename = String(eoloDir) + "/session.txt";

        File file = SD.open(filename.c_str(), "r+");
        if (!file)
        {
            Serial.println("No se pudo abrir el archivo de sesión para leer");
            return false;
        }

        String startStr = file.readStringUntil('\n');
        String durationStr = file.readStringUntil('\n');
        String targetFlowStr = file.readStringUntil('\n');
        String capturedVolumeStr = file.readStringUntil('\n');
        String usePlantowerStr = file.readStringUntil('\n');
        file.close();

        session.startDate = DateTime(startStr.c_str());

        session.duration = durationStr.toInt();
        if (session.startDate < components.rtc.now())
        {
            unsigned long delta = components.rtc.now().unixtime() - session.startDate.unixtime();
            session.duration += delta;
            session.startDate = components.rtc.now();
            Serial.println("Duración de sesión ajustada por tiempo pasado desde inicio");
            Serial.print(" Nueva startDate: ");
            Serial.println(session.startDate.timestamp());
            Serial.print(" Nueva duration: ");
            Serial.print(session.duration);
            Serial.print(" (+ ");
            Serial.print(delta);
            Serial.println(")");
        }

        session.targetFlow = targetFlowStr.toInt();
        session.capturedVolume = capturedVolumeStr.toFloat();
        session.usePlantower = usePlantowerStr == "1";

        Serial.println("Sesión cargada desde SD:");
        Serial.print(" startDate: ");
        Serial.println(startStr);
        Serial.print(" duration: ");
        Serial.println(durationStr);
        Serial.print(" targetFlow: ");
        Serial.println(targetFlowStr);
        Serial.print(" capturedVolume: ");
        Serial.println(capturedVolumeStr);
        Serial.print(" usePlantower: ");
        Serial.println(usePlantowerStr);

        return true;
    }

    void clearSession()
    {
        String filename = String(eoloDir) + "/session.txt";
        if (SD.exists(filename.c_str()))
        {
            SD.remove(filename.c_str());
            delay(5);
            if (!SD.exists(filename.c_str()))
            {
                Serial.println("Archivo de sesión eliminado de SD");
            }
            else
            {
                Serial.println("No se pudo eliminar el archivo de sesión de SD");
            }
        }
    }

    void logData()
    {
        if (sdStatus == SD_ERROR)
        {
            Serial.println("Aviso: SD anteriormente falló!");
        }
        sdStatus = SD_WRITING;

        String dateStr = session.startDate.timestamp();
        dateStr.replace(":", "_");
        dateStr.replace("-", "_");

        String filename = String(logsDir) + "/log_" + dateStr + ".csv";
        bool fileExists = SD.exists(filename.c_str());

        if (!fileExists)
        {
            File file = SD.open(filename.c_str(), "w+");
            if (!file)
            {
                sdStatus = SD_ERROR;
                Serial.println("No se pudo abrir el archivo para escribir/crear");
                return;
            }

            file.println("time,flow,flow_target,temperature,humidity,pressure,pm1,pm25,pm10,battery_pct");
            file.close();

            Serial.println("Archivo de log creado: " + filename);
        }
        else
        {
            Serial.println("Archivo de log ya existe: " + filename);

            File file = SD.open(filename.c_str(), "a+");
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

            Serial.println("Datos registrados en SD: ");
            Serial.print(" Time: ");
            Serial.println(components.rtc.now().timestamp());
            Serial.print(" Flow: ");
            Serial.println(components.flowSensor.flow);
            Serial.print(" Flow_target: ");
            Serial.println(session.targetFlow);
            Serial.print(" Temp: ");
            Serial.println(components.bme.temperature);
            Serial.print(" Hum: ");
            Serial.println(components.bme.humidity);
            Serial.print(" Pres: ");
            Serial.println(components.bme.pressure);
            Serial.print(" PM1: ");
            Serial.println(components.plantower.pm1);
            Serial.print(" PM2.5: ");
            Serial.println(components.plantower.pm25);
            Serial.print(" PM10: ");
            Serial.println(components.plantower.pm10);
            Serial.print(" Battery: ");
            Serial.println(components.battery.getPct());

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
        {
            setDisplayPower(true);
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
        DateTime endTime = DateTime(ctx.session.startDate.unixtime() + ctx.session.duration);
        ctx.session.elapsedTime = ctx.session.duration - (endTime.unixtime() - now);
        if (now >= endTime.unixtime())
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