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
    bool &isCapturing = capture.isCapturing;
    bool &isPaused = capture.isPaused;
    bool &isEnd = capture.isEnd;
    bool &motorOverheatActive = motorCapture.motorOverheatActive;
    bool &motorThermalSensorValid = motorCapture.motorThermalSensorValid;
    float &motorThermalTemperature = motorCapture.motorThermalTemperature;
    SDStatus &sdStatus = logging.sdStatus;
    bool &isSdReady = logging.isSdReady;
    const char *&eoloDir = logging.eoloDir;
    const char *&logsDir = logging.logsDir;
    std::atomic_bool &uploadPending = logging.uploadPending;
    std::atomic_bool &uploadActive = uploader.uploadActive;
    std::atomic_bool &logActive = logging.logActive;
    bool uiDirty = true;

    UiSnapshot uiSnapshot;
    TaskHandle_t bootTaskHandle = nullptr;
    // Written by boot init task (core 0), read by UI/main loop (core 1).
    std::atomic_bool bootInitComplete{false};
    // Written by boot init task (core 0), read by UI/main loop (core 1).
    std::atomic_bool bootInitRunning{false};

    enum class BootPhase : uint8_t {
        Idle,
        StartingModem,
        InitSD,
        Done
    };
    // Written by boot init task (core 0), read by splash UI/main loop (core 1).
    std::atomic<BootPhase> bootPhase{BootPhase::Idle};
    // Written by boot init task (core 0), read by splash UI/main loop (core 1).
    std::atomic_uint32_t bootPhaseStartMs{0};
    const char *rtcAdjustReturnScene = "inicio";

    using RTCNetworkSyncStatus = ::RTCNetworkSyncStatus;

public:
#ifndef FEATURE_HEADLESS
    Context(DisplayModel &display) : u8g2(display) {}
#else
    Context() {}
#endif

    void begin()
    {
        PROFILE_SCOPE("context.begin");
        // 1. Esperar a que los periféricos (ya encendidos en main.cpp) se estabilicen
        delay(100); 

        bool grande = false;
#ifdef FEATURE_MODEM
        grande = true;
#endif
        const char *versionType = grande ? "Standard" : "Express";
        LOG_F("Iniciando EOLO %s\n", versionType);

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
        startBootInitTask();
    }

    static void bootInitWorker(void *arg)
    {
        Context *self = static_cast<Context *>(arg);
        self->bootInitRunning = true;

#ifdef FEATURE_MODEM
        self->bootPhase = BootPhase::StartingModem;
        self->bootPhaseStartMs = millis();
        self->markUiDirty();
        self->components.modemService.begin();
#endif

        self->bootPhase = BootPhase::InitSD;
        self->bootPhaseStartMs = millis();
        self->markUiDirty();
        self->initSD();

        self->bootPhase = BootPhase::Done;
        self->bootInitComplete = true;
        self->bootInitRunning = false;
        self->markUiDirty();
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
        u8g2.setBusClock(EoloConfig::board.displaySpiClockHz);
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

    bool initSD() { return logging.initSD(*this); }
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
        uiSnapshot.environment.ntcValid = motorThermalSensorValid;
        uiSnapshot.environment.ntcTemperature = motorThermalTemperature;
        uiSnapshot.environment.motorOverheat = motorOverheatActive;
        uiSnapshot.environment.motorThermalSensorValid = motorThermalSensorValid;
        uiSnapshot.environment.motorThermalTemperature = motorThermalTemperature;
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
        uiSnapshot.status.sdReady = isSdReady;
        uiSnapshot.status.sdStatus = (int)sdStatus;
#ifdef FEATURE_MODEM
        ModemServiceState modemState = components.modemService.state();
        uiSnapshot.status.modemEnabled = true;
        uiSnapshot.status.modemPowered = modemState != ModemServiceState::Off;
        uiSnapshot.status.modemActive = modemState == ModemServiceState::Booting ||
                                        modemState == ModemServiceState::Busy ||
                                        uploadActive;
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
        uiSnapshot.status.uploadPending = uploadPending.load();
        uiSnapshot.status.uploadActive = uploadActive.load();
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

    void startLogTask() { logging.startLogTask(*this); }
    void enqueueLogData() { logging.enqueueLogData(*this); }
    bool logsIdle() const { return logging.logsIdle(); }
    void processCaptureSample() { logging.processCaptureSample(*this); }
    bool logData() { return logging.logData(*this); }
    void uploadData() { uploader.uploadData(*this); }

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
    bool updateMotorThermalProtection() { return motorCapture.updateThermalProtection(*this); }
    void resetMotorFlowController() { motorCapture.resetFlowController(); }
    void updateMotors() { motorCapture.updateMotors(*this); }
    void updateCapture() { capture.update(*this); }

} Context;

#define CONTEXT_CLASS_DEFINED

// Re-include the headers to trigger compilation of their inline implementations
#include "Logging/LogService.h"
#include "CaptureController.h"
#include "RTCNetworkSync.h"
#include "MotorCaptureControl.h"
#include "UploadService.h"

#endif
