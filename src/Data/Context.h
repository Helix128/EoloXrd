#ifndef CONTEXT_H
#define CONTEXT_H

#include "Session.h"
#include "Components.h"
#include "CalibrationManager.h"
#include "ClockSettings.h"
#include "UiSnapshot.h"
#include "../Config.h"
#include "../Board/I2CBus.h"
#include "../Drawing/SceneManager.h"
#include "ESPJob.h"
#include <SD.h>
#include <Preferences.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "Profiler.h"

enum SDStatus
{
    SD_OK,
    SD_WRITING,
    SD_MISSING,
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
    bool sdInitAttempted = false;
    bool isSdReady = false;
    bool sdMissingLogReported = false;
    bool uploadPending = false;
    bool uploadActive = false;
    bool uiDirty = true;

    const char *eoloDir = "/EOLO";
    const char *logsDir = "/EOLO/logs";

    CalibrationManager calibration;
    ClockSettings clockSettings;

    Preferences preferences;
    UiSnapshot uiSnapshot;
    QueueHandle_t logQueue = nullptr;
    TaskHandle_t logTaskHandle = nullptr;
    TaskHandle_t bootTaskHandle = nullptr;
    bool bootInitComplete = false;
    bool bootInitRunning = false;
    const char *rtcAdjustReturnScene = "inicio";

    struct LogJob
    {
        uint32_t queuedAt;
    };

public:
    Context(DisplayModel &display) : u8g2(display) {}

    void begin()
    {
        PROFILE_SCOPE("context.begin");
        instance = this;

        // 1. Esperar a que los periféricos (ya encendidos en main.cpp) se estabilicen
        delay(100); 

        // 2. Inicializar bus I2C una sola vez para todo el sistema
        I2CBus::getInstance().begin();

        bool grande = false;
#ifdef FEATURE_MODEM
        grande = true;
#endif
        const char *versionType = grande ? "Standard" : "Express";
        LOG_F("Iniciando EOLO %s\n", versionType);

        // 3. Inicializar pantalla
        LOG_LN("Iniciando pantalla...");
        initDisplay();
        LOG_LN("Pantalla iniciada");

        // 4. Inicializar motores y componentes
        components.motor.begin();
        components.motor.setPwm(0);

        LOG_LN("Inicializando contexto...");
        components.begin();

        calibration.load();
        startLogTask();
        updateUiSnapshot(true);
        startBootInitTask();
    }

    static void bootInitWorker(void *arg)
    {
        Context *self = static_cast<Context *>(arg);
        self->bootInitRunning = true;
        self->syncRTCFromNtp();
        self->initSD();
        self->bootInitComplete = true;
        self->bootInitRunning = false;
        self->markUiDirty();
        self->bootTaskHandle = nullptr;
        vTaskDelete(nullptr);
    }

    void startBootInitTask()
    {
        if (bootTaskHandle != nullptr || bootInitComplete || bootInitRunning)
            return;

        xTaskCreatePinnedToCore(
            bootInitWorker,
            "EoloBootInit",
            8192,
            this,
            1,
            &bootTaskHandle,
            0);
    }

    bool syncRTCFromNtp()
    {
#ifdef FEATURE_MODEM
        RTCManager::NtpServer server = RTCManager::defaultNtpServer();
        LOG_F("Sincronizando RTC con NTP %s (%s)...\n", server.name, server.host);
        bool synced = false;

        if (!components.modem.begin())
        {
            LOG_LN("No se pudo iniciar modem para sincronizacion NTP");
            return false;
        }

        if (!components.modem.ensureConnected())
        {
            LOG_LN("No hay conexion de modem; se omite sincronizacion NTP");
            return false;
        }

        synced = components.rtc.syncNtp(components.modem, clockSettings, server);
        if (!synced)
            LOG_LN("Sincronizacion RTC fallida; continua booteo normal");
        return synced;
#else
        return false;
#endif
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
        if (sdInitAttempted)
            return isSdReady;
        sdInitAttempted = true;

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

        sdcard_type_t sdType = SD.cardType();
        if (sdType == CARD_NONE)
        {
            LOG_LN("No se detectó tarjeta SD");
            sdStatus = SD_MISSING;
            isSdReady = false;
            return false;
        }
        isSdReady = true;
        sdStatus = SD_OK;
        sdMissingLogReported = false;

        LOG_OUT("Tipo de tarjeta SD: ");
        switch (sdType)
        {
        case CARD_MMC:
            LOG_OUT_LN("MMC");
            break;
        case CARD_SD:
            LOG_OUT_LN("SDSC");
            break;
        case CARD_SDHC:
            LOG_OUT_LN("SDHC");
            break;
        case CARD_UNKNOWN:
        default:
            LOG_OUT_LN("Desconocido");
            break;
        }

        uint64_t sdCardSize = SD.cardSize();
        LOG_OUT("Tamaño de la tarjeta SD: ");
        LOG_OUT(sdCardSize / (1024 * 1024));
        LOG_OUT_LN(" MB");

        uint64_t totalBytes = SD.totalBytes();
        uint64_t usedBytes = SD.usedBytes();
        LOG_OUT("Espacio total: ");
        LOG_OUT(totalBytes / (1024 * 1024));
        LOG_OUT_LN(" MB");
        LOG_OUT("Espacio usado: ");
        LOG_OUT(usedBytes / (1024 * 1024));
        LOG_OUT(" MB (");
        LOG_OUT(totalBytes > 0 ? (usedBytes * 100) / totalBytes : 0);
        LOG_OUT_LN("%)");

        if (!SD.exists(eoloDir))
        {
            sdStatus = SD_WRITING;
            if (!SD.mkdir(eoloDir))
            {
                LOG_LN("No se pudo crear directorio /EOLO en SD");
                markSdFailed();
                return false;
            }
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
            if (!SD.mkdir(logsDir))
            {
                LOG_LN("No se pudo crear directorio /EOLO/logs en SD");
                markSdFailed();
                return false;
            }
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

    void markSdFailed()
    {
        sdStatus = SD_ERROR;
        isSdReady = false;
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
        LOG_OUT(" startDate: ");
        LOG_OUT_LN(session.startDate.timestamp());
        LOG_OUT(" duration: ");
        LOG_OUT_LN(session.duration);
        LOG_OUT(" elapsedTime: ");
        LOG_OUT_LN(session.elapsedTime);
        LOG_OUT(" targetFlow: ");
        LOG_OUT_LN(session.targetFlow);
        LOG_OUT(" capturedVolume: ");
        LOG_OUT_LN(session.capturedVolume);
        LOG_OUT(" usePlantower: ");
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

            LOG_OUT("Tiempo transcurrido restaurado: ");
            LOG_OUT(elapsedTime);
            LOG_OUT_LN("s");
        }

        session.targetFlow = targetFlowVal;
        session.capturedVolume = capturedVolumeVal;
        session.usePlantower = usePlantowerVal;

        LOG_LN("Sesión cargada desde Flash:");
        LOG_OUT(" startDate: ");
        LOG_OUT_LN(session.startDate.timestamp());
        LOG_OUT(" duration: ");
        LOG_OUT_LN(session.duration);
        LOG_OUT(" elapsedTime: ");
        LOG_OUT_LN(elapsedTime);
        LOG_OUT(" targetFlow: ");
        LOG_OUT_LN(session.targetFlow);
        LOG_OUT(" capturedVolume: ");
        LOG_OUT_LN(capturedVolumeVal);
        LOG_OUT(" usePlantower: ");
        LOG_OUT_LN(session.usePlantower);

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

    void markUiDirty()
    {
        uiDirty = true;
    }

    const UiSnapshot &getUiSnapshot() const
    {
        return uiSnapshot;
    }

    bool updateUiSnapshot(bool force = false)
    {
        UiSnapshot previous = uiSnapshot;

        uiSnapshot.flow.enabled = true;
        uiSnapshot.flow.targetFlow = session.targetFlow;
        uiSnapshot.flow.capturedVolume = session.capturedVolume;
        FlowData flowData;
        if (components.flowSensor.getData(flowData) && flowData.valid)
        {
            uiSnapshot.flow.valid = true;
            uiSnapshot.flow.flow = flowData.flow;
            uiSnapshot.flow.velocity = flowData.velocity;
        }
        else
        {
            uiSnapshot.flow.valid = false;
            uiSnapshot.flow.flow = -1.0f;
            uiSnapshot.flow.velocity = 0.0f;
        }

        uiSnapshot.environment.temperature = components.bme.temperature;
        uiSnapshot.environment.humidity = components.bme.humidity;
        uiSnapshot.environment.pressure = components.bme.pressure;

        uiSnapshot.airQuality.enabled = session.usePlantower;
        PlantowerData ptowerData;
        if (session.usePlantower && components.plantower.getData(ptowerData) && ptowerData.valid)
        {
            uiSnapshot.airQuality.valid = true;
            uiSnapshot.airQuality.pm1 = ptowerData.pm1_0;
            uiSnapshot.airQuality.pm25 = ptowerData.pm2_5;
            uiSnapshot.airQuality.pm10 = ptowerData.pm10_0;
        }
        else
        {
            uiSnapshot.airQuality.valid = false;
            uiSnapshot.airQuality.pm1 = 0.0f;
            uiSnapshot.airQuality.pm25 = 0.0f;
            uiSnapshot.airQuality.pm10 = 0.0f;
        }

#ifdef FEATURE_ANEMOMETER
        uiSnapshot.wind.enabled = true;
        AnemometerData anemoData;
        if (components.anemometer.getData(anemoData) && anemoData.valid)
        {
            uiSnapshot.wind.valid = true;
            uiSnapshot.wind.speed = anemoData.speed;
            uiSnapshot.wind.windKph = anemoData.windKph;
            uiSnapshot.wind.direction = anemoData.direction;
        }
        else
        {
            uiSnapshot.wind.valid = false;
        }
#else
        uiSnapshot.wind.enabled = false;
        uiSnapshot.wind.valid = false;
#endif

#ifdef FEATURE_DUAL_BATTERY
        uiSnapshot.power.dualBattery = true;
        uiSnapshot.power.poweredByDc = components.battery.isPoweredByDC();
        uiSnapshot.power.activeBattery = components.battery.getActiveMosfet();
        uiSnapshot.power.batteryPct0 = components.battery.getPct(0);
        uiSnapshot.power.batteryPct1 = components.battery.getPct(1);
        uiSnapshot.power.batteryPct = components.battery.getPct();
        uiSnapshot.power.batteryVoltage0 = components.battery.getBatteryVoltage(0);
        uiSnapshot.power.batteryVoltage1 = components.battery.getBatteryVoltage(1);
        uiSnapshot.power.batteryVoltage = components.battery.getVoltage();
        uiSnapshot.power.dcVoltage = components.battery.getDCVoltage();
#else
        uiSnapshot.power.dualBattery = false;
        uiSnapshot.power.poweredByDc = false;
        uiSnapshot.power.activeBattery = 0;
#if BAREBONES == true
        uiSnapshot.power.batteryPct = (millis() / 100) % 101;
#else
        uiSnapshot.power.batteryPct = components.battery.getPct();
#endif
        uiSnapshot.power.batteryVoltage = components.battery.getVoltage();
#endif

        DateTime now = components.rtc.now();
        uiSnapshot.status.sdReady = isSdReady;
        uiSnapshot.status.sdStatus = (int)sdStatus;
#ifdef FEATURE_MODEM
        uiSnapshot.status.modemEnabled = true;
#else
        uiSnapshot.status.modemEnabled = false;
#endif
        uiSnapshot.status.uploadPending = uploadPending;
        uiSnapshot.status.uploadActive = uploadActive;
        uiSnapshot.status.displayOn = isDisplayOn;
        uiSnapshot.status.unixTime = now.unixtime();
        uiSnapshot.status.hour = now.hour();
        uiSnapshot.status.minute = now.minute();

        bool changed = force ||
                       previous.status.sdStatus != uiSnapshot.status.sdStatus ||
                       previous.status.uploadPending != uiSnapshot.status.uploadPending ||
                       previous.status.uploadActive != uiSnapshot.status.uploadActive ||
                       previous.status.displayOn != uiSnapshot.status.displayOn ||
                       previous.status.minute != uiSnapshot.status.minute ||
                       previous.power.batteryPct != uiSnapshot.power.batteryPct;
        if (changed)
            markUiDirty();
        return changed;
    }

    static void logTaskWorker(void *arg)
    {
        Context *self = static_cast<Context *>(arg);
        LogJob job;
        while (true)
        {
            if (xQueueReceive(self->logQueue, &job, portMAX_DELAY) == pdTRUE)
            {
                (void)job;
                self->uploadPending = false;
                self->processCaptureSample();
                self->markUiDirty();
            }
        }
    }

    void startLogTask()
    {
        if (logQueue == nullptr)
            logQueue = xQueueCreate(4, sizeof(LogJob));
        if (logTaskHandle == nullptr && logQueue != nullptr)
            xTaskCreatePinnedToCore(logTaskWorker, "EoloLogTask", 8192, this, 1, &logTaskHandle, 0);
    }

    void enqueueLogData()
    {
        if (logQueue == nullptr)
            startLogTask();

        LogJob job = {millis()};
        if (logQueue != nullptr && xQueueSend(logQueue, &job, 0) == pdTRUE)
        {
            uploadPending = true;
            markUiDirty();
            return;
        }

        LOG_LN("Cola de log llena o no disponible; se omite log para no bloquear UI");
    }

    void processCaptureSample()
    {
        saveSession();
        logData();
#ifdef FEATURE_MODEM
        uploadData();
#endif
    }

    bool logData()
    {
        PROFILE_SCOPE("context.log");

        if (!isSdReady)
        {
            if (!sdMissingLogReported)
            {
                LOG_LN("SD no disponible; se omite log local para esta sesión");
                sdMissingLogReported = true;
            }
            return false;
        }

        LOG_LN("Iniciando log de datos en SD...");
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
                markSdFailed();
                LOG_LN("No se pudo abrir el archivo para escribir/crear");
                return false;
            }

#ifdef FEATURE_ANEMOMETER
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
                markSdFailed();
                LOG_LN("No se pudo abrir el archivo para escribir");
                return false;
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

#ifdef FEATURE_ANEMOMETER

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
            LOG_OUT(" Time: ");
            LOG_OUT_LN(components.rtc.now().timestamp());
            LOG_OUT(" Flow: ");
            LOG_OUT_LN(flowData.flow);
            LOG_OUT(" Flow_target: ");
            LOG_OUT_LN(session.targetFlow);
            LOG_OUT(" Temp: ");
            LOG_OUT_LN(components.bme.temperature);
            LOG_OUT(" Hum: ");
            LOG_OUT_LN(components.bme.humidity);
            LOG_OUT(" Pres: ");
            LOG_OUT_LN(components.bme.pressure);
            LOG_OUT(" PM1: ");
            LOG_OUT_LN(ptowerData.pm1_0);
            LOG_OUT(" PM2.5: ");
            LOG_OUT_LN(ptowerData.pm2_5);
            LOG_OUT(" PM10: ");
            LOG_OUT_LN(ptowerData.pm10_0);
            LOG_OUT(" Battery: ");
            LOG_OUT_LN(components.battery.getPct());
            file.close();

            LOG_LN("Archivo de log escrito!");
        }
        sdStatus = SD_OK;
        LOG_LN("Log completado con éxito.");
        return true;
    }

    void uploadData()
    {
#ifdef FEATURE_MODEM
        PROFILE_SCOPE("context.upload");
        uploadActive = true;
        markUiDirty();
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
        uploadActive = false;
        markUiDirty();
#endif
    }

    bool update()
    {
        components.input.poll();
        components.poll();

        bool inputChanged = components.input.hasChanged;
        if (components.input.hasChanged)
        {
            setDisplayPower(true);
            lastInputTime = millis();
            components.input.hasChanged = false;
            markUiDirty();
        }

        if (isDisplayOn && (millis() - lastInputTime > DISPLAY_TIMEOUT_MS))
        {
            setDisplayPower(false);
            markUiDirty();
        }

        updateCapture();
        bool statusChanged = updateUiSnapshot();
        bool changed = inputChanged || statusChanged || uiDirty;
        uiDirty = false;
        return changed;
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

        int p0 = 0, p1 = 0;
        if (calibration.getMotorPwms(session.targetFlow, p0, p1))
        {
            if (lastTargetFlow != session.targetFlow)
            {
                LOG_OUT("Flujo objetivo: ");
                LOG_OUT(session.targetFlow, 2);
                LOG_OUT(" L/min -> PWM[");
                LOG_OUT(p0);
                LOG_OUT(", ");
                LOG_OUT(p1);
                LOG_OUT_LN("]");
                lastTargetFlow = session.targetFlow;
            }

            components.motor.setMotorPwm(0, p0);
            components.motor.setMotorPwm(1, p1);
        }
        else
        {
            components.motor.setPwm(0);
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
            LOG_OUT("Tiempo transcurrido: ");
            LOG_OUT_LN(ctx.session.elapsedTime);

            LOG_OUT("Duración establecida: ");
            LOG_OUT_LN(ctx.session.duration);

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
            enqueueLogData();
        }
    }

} Context;

inline Context *Context::instance = nullptr;
#endif
