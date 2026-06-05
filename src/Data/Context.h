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
#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)
#include "HeadlessMotorCalibration.h"
#endif
#include "../Config.h"
#include "../Board/I2CBus.h"
#ifndef FEATURE_HEADLESS
#include "../Drawing/SceneManager.h"
#endif
#include "ESPJob.h"
#ifndef FEATURE_HEADLESS
#include <U8g2lib.h>
#endif
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Profiler.h"

typedef struct Context
{
    static Context *instance;
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
#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)
    HeadlessMotorCalibration headlessCalibration;
#endif

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
    bool &uploadPending = logging.uploadPending;
    bool &uploadActive = uploader.uploadActive;
    volatile bool &logActive = logging.logActive;
    bool uiDirty = true;

    UiSnapshot uiSnapshot;
    TaskHandle_t bootTaskHandle = nullptr;
    bool bootInitComplete = false;
    bool bootInitRunning = false;

    enum class BootPhase : uint8_t {
        Idle,
        StartingModem,
        InitSD,
        Done
    };
    volatile BootPhase bootPhase = BootPhase::Idle;
    uint32_t bootPhaseStartMs = 0;
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

    bool syncRTCFromTimeServer(const char *url = RTCManager::DefaultTimeServerUrl) { return rtcSync.syncFromTimeServer(*this, url); }
    bool startRTCNetworkSync() { return rtcSync.start(*this); }
    RTCNetworkSyncStatus getRTCNetworkSyncStatus() const { return rtcSync.getStatus(); }

    void initDisplay()
    {
#ifndef FEATURE_HEADLESS
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
#endif
    }

    void setDisplayPower(bool on)
    {
#ifndef FEATURE_HEADLESS
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

        uiSnapshot.environment.temperature = components.bme.temperature;
        uiSnapshot.environment.humidity = components.bme.humidity;
        uiSnapshot.environment.pressure = components.bme.pressure;
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
#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)
        headlessCalibration.update(*this);
#endif
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

    bool isHeadlessCalibrationRunning() const {
#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)
        return headlessCalibration.isRunning();
#else
        return false;
#endif
    }

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

inline Context *Context::instance = nullptr;

#include "SessionStoreImpl.h"
#include "Logging/LogServiceImpl.h"
#include "RTCNetworkSyncImpl.h"
#include "UploadServiceImpl.h"
#include "MotorCaptureControlImpl.h"
#include "CaptureControllerImpl.h"
#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)
#include "HeadlessMotorCalibrationImpl.h"
#endif

#endif
