#ifndef CONTEXT_H
#define CONTEXT_H

#include "Session.h"
#include "Components.h"
#include "../Config.h"
#include "../Drawing/SceneManager.h"
#include "ESPJob.h"
#include <SD.h>
#include <Preferences.h>
#include "Profiler.h"

enum SDStatus
{
    SD_OK,
    SD_WRITING,
    SD_ERROR
};

typedef struct Context
{
    static Context *instance;
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
    bool isSdReady = false;

    const char *eoloDir = "/EOLO";
    const char *logsDir = "/EOLO/logs";

    // Datos de calibración
    static const int MAX_CAL_POINTS = 200;
    int numCalPoints = 0;
    float calMotorPcts[MAX_CAL_POINTS];
    float calFlows[MAX_CAL_POINTS];
    bool isCalibrationLoaded = false;

    Preferences preferences;

public:
    Context(DisplayModel &display) : u8g2(display) {}

    void begin()
    {
        Profiler p("Context begin");
        instance = this;

        bool grande = false;
#ifdef EOLO_GRANDE
        grande = true;
#endif
        const char *versionType = grande ? "Standard" : "Express";
        LOG_F("Iniciando EOLO %s\n", versionType);

        components.motor.begin();
        components.motor.setPWM(0); // Asegurar que los motores estén apagados

        LOG_LN("Iniciando pantalla...");
        delay(50);
        digitalWrite(PPH_PWR_PIN, HIGH);
        initDisplay();
        LOG_LN("Pantalla iniciada");
        LOG_LN("Inicializando contexto...");
        components.begin();

        initDisplay();
        delay(50);

        loadCalibration(); // ya no depende de SD
        initSD();          // solo para logs
    }

    void initDisplay()
    {
        bool began = u8g2.begin();
        if (!began)
        {
            LOG_LN("Fallo al iniciar pantalla");
            return;
        }
        u8g2.setBusClock(I2C_CLOCK); // (fix pantalla para que la señal sea más cuadrada)
        u8g2.clearBuffer();
        u8g2.setFont(FONT_BOLD_L);
        int textWidth = u8g2.getStrWidth("EOLO");
        int x = (u8g2.getDisplayWidth() - textWidth) / 2;
        int y = (u8g2.getDisplayHeight() / 2) + (u8g2.getAscent() / 2);
        u8g2.drawStr(x, y, "EOLO");
        u8g2.sendBuffer();
        isDisplayReady = true;
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

    /* TODO - cambiar SD.h por SdFat.h para que el tiempo de los logs esté bien
    static void dateTime(uint16_t *date, uint16_t *time)
    {
        DateTime now = Context::instance->components.rtc.now();
        *date = FAT_DATE(now.year(), now.month(), now.day());
        *time = FAT_TIME(now.hour(), now.minute(), now.second());
    }
    */

    bool initSD()
    {
        if (isSdReady)
            return true;
        // SdFile::dateTimeCallback(dateTime);

        if (!SD.begin(SD_CS_PIN))
        {
            LOG_LN("Fallo al inicializar SD");
            sdStatus = SD_ERROR;
            isSdReady = false;
            return false;
        }
        isSdReady = true;
        sdcard_type_t sdType = SD.cardType();
        if (sdType == CARD_NONE)
        {
            LOG_LN("No se detectó tarjeta SD");
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
            LOG_LN("Directorio /EOLO creado en SD");
            sdStatus = SD_OK;
        }
        else
        {
            LOG_LN("Directorio /EOLO ya existe en SD");
        }

        if (!SD.exists(logsDir))
        {
            sdStatus = SD_WRITING;
            SD.mkdir(logsDir);
            LOG_LN("Directorio /EOLO/logs creado en SD");
            sdStatus = SD_OK;
        }
        else
        {
            LOG_LN("Directorio /EOLO/logs ya existe en SD");
        }

        LOG_LN("SD inicializada");
        sdStatus = SD_OK;
        return true;
    }

    void saveSession()
    {
        LOG_LN("Guardando sesión en Flash...");
        preferences.begin("eolo_session", false);

        preferences.putULong("startDate", session.startDate.unixtime());
        preferences.putUInt("duration", session.duration);
        preferences.putULong("elapsedTime", session.elapsedTime);
        preferences.putFloat("targetFlow", session.targetFlow);
        preferences.putFloat("capturedVol", session.capturedVolume);
        preferences.putBool("usePlantower", session.usePlantower);

        preferences.end();

        LOG_LN("Sesión guardada en Flash:");
        Serial.print(" startDate: ");
        Serial.println(session.startDate.timestamp());
        Serial.print(" duration: ");
        Serial.println(session.duration);
        Serial.print(" elapsedTime: ");
        Serial.println(session.elapsedTime);
        Serial.print(" targetFlow: ");
        Serial.println(session.targetFlow);
        Serial.print(" capturedVolume: ");
        Serial.println(session.capturedVolume);
        Serial.print(" usePlantower: ");
        LOG_LN(session.usePlantower);
    }

    bool loadSession()
    {
        preferences.begin("eolo_session", false);

        if (!preferences.isKey("startDate"))
        {
            preferences.end();
            LOG_LN("No se encontró sesión guardada en Flash");
            return false;
        }

        uint32_t startUnix = preferences.getULong("startDate", 0);
        uint32_t durationVal = preferences.getUInt("duration", 0);
        unsigned long elapsedTime = preferences.getULong("elapsedTime", 0);
        float targetFlowVal = preferences.getFloat("targetFlow", 5.0f);
        float capturedVolumeVal = preferences.getFloat("capturedVol", 0.0f);
        bool usePlantowerVal = preferences.getBool("usePlantower", true);

        preferences.end();

        DateTime loadedStart = DateTime(startUnix);
        session.startDate = loadedStart;
        session.duration = durationVal;
        session.elapsedTime = 0;

        if (session.startDate < components.rtc.now())
        {
            session.startDate = components.rtc.now();
            session.duration = durationVal - elapsedTime;
            session.elapsedTime = 0;

            Serial.print("Tiempo transcurrido restaurado: ");
            Serial.print(elapsedTime);
            Serial.println("s");
        }

        session.targetFlow = targetFlowVal;
        session.capturedVolume = capturedVolumeVal;
        session.usePlantower = usePlantowerVal;

        LOG_LN("Sesión cargada desde Flash:");
        Serial.print(" startDate: ");
        Serial.println(session.startDate.timestamp());
        Serial.print(" duration: ");
        Serial.println(session.duration);
        Serial.print(" elapsedTime: ");
        Serial.println(elapsedTime);
        Serial.print(" targetFlow: ");
        Serial.println(session.targetFlow);
        Serial.print(" capturedVolume: ");
        Serial.println(capturedVolumeVal);
        Serial.print(" usePlantower: ");
        Serial.println(session.usePlantower);

        saveSession();
        return true;
    }

    bool canLoadSession()
    {
        preferences.begin("eolo_session", false);
        bool hasSession = preferences.isKey("startDate");
        preferences.end();
        return hasSession;
    }

    void clearSession()
    {
        preferences.begin("eolo_session", false);
        preferences.clear();
        preferences.end();
        LOG_LN("Sesión eliminada de Flash");
    }

    void saveCalibration()
    {
        preferences.begin("eolo_calib", false);

        preferences.putInt("numPoints", numCalPoints);
        preferences.putBytes("motorPcts", calMotorPcts, numCalPoints * sizeof(float));
        preferences.putBytes("flows", calFlows, numCalPoints * sizeof(float));

        preferences.end();

        LOG_LN("Calibración guardada en Flash:");
        Serial.print(" Número de puntos: ");
        Serial.println(numCalPoints);
        if (numCalPoints > 0)
        {
            Serial.print(" Rango de flujo: ");
            Serial.print(calFlows[0], 2);
            Serial.print(" - ");
            Serial.print(calFlows[numCalPoints - 1], 2);
            Serial.println(" L/min");
        }
    }

    bool loadCalibration()
    {
        preferences.begin("eolo_calib", false);

        if (!preferences.isKey("numPoints"))
        {
            preferences.end();
            LOG_LN("No hay calibración guardada en Flash");
            isCalibrationLoaded = false;
            return false;
        }

        numCalPoints = preferences.getInt("numPoints", 0);

        if (numCalPoints <= 0 || numCalPoints > MAX_CAL_POINTS)
        {
            preferences.end();
            LOG_LN("Calibración inválida en Flash");
            isCalibrationLoaded = false;
            numCalPoints = 0;
            return false;
        }

        size_t motorBytes = preferences.getBytes("motorPcts", calMotorPcts, numCalPoints * sizeof(float));
        size_t flowBytes = preferences.getBytes("flows", calFlows, numCalPoints * sizeof(float));

        preferences.end();

        if (motorBytes != numCalPoints * sizeof(float) || flowBytes != numCalPoints * sizeof(float))
        {
            LOG_LN("Error al leer calibración desde Flash");
            isCalibrationLoaded = false;
            numCalPoints = 0;
            return false;
        }

        isCalibrationLoaded = true;
        LOG_LN("Calibración cargada desde Flash:");
        Serial.print(" Número de puntos: ");
        Serial.println(numCalPoints);
        if (numCalPoints > 0)
        {
            Serial.print(" Rango de flujo: ");
            Serial.print(calFlows[0], 2);
            Serial.print(" - ");
            Serial.print(calFlows[numCalPoints - 1], 2);
            Serial.println(" L/min");

            if (calFlows[0] == 0 && calFlows[numCalPoints - 1] == 0)
            {
                LOG_LN("Calibración inválida detectada (rango 0-0). Ejecutando nueva calibración...");
                isCalibrationLoaded = false;
                return false;
            }
        }

        return true;
    }

    void testCalibration()
    {
        Context &ctx = *this;

        Serial.println("\n========================================");
        Serial.println("DIAGNÓSTICO DE CALIBRACIÓN");
        Serial.println("========================================");

        if (!ctx.isCalibrationLoaded || ctx.numCalPoints == 0)
        {
            LOG_LN("ERROR: No hay calibración cargada!");
            delay(3000);
            return;
        }

        Serial.print("Puntos de calibración: ");
        Serial.println(ctx.numCalPoints);
        Serial.println("\nTabla completa Motor% -> Flujo:");
        Serial.println("Motor%  | Flujo(L/min)");
        Serial.println("--------+-------------");
        for (int i = 0; i < ctx.numCalPoints; i++)
        {
            Serial.print(ctx.calMotorPcts[i], 1);
            Serial.print("%");
            if (ctx.calMotorPcts[i] < 10)
                Serial.print("  ");
            else if (ctx.calMotorPcts[i] < 100)
                Serial.print(" ");
            Serial.print("     | ");
            Serial.println(ctx.calFlows[i], 2);
        }

        Serial.println("\n========================================");
        Serial.println("PRUEBA DE INTERPOLACIÓN");
        Serial.println("========================================");

        // Probar todo el rango de flujo calibrado
        float minFlow = ctx.calFlows[0];
        float maxFlow = ctx.calFlows[ctx.numCalPoints - 1];
        int numTests = 10;
        float step = (maxFlow - minFlow) / (numTests - 1);

        Serial.println("Flujo Obj. | Motor% Calc. | Esperado");
        Serial.println("-----------+--------------+---------");
        for (int i = 0; i < numTests; i++)
        {
            float targetFlow = minFlow + i * step;
            float calculatedPct = ctx.getTargetMotorPct(targetFlow);

            Serial.print(targetFlow, 2);
            Serial.print(" L/min");
            if (targetFlow < 10)
                Serial.print("  ");
            Serial.print(" | ");
            Serial.print(calculatedPct, 1);
            Serial.print("%");
            if (calculatedPct < 10)
                Serial.print("  ");
            else if (calculatedPct < 100)
                Serial.print(" ");
            Serial.print("       | ");

            // Buscar en la tabla el motor% esperado
            bool found = false;
            for (int j = 0; j < ctx.numCalPoints - 1; j++)
            {
                if (targetFlow >= ctx.calFlows[j] && targetFlow <= ctx.calFlows[j + 1])
                {
                    float ratio = (targetFlow - ctx.calFlows[j]) / (ctx.calFlows[j + 1] - ctx.calFlows[j]);
                    float expectedPct = ctx.calMotorPcts[j] + ratio * (ctx.calMotorPcts[j + 1] - ctx.calMotorPcts[j]);
                    Serial.print(expectedPct, 1);
                    Serial.print("%");
                    found = true;
                    break;
                }
            }
            if (!found)
                Serial.print("fuera rango");
            Serial.println();
        }

        Serial.println("\n========================================");
        Serial.println("PRUEBA EN TIEMPO REAL");
        Serial.println("========================================");
        Serial.println("Probando todo el rango de flujo calibrado...");

        for (int i = 0; i < numTests; i++)
        {
            float targetFlow = minFlow + i * step;
            float calculatedPct = ctx.getTargetMotorPct(targetFlow);

            Serial.print("\nAplicando motor al ");
            Serial.print(calculatedPct, 1);
            Serial.print("% para ");
            Serial.print(targetFlow, 2);
            Serial.println(" L/min");

            ctx.components.motor.setPowerPct(calculatedPct);

            // Limpiar buffer del sensor
            for (int j = 0; j < 20; j++)
            {
                ;
                delay(100);
            }

            Serial.println("Tiempo | Flujo Real | Diferencia");
            Serial.println("-------+------------+-----------");
            for (int t = 0; t < 5; t++)
            {
                FlowData flowData;
                if (!ctx.components.flowSensor.getData(flowData) || !flowData.valid)
                {
                    Serial.println("Error al leer sensor de flujo");
                    continue;
                }
                float realFlow = flowData.flow;

                float diff = realFlow - targetFlow;

                Serial.print(t);
                Serial.print("s     | ");
                Serial.print(realFlow, 2);
                Serial.print(" L/min | ");
                if (diff >= 0)
                    Serial.print("+");
                Serial.print(diff, 2);
                Serial.println(" L/min");

                delay(1000);
            }
        }

        ctx.components.motor.setPowerPct(0);

        Serial.println("\n========================================");
        Serial.println("Diagnóstico completado");
        Serial.println("Presiona el botón para volver");
        Serial.println("========================================\n");

        // Esperar a que se presione el botón
        while (!ctx.components.input.isButtonPressed())
        {
            ctx.components.input.poll();
            delay(100);
        }
        ctx.components.input.resetCounter();
    }

    float getTargetMotorPct(float targetFlow)
    {
        if (!isCalibrationLoaded || numCalPoints == 0)
        {
            // Fallback
            LOG_LN("Advertencia: No hay calibración cargada.");
            return 0;
        }

        // Si el objetivo está por debajo del flujo calibrado mínimo
        if (targetFlow <= calFlows[0])
            return calMotorPcts[0];

        // Si el objetivo está por encima del flujo calibrado máximo
        if (targetFlow >= calFlows[numCalPoints - 1])
            return calMotorPcts[numCalPoints - 1];

        // Buscar el rango calibrado e interpolar
        for (int i = 0; i < numCalPoints - 1; i++)
        {
            float f0 = calFlows[i];
            float f1 = calFlows[i + 1];
            float m0 = calMotorPcts[i];
            float m1 = calMotorPcts[i + 1];

            if (targetFlow >= f0 && targetFlow <= f1)
            {
                // Interpolación lineal entre puntos (proteger división por cero)
                float denom = (f1 - f0);
                if (fabs(denom) < 1e-6f)
                {
                    // puntos de flujo idénticos, devolver promedio de potencias
                    return (m0 + m1) / 2.0f;
                }
                float t = (targetFlow - f0) / denom;
                return m0 + t * (m1 - m0);
            }
        }

        // Si no se encuentra, devolver el último valor
        return calMotorPcts[numCalPoints - 1];
    }

    void logData()
    {
        LOG_LN("Iniciando log de datos en SD...");
        Profiler p("Context logData");

        if (sdStatus == SD_ERROR)
        {
            LOG_LN("Aviso: SD anteriormente falló!");
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
                LOG_LN("No se pudo abrir el archivo para escribir/crear");
                return;
            }

#ifdef EOLO_GRANDE
            file.println("time,flow,flow_target,temperature,humidity,pressure,pm1,pm25,pm10,wind_speed,wind_direction,battery_pct");
#else
            file.println("time,flow,flow_target,temperature,humidity,pressure,pm1,pm25,pm10,battery_pct");
#endif
            file.close();

            LOG_LN("Archivo de log creado: " + filename);
        }
        else
        {
            LOG_LN("Archivo de log ya existe: " + filename);

            File file = SD.open(filename.c_str(), "a+");
            if (!file)
            {
                sdStatus = SD_ERROR;
                LOG_LN("No se pudo abrir el archivo para escribir");
                return;
            }

            // time
            file.print(components.rtc.now().timestamp());
            file.print(",");

            // flow
            FlowData flowData;
            if (!components.flowSensor.getData(flowData) || !flowData.valid)
            {
                flowData.flow = -1.0;
            }
            file.print(flowData.flow);
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

            PlantowerData ptowerData;
            bool ptOk = components.plantower.getData(ptowerData);

            // pm1
            file.print(ptowerData.pm1_0);
            file.print(",");

            // pm25
            file.print(ptowerData.pm2_5);
            file.print(",");

            // pm10
            file.print(ptowerData.pm10_0);
            file.print(",");

#ifdef EOLO_GRANDE

            AnemometerData anemoData;
            if (!components.anemometer.getData(anemoData) || !anemoData.valid)
            {
                LOG_LN("Error al leer anemómetro para log");
            }
            // wind_speed
            file.print(anemoData.speed);
            file.print(",");

            // wind_direction
            file.print(anemoData.direction);
            file.print(",");

#endif

            // battery_pct
            file.print(components.battery.getPct());
            file.println();

            LOG_LN("Datos registrados en SD: ");
            Serial.print(" Time: ");
            Serial.println(components.rtc.now().timestamp());
            Serial.print(" Flow: ");
            Serial.println(flowData.flow);
            Serial.print(" Flow_target: ");
            Serial.println(session.targetFlow);
            Serial.print(" Temp: ");
            Serial.println(components.bme.temperature);
            Serial.print(" Hum: ");
            Serial.println(components.bme.humidity);
            Serial.print(" Pres: ");
            Serial.println(components.bme.pressure);
            Serial.print(" PM1: ");
            Serial.println(ptowerData.pm1_0);
            Serial.print(" PM2.5: ");
            Serial.println(ptowerData.pm2_5);
            Serial.print(" PM10: ");
            Serial.println(ptowerData.pm10_0);
            Serial.print(" Battery: ");
            Serial.println(components.battery.getPct());
            file.close();

            LOG_LN("Archivo de log escrito!");
        }
        sdStatus = SD_OK;
        LOG_LN("Log completado con éxito.");
        saveSession();
#ifdef EOLO_GRANDE
        uploadData();
#endif
    }

    void uploadData()
    {
        Profiler p("Context uploadData");
        components.api.begin();
        AnemometerData anemoData;
        bool anemoOk = components.anemometer.getData(anemoData);
        components.api.addData(748, 5, anemoData.speed);      // velocidad del viento
        components.api.addData(748, 17, anemoData.direction); // direccion del viento
        components.api.addData(749, 3, -69);                  // temperatura (deberia ser del Plantower)
        components.api.addData(749, 6, -420);                 // humedad (deberia ser del Plantower)
        PlantowerData ptowerData;
        bool ptOk = components.plantower.getData(ptowerData);
        components.api.addData(749, 7, ptowerData.pm1_0);           // PM1.0
        components.api.addData(749, 8, ptowerData.pm2_5);           // PM2.5
        components.api.addData(749, 9, ptowerData.pm10_0);          // PM10
        components.api.addData(750, 3, components.bme.temperature); // temperatura
        components.api.addData(750, 6, components.bme.humidity);    // humedad
        components.api.addData(750, 13, components.bme.pressure);   // presion
        components.api.addData(751, 48, session.targetFlow);        // flujo objetivo
        components.api.addData(751, 49, session.capturedVolume);    // volumen capturado
        FlowData flowData;
        if (!components.flowSensor.getData(flowData) || !flowData.valid)
        {
            flowData.flow = -1.0;
        }
        components.api.addData(751, 50, flowData.flow);                  // flujo
        components.api.addData(752, 3, components.bme.temperature);      // temperatura
        components.api.addData(753, 4, components.battery.getVoltage()); // voltaje batería
        // GPS (no hay GPS aun asi que todo -1)
        components.api.addData(754, 11, -1); // latitud
        components.api.addData(754, 12, -1); // longitud
        components.api.addData(754, 15, -1); // intensidad de señal
        components.api.addData(754, 45, -1); // velocidad
        components.api.addData(754, 46, -1); // satélites
        components.api.send();
    }

    void update()
    {
        Profiler p("Context update");
        components.input.poll();
        components.poll();

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
        LOG_LN("Iniciando captura...");
        SceneManager::setScene("captura", *this);
        enableDisplay();
    }

    unsigned long int pauseTime = 0;
    void pauseCapture()
    {
        components.input.resetCounter();
        if (!isCapturing || isPaused)
            return;
        LOG_LN("Pausando captura...");
        isPaused = true;
        pauseTime = getUnixTime();
    }

    void resumeCapture()
    {
        components.input.resetCounter();
        if (!isCapturing || !isPaused)
            return;
        LOG_LN("Resumiendo captura...");
        isPaused = false;
        unsigned long now = getUnixTime();
        unsigned long pauseDelta = now - pauseTime;
        session.duration += pauseDelta;
    }

    void endCapture()
    {
        isCapturing = false;
        isEnd = true;

        LOG_LN("Captura finalizada.");
        components.motor.setPowerPct(0);
        components.input.resetCounter();
        SceneManager::setScene("end", *this);
        enableDisplay();
    }

    void resetCapture()
    {
        isCapturing = false;
        isPaused = false;
        session = Session();
        components.motor.setPowerPct(0);
        components.input.resetCounter();
        session.elapsedTime = 0;
        LOG_LN("Estado de captura reiniciado.");
    }

    void updateMotors()
    {
        static float lastTargetFlow = -1.0f;

        if (isCalibrationLoaded && numCalPoints > 0)
        {
            float targetMotorPct = getTargetMotorPct(session.targetFlow);

            if (lastTargetFlow != session.targetFlow)
            {
                Serial.print("Flujo objetivo: ");
                Serial.print(session.targetFlow, 2);
                Serial.print(" L/min -> Motor calculado: ");
                Serial.print(targetMotorPct, 2);
                Serial.println("%");
                lastTargetFlow = session.targetFlow;
            }

            targetMotorPct = constrain(targetMotorPct, 0.0f, 100.0f);
            components.motor.setPowerPct(targetMotorPct);
        }
        else
        {
            components.motor.setPowerPct(0);
        }
    }

    void updateCapture()
    {
        // LOG_LN("Actualizando estado de captura...");
        Context &ctx = *this;

        if (!ctx.isCapturing || ctx.isPaused)
            return;

        unsigned long now = ctx.getUnixTime();
        DateTime endTime = DateTime(ctx.session.startDate.unixtime() + ctx.session.duration);
        ctx.session.elapsedTime = ctx.session.duration - (endTime.unixtime() - now);
        if (now >= endTime.unixtime())
        {
            LOG_LN("Duración de captura alcanzada.");
            Serial.print("Tiempo transcurrido: ");
            Serial.println(ctx.session.elapsedTime);

            Serial.print("Duración establecida: ");
            Serial.println(ctx.session.duration);

            endCapture();
            return;
        }

#if !BAREBONES
        // LOG_LN("Actualizando motores...");
        updateMotors();
#else
        ctx.components.flowSensor.flow = ctx.session.targetFlow + millis() % 2;
#endif
        if (now - ctx.session.lastLog >= ctx.CAPTURE_INTERVAL)
        {
            ctx.session.lastLog = now;
            LOG_LN("Leyendo datos de sensores...");
            LOG_LN("Leyendo flujo...");
            ;

#if !BAREBONES
            LOG_LN("Leyendo BME280...");
            ctx.components.bme.readData();
            FlowData flowData;
            if (ctx.components.flowSensor.getData(flowData))
            {
                ctx.session.capturedVolume += (flowData.flow / 60.0f) * (ctx.CAPTURE_INTERVAL);
            }
            else
            {
                LOG_LN("Error al leer sensor de flujo para volumen capturado");
            }
#endif
#if true

#endif
            LOG_LN("Registrando datos...");
            logData();
        }
    }

} Context;

inline Context *Context::instance = nullptr;
#endif