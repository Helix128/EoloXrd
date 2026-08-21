#ifndef CONTEXT_H
#define CONTEXT_H

#include "Session.h"
#include "Components.h"
#include "CalibrationManager.h"
#include "UiSnapshot.h"
#include "Logging/LogService.h"
#include "SessionStore.h"
#include "RTCNetworkSync.h"
#include "CaptureController.h"
#include "MotorCaptureControl.h"
#include "UploadService.h"
#include "../Config/Legacy.h"
#include "../Board/I2CBus.h"
#include "../Board/SPIBus.h"
#ifndef FEATURE_HEADLESS
#include "../Drawing/SceneManager.h"
#endif
#ifndef FEATURE_HEADLESS
#include <U8g2lib.h>
#include <SPI.h>
#include "../Drawing/Logos.h"
#endif
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Profiler.h"
#include <atomic>

typedef struct Context
{
#ifndef FEATURE_HEADLESS
    DisplayModel &u8g2;
#endif
    bool isDisplayOn = true;
    bool isDisplayReady = false;
    unsigned long int lastInputTime = 0;
    const unsigned long int DISPLAY_TIMEOUT_MS = 60000;

    Components components;
    Session session;

    CalibrationManager calibration;
    LogService logging;
    SessionStore sessionStore;
    RTCNetworkSync rtcSync;
    CaptureController capture;
    MotorCaptureControl motorCapture;
    UploadService uploader;

    static constexpr int CAPTURE_INTERVAL = CaptureController::CAPTURE_INTERVAL;
    bool uiDirty = true;

    UiSnapshot uiSnapshot;
    TaskHandle_t bootTaskHandle = nullptr;
    // Written by boot init task (core 0), read by UI/main loop (core 1).
    std::atomic_bool bootInitComplete{false};
    // Written by boot init task (core 0), read by UI/main loop (core 1).
    std::atomic_bool bootInitRunning{false};

    enum class BootPhase : uint8_t {
        Idle,
        StartingServices,
        InitSD,
        WaitingI2C,
        Ready
    };
    // Written by boot init task (core 0), read by splash UI/main loop (core 1).
    std::atomic<BootPhase> bootPhase{BootPhase::Idle};
    // Written by boot init task (core 0), read by splash UI/main loop (core 1).
    std::atomic_uint32_t bootPhaseStartMs{0};
    // Revision used to ensure that a phase which precedes a blocking
    // peripheral operation was rendered at least once by the UI.
    std::atomic_uint32_t bootPhaseRevision{0};
    std::atomic_uint32_t bootPhaseRenderedRevision{0};
    const char *rtcAdjustReturnScene = "inicio";

    using RTCNetworkSyncStatus = ::RTCNetworkSyncStatus;

public:
#ifndef FEATURE_HEADLESS
    Context(DisplayModel &display) : u8g2(display) {}
#else
    Context() {}
#endif

    // Context remains the public boundary for application and scene code.
    // These queries deliberately expose values, never aliases to the mutable
    // state owned by capture, motor control, logging, or upload services.
    bool isCaptureActive() const { return capture.isCapturing; }
    bool isCapturePaused() const { return capture.isPaused; }
    bool hasCaptureEnded() const { return capture.isEnd; }

    bool isMotorOverheatActive() const { return motorCapture.motorOverheatActive; }
    bool isMotorThermalSensorValid() const { return motorCapture.motorThermalSensorValid; }
    float motorThermalTemperatureC() const { return motorCapture.motorThermalTemperature; }

    SDStatus sdStatus() const { return logging.sdStatus; }
    bool isSdReady() const { return logging.isSdReady; }
    const char *logsDirectory() const { return logging.logsDir; }

    bool isLogActive() const { return logging.logActive.load(); }
    bool isUploadPending() const {
#ifdef FEATURE_MODEM
        return logging.uploadPending.load() || components.api.pendingTelemetry() > 0;
#else
        return logging.uploadPending.load();
#endif
    }
    bool isUploadActive() const {
#ifdef FEATURE_MODEM
        return uploader.uploadActive.load() || components.api.telemetryInFlight();
#else
        return uploader.uploadActive.load();
#endif
    }

    void begin()
    {
        PROFILE_SCOPE("context.begin");
        // 1. Esperar a que los periféricos (ya encendidos en main.cpp) se estabilicen
        delay(100); 

        LOG_F("Iniciando %s\n", EoloConfig::kModelName);

#ifndef FEATURE_HEADLESS
        // 3. Inicializar pantalla
        LOG_LN("Iniciando pantalla...");
        initDisplay();
        LOG_LN("Pantalla iniciada");
#else
        isDisplayOn = false;
        isDisplayReady = false;
#endif

        // 4. Inicializar motores y componentes
        components.motor.begin();
        components.motor.setPwm(0);

        LOG_LN("Inicializando contexto...");
        components.begin();

        calibration.load();
        startLogTask();
        updateUiSnapshot(true);
#ifdef FEATURE_HEADLESS
        // Headless targets have no splash frame that can start the boot task.
        startBootInitTask();
#endif
    }

    void setBootPhase(BootPhase phase)
    {
        bootPhase = phase;
        bootPhaseStartMs = millis();
        uint32_t revision = bootPhaseRevision.load() + 1UL;
        bootPhaseRevision = revision;
        markUiDirty();
    }

    uint32_t currentBootPhaseRevision() const
    {
        return bootPhaseRevision.load();
    }

    void acknowledgeBootPhaseRendered()
    {
        bootPhaseRenderedRevision = bootPhaseRevision.load();
    }

    void waitForBootPhaseRendered(uint32_t revision, uint32_t timeoutMs = 250UL)
    {
#ifndef FEATURE_HEADLESS
        uint32_t started = millis();
        while (bootPhaseRenderedRevision.load() < revision &&
               (uint32_t)(millis() - started) < timeoutMs)
        {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
#else
        (void)revision;
        (void)timeoutMs;
#endif
    }

    static void bootInitWorker(void *arg)
    {
        Context *self = static_cast<Context *>(arg);
        uint32_t bootStarted = millis();
        self->bootInitRunning = true;

        self->setBootPhase(BootPhase::StartingServices);
#ifdef FEATURE_MODEM
        // begin() only starts the service/worker. The modem's long power-on
        // and AT handshake continue independently of local boot readiness.
        bool modemServiceStarted = self->components.modemService.begin();
        LOG_F("Boot: servicio modem %s; arranque en segundo plano\n",
              modemServiceStarted ? "iniciado" : "no disponible");
#endif

        self->setBootPhase(BootPhase::InitSD);
        uint32_t sdPhaseRevision = self->currentBootPhaseRevision();
        self->waitForBootPhaseRendered(sdPhaseRevision);
        uint32_t sdStarted = millis();
        bool sdReady = self->initSD();
        LOG_F("Boot: SD %s en %lu ms\n",
              sdReady ? "lista" : "no disponible",
              (unsigned long)(millis() - sdStarted));

        self->setBootPhase(BootPhase::WaitingI2C);
        I2CBus &i2c = I2CBus::getInstance();
        uint32_t i2cStarted = millis();
        while (!i2c.warmupComplete())
            vTaskDelay(pdMS_TO_TICKS(10));
        LOG_F("Boot: warm-up I2C completo en %lu ms\n",
              (unsigned long)(millis() - i2cStarted));

        self->setBootPhase(BootPhase::Ready);
        self->bootInitComplete = true;
        self->bootInitRunning = false;
        self->markUiDirty();
        LOG_F("Boot: interfaz local lista en %lu ms; modem continúa en segundo plano\n",
              (unsigned long)(millis() - bootStarted));
        self->bootTaskHandle = nullptr;
        vTaskDelete(nullptr);
    }

    void startBootInitTask()
    {
        if (bootTaskHandle != nullptr || bootInitComplete.load() || bootInitRunning.load())
            return;

        // Prio 1: tarea de inicializacion de una sola ejecucion; finaliza antes de que corran los sensores.
        // Stack 8192: realiza I2C, SD, RTC y setup inicial con buffers temporales grandes.
        xTaskCreatePinnedToCore(
            bootInitWorker,
            "EoloBootInit",
            8192,
            this,
            1,
            &bootTaskHandle,
            0);
    }

    bool syncRTCFromTimeServer(const char *url = RTCManager::DefaultTimeServerUrl)
    {
#ifdef FEATURE_MODEM
        bool queued = rtcSync.syncFromTimeServer(components.modemService, components.rtc, url);
        if (queued) markUiDirty();
        return queued;
#else
        (void)url;
        return false;
#endif
    }

    bool startRTCNetworkSync()
    {
#ifdef FEATURE_MODEM
        bool started = rtcSync.start(components.modemService, components.rtc,
                                     RTCManager::DefaultTimeServerUrl);
        if (started) markUiDirty();
        return started;
#else
        return false;
#endif
    }
    RTCNetworkSyncStatus getRTCNetworkSyncStatus() const { return rtcSync.getStatus(); }

    void initDisplay()
    {
#ifndef FEATURE_HEADLESS
        SPIBus::Guard spiGuard;
#if !EOLO_DISPLAY_SPI
        I2CBus::Guard i2cGuard;
#endif
#if EOLO_DISPLAY_SPI && EOLO_DISPLAY_HW_SPI
        SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
#endif
        bool began = u8g2.begin();
        if (!began)
        {
            LOG_LN("Fallo al iniciar pantalla");
            return;
        }
#if EOLO_DISPLAY_SPI && EOLO_DISPLAY_HW_SPI
        u8g2.setBusClock(EoloConfig::displaySpiClockHz);
#elif !EOLO_DISPLAY_SPI
        u8g2.setBusClock(I2C_CLOCK);
#endif
        u8g2.clearBuffer();
        u8g2.setBitmapMode(1);
        u8g2.drawXBM(0, 0, 128, 64, cmas);
        u8g2.sendBuffer();
        isDisplayReady = true;
#endif
    }

    void setDisplayPower(bool on)
    {
#ifndef FEATURE_HEADLESS
        SPIBus::Guard spiGuard;
        isDisplayOn = on;
        if (on)
        {
            u8g2.setPowerSave(0);
        }
        else
        {
            u8g2.setPowerSave(1);
        }
#else
        (void)on;
        isDisplayOn = false;
#endif
    }

    void enableDisplay()
    {
#ifndef FEATURE_HEADLESS
        lastInputTime = millis();
        components.input.hasChanged = false;
        setDisplayPower(true);
#endif
    }

    bool initSD() { return logging.initSD(); }
    void markSdFailed() { logging.markSdFailed(); }
    void saveSession() { sessionStore.save(session); }
    bool loadSession() { return sessionStore.load(session, components.rtc); }
    bool canLoadSession() { return sessionStore.canLoad(); }
    void clearSession() { sessionStore.clear(); }

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

        BME280Data bmeData;
        bool bmeValid = components.bme.getData(bmeData);
        uiSnapshot.environment.valid = bmeValid;
        uiSnapshot.environment.temperature = bmeValid ? bmeData.temperature : -1.0f;
        uiSnapshot.environment.humidity = bmeValid ? bmeData.humidity : -1.0f;
        uiSnapshot.environment.pressure = bmeValid ? bmeData.pressure : -1.0f;
#ifdef FEATURE_NTC
        uiSnapshot.environment.ntcValid = isMotorThermalSensorValid();
        uiSnapshot.environment.ntcTemperature = motorThermalTemperatureC();
        uiSnapshot.environment.motorOverheat = isMotorOverheatActive();
        uiSnapshot.environment.motorThermalSensorValid = isMotorThermalSensorValid();
        uiSnapshot.environment.motorThermalTemperature = motorThermalTemperatureC();
#else
        uiSnapshot.environment.ntcValid = false;
        uiSnapshot.environment.ntcTemperature = -1.0f;
        uiSnapshot.environment.motorOverheat = false;
        uiSnapshot.environment.motorThermalSensorValid = false;
        uiSnapshot.environment.motorThermalTemperature = -1.0f;
#endif

        uiSnapshot.airQuality.enabled = session.usePlantower;
#ifdef FEATURE_PLANTOWER
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
#else
        uiSnapshot.airQuality.enabled = false;
        uiSnapshot.airQuality.valid = false;
        uiSnapshot.airQuality.pm1 = 0.0f;
        uiSnapshot.airQuality.pm25 = 0.0f;
        uiSnapshot.airQuality.pm10 = 0.0f;
#endif

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
        uiSnapshot.power.valid = components.battery.hasValidData();
        uiSnapshot.power.stale = components.battery.isStale();
        uiSnapshot.power.ageMs = uiSnapshot.power.valid
                                    ? millis() - components.battery.getLastSuccessMs()
                                    : 0;
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
        uiSnapshot.power.valid = true;
        uiSnapshot.power.stale = false;
        uiSnapshot.power.ageMs = 0;
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
        uiSnapshot.status.sdReady = isSdReady();
        uiSnapshot.status.sdStatus = (int)sdStatus();
#ifdef FEATURE_MODEM
        ModemServiceState modemState = components.modemService.state();
        uiSnapshot.status.modemEnabled = true;
        uiSnapshot.status.modemPowered = modemState != ModemServiceState::Off;
        uiSnapshot.status.modemActive = modemState == ModemServiceState::Booting ||
                                        modemState == ModemServiceState::Busy ||
                                        isUploadActive();
        uiSnapshot.status.modemError = modemState == ModemServiceState::Error;
        uiSnapshot.status.modemSignalKnown = components.modemService.hasSignalQuality();
        uiSnapshot.status.modemSignalBars = components.modemService.signalQualityBars();
        uiSnapshot.status.modemSignalCsq = components.modemService.signalQualityCsq();
#else
        uiSnapshot.status.modemEnabled = false;
        uiSnapshot.status.modemPowered = false;
        uiSnapshot.status.modemActive = false;
        uiSnapshot.status.modemError = false;
        uiSnapshot.status.modemSignalKnown = false;
        uiSnapshot.status.modemSignalBars = 0;
        uiSnapshot.status.modemSignalCsq = 99;
#endif
        uiSnapshot.status.uploadPending = isUploadPending();
        uiSnapshot.status.uploadActive = isUploadActive();
        uiSnapshot.status.displayOn = isDisplayOn;
        uiSnapshot.status.unixTime = now.unixtime();
        uiSnapshot.status.hour = now.hour();
        uiSnapshot.status.minute = now.minute();

        bool changed = force ||
                       previous.status.sdStatus != uiSnapshot.status.sdStatus ||
                       previous.status.uploadPending != uiSnapshot.status.uploadPending ||
                       previous.status.uploadActive != uiSnapshot.status.uploadActive ||
                       previous.status.modemPowered != uiSnapshot.status.modemPowered ||
                       previous.status.modemActive != uiSnapshot.status.modemActive ||
                       previous.status.modemError != uiSnapshot.status.modemError ||
                       previous.status.modemSignalKnown != uiSnapshot.status.modemSignalKnown ||
                       previous.status.modemSignalBars != uiSnapshot.status.modemSignalBars ||
                       previous.status.modemSignalCsq != uiSnapshot.status.modemSignalCsq ||
                       previous.status.displayOn != uiSnapshot.status.displayOn ||
                       previous.status.minute != uiSnapshot.status.minute ||
                       previous.environment.motorOverheat != uiSnapshot.environment.motorOverheat ||
                       previous.environment.motorThermalSensorValid != uiSnapshot.environment.motorThermalSensorValid ||
                       fabsf(previous.environment.motorThermalTemperature - uiSnapshot.environment.motorThermalTemperature) > 0.1f ||
                       previous.power.batteryPct != uiSnapshot.power.batteryPct;
        if (changed)
            markUiDirty();
        return changed;
    }

    void startLogTask() { logging.startLogTask(); }

    bool buildLogRecord(LogRecord &record)
    {
        record = LogRecord();
        record.timestampUnix = getUnixTime();
        record.elapsedSeconds = session.elapsedTime;
        record.targetFlow = session.targetFlow;
        record.capturedVolume = session.capturedVolume;
        record.sessionActive = isCaptureActive();

        BME280Data bmeData;
        (void)components.bme.getData(bmeData);
        record.environment = bmeData;

        (void)components.flowSensor.getData(record.flow);
#ifdef FEATURE_PLANTOWER
        (void)components.plantower.getData(record.plantower);
#endif
#ifdef FEATURE_ANEMOMETER
        (void)components.anemometer.getData(record.anemometer);
#endif
#ifdef FEATURE_NTC
        (void)components.ntc.getData(record.ntc);
#endif
        record.batteryVoltage = components.battery.getVoltage();
        record.batteryPercent = components.battery.getPct();
        return record.flow.valid || record.environment.valid ||
               record.plantower.valid || record.anemometer.valid;
    }

    void enqueueLogData()
    {
        if (!logging.hasLogQueue())
            logging.startLogTask();

        LogRecord record;
        (void)buildLogRecord(record);
        saveSession();

#ifdef FEATURE_FLOW_PID
        const bool kickActive = motorCapture.getPidStatus().kickActive;
#else
        const bool kickActive = false;
#endif
        logging.enqueueLogRecord(record, session.startUnix,
#ifdef FEATURE_FLOW_PID
                                 true,
#else
                                 false,
#endif
#ifdef FEATURE_PLANTOWER
                                 session.usePlantower,
#else
                                 false,
#endif
#ifdef FEATURE_ANEMOMETER
                                 true,
#else
                                 false,
#endif
#ifdef FEATURE_NTC
                                 true,
#else
                                 false,
#endif
                                 kickActive);
    }
    bool logsIdle() const { return logging.logsIdle(); }
    void processCaptureSample()
    {
        enqueueLogData();
#ifdef FEATURE_MODEM
        uploadData();
#endif
    }
    bool logData()
    {
        LogRecord record;
        (void)buildLogRecord(record);
        return logging.logData(record, session.startUnix,
#ifdef FEATURE_FLOW_PID
                               true,
#else
                               false,
#endif
#ifdef FEATURE_PLANTOWER
                               session.usePlantower,
#else
                               false,
#endif
#ifdef FEATURE_ANEMOMETER
                               true,
#else
                               false,
#endif
#ifdef FEATURE_NTC
                               true,
#else
                               false,
#endif
#ifdef FEATURE_FLOW_PID
                               motorCapture.getPidStatus().kickActive ? "Arrancando" : "Capturando"
#else
                               "Capturando"
#endif
        );
    }
    bool collectTelemetrySnapshot(TelemetrySnapshot &snapshot)
    {
        snapshot = TelemetrySnapshot();
        snapshot.timestampUnix = getUnixTime();
        snapshot.targetFlow = session.targetFlow;
        snapshot.capturedVolume = session.capturedVolume;

        BME280Data bmeData;
        bool bmeValid = components.bme.getData(bmeData);
        snapshot.environment.temperature = bmeValid ? bmeData.temperature : -1.0f;
        snapshot.environment.humidity = bmeValid ? bmeData.humidity : -1.0f;
        snapshot.environment.pressure = bmeValid ? bmeData.pressure : -1.0f;
        snapshot.environment.valid = bmeValid;
        float batteryVoltage = components.battery.getVoltage();
        snapshot.batteryVoltage = isfinite(batteryVoltage) && batteryVoltage >= 0.0f ? batteryVoltage : -1.0f;

        float rtcTemperature = -1.0f;
        snapshot.rtcTemperature = components.rtc.getTemperature(rtcTemperature) ? rtcTemperature : -1.0f;

#ifdef FEATURE_ANEMOMETER
        (void)components.anemometer.getData(snapshot.anemometer);
#endif
#ifdef FEATURE_PLANTOWER
        (void)components.plantower.getData(snapshot.plantower);
#endif
        (void)components.flowSensor.getData(snapshot.flow);
#ifdef FEATURE_MODEM
        GnssData gnss = components.modemService.gnssData();
        if (gnss.valid) {
            snapshot.latitude = gnss.latitude;
            snapshot.longitude = gnss.longitude;
            snapshot.gpsSpeed = gnss.speedKmh;
            snapshot.satellites = gnss.satellites;
        }
        snapshot.signalStrength = components.modemService.signalQualityForTelemetry();
#endif
        return snapshot.flow.valid || snapshot.environment.valid ||
               snapshot.anemometer.valid || snapshot.plantower.valid;
    }

    void uploadData()
    {
#ifdef FEATURE_MODEM
        PROFILE_SCOPE("context.upload");
        uploader.uploadActive = true;
        markUiDirty();

        TelemetrySnapshot snapshot;
        (void)collectTelemetrySnapshot(snapshot);
        uploader.publishSnapshot(components.api, snapshot);
        uploader.uploadActive = false;
        markUiDirty();
#endif
    }

    bool update()
    {
#ifndef FEATURE_HEADLESS
        components.input.poll();
#endif
        components.poll();

        bool inputChanged = false;
#ifndef FEATURE_HEADLESS
        inputChanged = components.input.hasChanged;
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
#endif

        updateMotorThermalProtection();
#if defined(FEATURE_FLOW_PID)
        if (motorCapture.isPidTestRunning())
            updateMotors();
#endif
        updateCapture();
        bool statusChanged = updateUiSnapshot();
        bool changed = inputChanged || statusChanged || uiDirty;
        uiDirty = false;
        return changed;
    }

    uint32_t getUnixTime()
    {
        DateTime now = components.rtc.now();
        return components.rtc.isValid(now) ? now.unixtime() : 0UL;
    }

    bool isHeadlessCalibrationRunning() const { return false; }

    void beginCapture() { capture.begin(*this); }
    void pauseCapture() { capture.pause(*this); }
    void resumeCapture() { capture.resume(*this); }
    void endCapture() { capture.end(*this); }
    void resetCapture() { capture.reset(*this); }
    bool updateMotorThermalProtection()
    {
#ifdef FEATURE_NTC
        const bool previousActive = motorCapture.motorOverheatActive;
        const bool previousValid = motorCapture.motorThermalSensorValid;
        const float previousTemperature = motorCapture.motorThermalTemperature;
        NTCData ntcData;
        const bool ntcValid = components.ntc.getData(ntcData);
        const bool overheat = motorCapture.updateThermalProtection(ntcData, ntcValid,
                                                                     components.motor);
        if (previousActive != motorCapture.motorOverheatActive ||
            previousValid != motorCapture.motorThermalSensorValid ||
            fabsf(previousTemperature - motorCapture.motorThermalTemperature) > 0.1f)
        {
            markUiDirty();
        }
        return overheat;
#else
        NTCData ignoredNtc;
        return motorCapture.updateThermalProtection(ignoredNtc, false, components.motor);
#endif
    }
    void resetMotorFlowController() { motorCapture.resetFlowController(); }
    void stopPidTest()
    {
#if defined(FEATURE_FLOW_PID) || defined(FEATURE_FLOW_CALIBRATION)
        motorCapture.stopPidTest(components.motor);
#endif
    }
    void updateMotors()
    {
        FlowData flowData;
        const bool flowReadValid = components.flowSensor.getData(flowData);
        motorCapture.updateMotors(components.motor, flowData, flowReadValid,
                                  session.targetFlow, calibration);
    }
    void updateCapture() { capture.update(*this); }

} Context;

// Capture control remains a transitional adapter over Context. Its bindings
// are explicit and included only after this composition root is complete.
#include "ContextCaptureController.h"
#include "ContextHeadlessMotorCalibration.h"

#endif
