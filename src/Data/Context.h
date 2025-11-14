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

public:
    Context(DisplayModel &display) : u8g2(display) {}

    void begin()
    {
        instance = this;

        bool grande = false;
        #ifdef EOLO_GRANDE
            grande = true;
        #endif
        Serial.print("EOLO");
        if (grande)
        {
            Serial.println(" (grande)");
        }
        else
        {
            Serial.println(" (chico)");
        }
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
        loadCalibration();
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
            Serial.println("Fallo al inicializar SD");
            sdStatus = SD_ERROR;
            isSdReady = false;
            return false;
        }
        isSdReady = true;
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
        if (!isSdReady)
        {
            initSD();
        }
        String filename = String(eoloDir) + "/session.txt";

        String startStr = session.startDate.timestamp();
        String durationStr = String(session.duration);
        String elapsedStr = String(session.elapsedTime);
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
        file.println(elapsedStr);
        file.println(targetFlowStr);
        file.println(capturedVolumeStr);
        file.println(usePlantowerStr);
        file.close();

        Serial.println("Sesión guardada en SD:");
        Serial.print(" startDate: ");
        Serial.println(startStr);
        Serial.print(" duration: ");
        Serial.println(durationStr);
        Serial.print(" elapsedTime: ");
        Serial.println(elapsedStr);
        Serial.print(" targetFlow: ");
        Serial.println(targetFlowStr);
        Serial.print(" capturedVolume: ");
        Serial.println(capturedVolumeStr);
        Serial.print(" usePlantower: ");
        Serial.println(usePlantowerStr);
    }

    bool loadSession()
    {
        if (!isSdReady)
        {
            initSD();
        }
        String filename = String(eoloDir) + "/session.txt";

        File file = SD.open(filename.c_str(), "r+");
        if (!file)
        {
            Serial.println("No se pudo abrir el archivo de sesión para leer");
            return false;
        }

        String startStr = file.readStringUntil('\n');
        String durationStr = file.readStringUntil('\n');
        String elapsedStr = file.readStringUntil('\n');
        String targetFlowStr = file.readStringUntil('\n');
        String capturedVolumeStr = file.readStringUntil('\n');
        String usePlantowerStr = file.readStringUntil('\n');
        file.close();

        long elapsedTime = elapsedStr.toInt();

        DateTime loadedStart = DateTime(startStr.c_str());
        session.startDate = loadedStart;
        session.duration = durationStr.toInt();
        session.elapsedTime = 0;

        if (session.startDate < components.rtc.now()) // reajusta tiempo solo si la fecha de inicio ya pasó
        {
            session.startDate = components.rtc.now();
            session.duration = durationStr.toInt();
            session.duration -= elapsedTime;
            session.elapsedTime = 0;

            Serial.print("Tiempo transcurrido restaurado: ");
            Serial.print(elapsedTime);
            Serial.println("s");
        }

            // Use toFloat() to preserve fractional flow values (e.g., 5.2 L/min)
            session.targetFlow = targetFlowStr.toFloat();
        session.capturedVolume = capturedVolumeStr.toFloat();
        session.usePlantower = usePlantowerStr == "1";

        Serial.println("Sesión cargada desde SD:");
        Serial.print(" startDate: ");
        Serial.println(startStr);
        Serial.print(" duration: ");
        Serial.println(durationStr);
        Serial.print(" elapsedTime: ");
        Serial.println(elapsedStr);
        Serial.print(" targetFlow: ");
        Serial.println(targetFlowStr);
        Serial.print(" capturedVolume: ");
        Serial.println(capturedVolumeStr);
        Serial.print(" usePlantower: ");
        Serial.println(usePlantowerStr);

        saveSession();
        return true;
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

    void saveCalibration()
    {
        if (!isSdReady)
        {
            initSD();
        }
        String filename = String(eoloDir) + "/calibracion.csv";
        if (SD.exists(filename.c_str()))
        {
            SD.remove(filename.c_str());
            delay(5); 
        }

        File file = SD.open(filename.c_str(), "w+");
        if (!file)
        {
            Serial.println("No se pudo crear el archivo de calibración para escribir");
            return;
        }



        file.println("motor,flujo");
        for (int i = 0; i < numCalPoints; i++)
        {
            file.print(calMotorPcts[i], 2);
            file.print(",");
            file.println(calFlows[i], 2);
        }
        file.close();

        Serial.println("Calibración guardada en SD:");
        Serial.print(" Número de puntos: ");
        Serial.println(numCalPoints);
        Serial.print(" Rango de flujo: ");
        Serial.print(calFlows[0], 2);
        Serial.print(" - ");
        Serial.print(calFlows[numCalPoints - 1], 2);
        Serial.println(" L/min");
    }

    bool loadCalibration()
    {
        if (!isSdReady)
        {
            initSD();
        }

        if(!isSdReady){ // si tras reintentar el initSD sigue sin estar lista, se asume un fallo y no se carga la calib.
            return false;
        }
        String filename = String(eoloDir) + "/calibracion.csv";

        if (!SD.exists(filename.c_str()))
        {
            Serial.println("Archivo de calibración no existe");
            isCalibrationLoaded = false;
            return false;
        }

        File file = SD.open(filename.c_str(), "r+");
        if (!file)
        {
            Serial.println("No se pudo abrir el archivo de calibración para leer");
            isCalibrationLoaded = false;
            return false;
        }

        // Omitir header
        file.readStringUntil('\n');

        numCalPoints = 0;
        while (file.available() && numCalPoints < MAX_CAL_POINTS)
        {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.length() == 0)
                break;

            int commaIndex = line.indexOf(',');
            if (commaIndex == -1)
                continue;

            String motorStr = line.substring(0, commaIndex);
            String flowStr = line.substring(commaIndex + 1);

            calMotorPcts[numCalPoints] = motorStr.toFloat();
            calFlows[numCalPoints] = flowStr.toFloat();
            numCalPoints++;
        }
        file.close();

            // Ordenar los puntos por flujo ascendente por si el CSV estaba desordenado
            if (numCalPoints > 1)
            {
                for (int i = 0; i < numCalPoints - 1; i++)
                {
                    for (int j = 0; j < numCalPoints - i - 1; j++)
                    {
                        if (calFlows[j] > calFlows[j + 1])
                        {
                            float tf = calFlows[j];
                            calFlows[j] = calFlows[j + 1];
                            calFlows[j + 1] = tf;

                            float tm = calMotorPcts[j];
                            calMotorPcts[j] = calMotorPcts[j + 1];
                            calMotorPcts[j + 1] = tm;
                        }
                    }
                }
            }

            isCalibrationLoaded = true;
            Serial.println("Calibración cargada desde SD:");
            Serial.print(" Número de puntos: ");
            Serial.println(numCalPoints);
            if (numCalPoints > 0)
            {
                Serial.print(" Rango de flujo: ");
                Serial.print(calFlows[0], 2);
                Serial.print(" - ");
                Serial.print(calFlows[numCalPoints - 1], 2);
                Serial.println(" L/min");
                
                // Verificar si el rango es inválido (0 - 0)
                if (calFlows[0] == 0 && calFlows[numCalPoints - 1] == 0)
                {
                Serial.println("Calibración inválida detectada (rango 0-0). Ejecutando nueva calibración...");
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
        
        if(!ctx.isCalibrationLoaded || ctx.numCalPoints == 0)
        {
            Serial.println("ERROR: No hay calibración cargada!");
            delay(3000);
            return;
        }
        
        Serial.print("Puntos de calibración: ");
        Serial.println(ctx.numCalPoints);
        Serial.println("\nTabla completa Motor% -> Flujo:");
        Serial.println("Motor%  | Flujo(L/min)");
        Serial.println("--------+-------------");
        for(int i = 0; i < ctx.numCalPoints; i++)
        {
            Serial.print(ctx.calMotorPcts[i], 1);
            Serial.print("%");
            if(ctx.calMotorPcts[i] < 10) Serial.print("  ");
            else if(ctx.calMotorPcts[i] < 100) Serial.print(" ");
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
        for(int i = 0; i < numTests; i++)
        {
            float targetFlow = minFlow + i * step;
            float calculatedPct = ctx.getTargetMotorPct(targetFlow);
            
            Serial.print(targetFlow, 2);
            Serial.print(" L/min");
            if(targetFlow < 10) Serial.print("  ");
            Serial.print(" | ");
            Serial.print(calculatedPct, 1);
            Serial.print("%");
            if(calculatedPct < 10) Serial.print("  ");
            else if(calculatedPct < 100) Serial.print(" ");
            Serial.print("       | ");
            
            // Buscar en la tabla el motor% esperado
            bool found = false;
            for(int j = 0; j < ctx.numCalPoints - 1; j++)
            {
                if(targetFlow >= ctx.calFlows[j] && targetFlow <= ctx.calFlows[j+1])
                {
                    float ratio = (targetFlow - ctx.calFlows[j]) / (ctx.calFlows[j+1] - ctx.calFlows[j]);
                    float expectedPct = ctx.calMotorPcts[j] + ratio * (ctx.calMotorPcts[j+1] - ctx.calMotorPcts[j]);
                    Serial.print(expectedPct, 1);
                    Serial.print("%");
                    found = true;
                    break;
                }
            }
            if(!found) Serial.print("fuera rango");
            Serial.println();
        }
        
        Serial.println("\n========================================");
        Serial.println("PRUEBA EN TIEMPO REAL");
        Serial.println("========================================");
        Serial.println("Probando todo el rango de flujo calibrado...");

        for(int i = 0; i < numTests; i++)
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
            for(int j = 0; j < 20; j++)
            {
                ctx.components.flowSensor.readData();
                delay(100);
            }

            Serial.println("Tiempo | Flujo Real | Diferencia");
            Serial.println("-------+------------+-----------");
            for(int t = 0; t < 5; t++)
            {
                ctx.components.flowSensor.readData();
                float realFlow = ctx.components.flowSensor.flow;
                float diff = realFlow - targetFlow;
                
                Serial.print(t);
                Serial.print("s     | ");
                Serial.print(realFlow, 2);
                Serial.print(" L/min | ");
                if(diff >= 0) Serial.print("+");
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
        while(!ctx.components.input.isButtonPressed())
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
            Serial.println("Advertencia: No hay calibración cargada, usando mapeo lineal");
            return constrain(targetFlow * 12.5f, 0.0f, 100.0f); // Estimado aproximado: 8 L/min a 100%
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
        saveSession();
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
        Serial.println("Estado de captura reiniciado.");
    }

    void updateMotors()
    {
        static float lastTargetFlow = -1.0f;
        static float lastAppliedPct = -1.0f;

        if (isCalibrationLoaded && numCalPoints > 0)
        {
            // Use calibration-based control (suavizado/rampa)
            float targetMotorPct = getTargetMotorPct(session.targetFlow);

            // Inicializar lastAppliedPct con el valor actual si es la primera vez
            if (lastAppliedPct < 0.0f)
                lastAppliedPct = (float)components.motor.getPowerPct();

            // Print cuando cambia el objetivo para diagnóstico
            if (lastTargetFlow != session.targetFlow)
            {
                Serial.print("Flujo objetivo: ");
                Serial.print(session.targetFlow, 2);
                Serial.print(" L/min -> Motor calculado: ");
                Serial.print(targetMotorPct, 2);
                Serial.println("%");
                lastTargetFlow = session.targetFlow;
            }

            // Aplicar rampa para evitar cambios bruscos (max 2% por ciclo)
            float delta = targetMotorPct - lastAppliedPct;
            const float maxStep = 2.0f; // % por ciclo
            if (fabs(delta) > maxStep)
            {
                lastAppliedPct += (delta > 0.0f) ? maxStep : -maxStep;
            }
            else
            {
                lastAppliedPct = targetMotorPct;
            }

            // Asegurar rango
            lastAppliedPct = constrain(lastAppliedPct, 0.0f, 100.0f);
            components.motor.setPowerPct(lastAppliedPct);
        }
        else
        {
            // Fallback to PID-like control
            float currentFlow = components.flowSensor.flow;
            float targetFlow = session.targetFlow;
            float error = targetFlow - currentFlow;
            float newPct = (float)components.motor.getPowerPct() + error * 0.1f;
            newPct = constrain(newPct, 0.0f, 100.0f);
            components.motor.setPowerPct(newPct);
        }
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
            ctx.session.capturedVolume += (ctx.components.flowSensor.flow / 60.0f) * (ctx.CAPTURE_INTERVAL);
            logData();

#else
            Serial.println("Capturando datos... (modo barebones, solo serial)");
#endif
        }
    }

} Context;

inline Context *Context::instance = nullptr;
#endif