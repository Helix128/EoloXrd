#ifndef EOLO_APPLICATION_DRON_APPLICATION_H
#define EOLO_APPLICATION_DRON_APPLICATION_H

#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)

#include "../Config/Legacy.h"

#include <Arduino.h>
#include <esp_sleep.h>

#include "../Board/CaptureSwitches.h"
#include "../Board/HeadlessSetupServer.h"
#include "../Board/HeadlessSetupTypes.h"
#include "../Data/Context.h"
#include "../Utility/DebugConsole.h"
#include "../Utility/RS485Monitor.h"
#include "Profiler.h"

// EOLO Dron has a different product flow from the display-based models.  The
// application owns all headless state so main.cpp remains only the Arduino
// entry point while construction and startup ordering remain unchanged.
class DronApplication
{
public:
    DronApplication()
        : _headlessSetupServer(_context, _captureSwitches)
    {
    }

    void begin()
    {
#if PPH_PWR_PIN >= 0
        pinMode(PPH_PWR_PIN, OUTPUT); // perifericos
        digitalWrite(PPH_PWR_PIN, HIGH); // Encender perifericos (I2C, display) lo antes posible
#endif
        // El gestor de energia y el ProMini del Eolo MP necesitan varios segundos
        // para estabilizarse tras el N-FET. El worker I2C cuenta desde este punto y
        // mantiene libre el loop/UI durante el warmup.
        I2CBus::getInstance().setWarmupFromNow(I2C_WARMUP_MS);
#ifdef FEATURE_MODEM
        if constexpr (EoloConfig::modemPowerMode == EoloConfig::ModemPowerMode::AlwaysOn)
            Modem::configurePowerPinOn();
        else
            Modem::configurePowerPinOff();
#else
#if MODEM_PWR_PIN >= 0
        pinMode(MODEM_PWR_PIN, OUTPUT); // modem
        digitalWrite(MODEM_PWR_PIN, LOW);
#endif
#endif

        _context.components.motor.begin(); // apagar motores

        Serial.begin(115200);

#ifdef FEATURE_MODEM
        _debugConsole.attachModemService(&_context.components.modemService);
        _debugConsole.attachSensorApi(&_context.components.api);
#endif
        _debugConsole.attachRTC(&_context.components.rtc);
        _debugConsole.attachCaptureSwitches(&_captureSwitches);
        _debugConsole.attachDroneContext(&_context);
        RS485Monitor::getInstance(); // Inicializar monitor RS485
        LOG_LN("RS485 Monitor inicializado");

        // Inicialización del contexto de la app
        _context.begin();

        setDroneLed(StatusLedPattern::Boot);
    }

    void update()
    {
        _debugConsole.poll();

        if (millis() - _lastFrameMs < kTargetMs)
        {
            return;
        }

        _frameStartMs = millis();
        _lastFrameMs += kTargetMs;

        _context.update();
        updateDroneController();

        const unsigned long frameExecutionMs = millis() - _frameStartMs;
        PROFILE_MARK("loop.frame", frameExecutionMs * 1000UL);
        RS485Monitor::getInstance().recordLoopFrameTime(frameExecutionMs);
        RS485Monitor::getInstance().checkAndReportViolations();
    }

private:
    enum class DroneBootState : uint8_t
    {
        Booting,
        Idle,
        Setup,
        Waiting,
        Capturing,
        Finished,
        Debug
    };

    static constexpr unsigned long kTargetMs = 8UL;

    // Preserve main.cpp's original construction order: Context, switches,
    // server, then console.
    Context _context;
    CaptureSwitches _captureSwitches;
    HeadlessSetupServer _headlessSetupServer;
    DebugConsole _debugConsole;
    DroneBootState _droneState = DroneBootState::Booting;
    bool _droneFinishHandled = false;
    unsigned long _lastFrameMs = 0;
    unsigned long _frameStartMs = 0;

    void setDroneLed(StatusLedPattern pattern)
    {
#ifdef FEATURE_NEOPIXEL
        _context.components.statusLed.setPattern(pattern);
#else
        (void)pattern;
#endif
    }

    void updateDroneStatusLed()
    {
        if (_droneState == DroneBootState::Capturing &&
            (_context.sdStatus() == SD_ERROR || _context.sdStatus() == SD_MISSING))
        {
            setDroneLed(StatusLedPattern::Error);
            return;
        }

        if (_droneState == DroneBootState::Capturing && _context.isMotorOverheatActive())
        {
            setDroneLed(StatusLedPattern::MotorOverheat);
            return;
        }

        if (_droneState == DroneBootState::Capturing &&
            (_context.isLogActive() || _context.isUploadPending() || _context.isUploadActive()))
        {
            setDroneLed(StatusLedPattern::Busy);
            return;
        }

        switch (_droneState)
        {
        case DroneBootState::Setup:
            setDroneLed(StatusLedPattern::Setup);
            break;
        case DroneBootState::Waiting:
            setDroneLed(StatusLedPattern::Waiting);
            break;
        case DroneBootState::Capturing:
            setDroneLed(StatusLedPattern::Capturing);
            break;
        case DroneBootState::Finished:
            setDroneLed(StatusLedPattern::Finished);
            break;
        case DroneBootState::Debug:
            setDroneLed(StatusLedPattern::Setup);
            break;
        case DroneBootState::Idle:
        default:
            setDroneLed(StatusLedPattern::Boot);
            break;
        }
    }

    void startDroneConfiguredCapture(const HeadlessSetupConfig &config)
    {
        const uint32_t nowUnix = _context.getUnixTime();
        HeadlessSetup::applyToSession(config, _context.session, nowUnix);
        _context.clearSession();
        _context.saveSession();

        if (config.waitSeconds == 0)
        {
            LOG_LN("Drone: captura instantanea desde setup web.");
            _context.beginCapture();
            _droneState = DroneBootState::Capturing;
        }
        else
        {
            LOG_OUT("Drone: setup web confirmado; esperando ");
            LOG_OUT(config.waitSeconds);
            LOG_OUT_LN(" segundos antes de capturar.");
            _droneState = DroneBootState::Waiting;
        }
        updateDroneStatusLed();
    }

    void configureDroneCapture()
    {
        _captureSwitches.begin();
        delay(100);
        const CaptureSwitchSnapshot switchSnapshot = _captureSwitches.snapshot();
        CaptureSwitches::printSnapshot(stdOut, switchSnapshot);
        const CaptureSwitchSelection selection = switchSnapshot.selection;

        if (HeadlessSetup::shouldEnterWebSetup(selection.waitCode))
        {
            LOG_LN("Switches de espera en Off; entrando a setup web de EOLO Dron.");
            _context.components.motor.setPowerPct(0);
            _headlessSetupServer.begin();
            _droneState = DroneBootState::Setup;
            updateDroneStatusLed();
            return;
        }

        _context.session.usePlantower = false;
        _context.session.targetFlow = DRONE_TARGET_FLOW_LPM;

        if (!selection.shouldStart)
        {
            LOG_LN("Configuracion de switches indica idle; captura no iniciada.");
            _context.components.motor.setPowerPct(0);
            _droneState = DroneBootState::Idle;
            updateDroneStatusLed();
            return;
        }

        const uint32_t nowUnix = _context.getUnixTime();
        _context.session.startUnix = nowUnix + selection.waitSeconds;
        _context.session.duration = selection.durationSeconds;
        _context.session.elapsedTime = 0;
        _context.session.lastLog = 0;
        _context.session.capturedVolume = 0.0f;
        _context.clearSession();
        _context.saveSession();

        if (selection.instantStart)
        {
            LOG_LN("Drone: captura instantanea.");
            _context.beginCapture();
            _droneState = DroneBootState::Capturing;
        }
        else
        {
            LOG_OUT("Drone: esperando ");
            LOG_OUT(selection.waitSeconds);
            LOG_OUT_LN(" segundos antes de capturar.");
            _droneState = DroneBootState::Waiting;
        }
        updateDroneStatusLed();
    }

    void updateDroneController()
    {
        if (_droneState == DroneBootState::Booting)
        {
            if (!_context.bootInitComplete.load())
            {
                updateDroneStatusLed();
                return;
            }
            configureDroneCapture();
            return;
        }

        if (_droneState == DroneBootState::Debug)
        {
            _context.components.motor.updateRamp();
            _context.updateMotorThermalProtection();
            _headlessSetupServer.handleClient();
            updateDroneStatusLed();
            return;
        }

        if (_droneState == DroneBootState::Setup)
        {
            _context.components.motor.setPowerPct(0);
            _headlessSetupServer.handleClient();
            if (_headlessSetupServer.debugModeActive())
            {
                LOG_LN("Drone: entrando a modo debug PWM; servidor web permanece activo.");
                _droneState = DroneBootState::Debug;
                updateDroneStatusLed();
                return;
            }
            if (_headlessSetupServer.confirmed())
            {
                const HeadlessSetupConfig config = _headlessSetupServer.confirmedConfig();
                _headlessSetupServer.stop();
                startDroneConfiguredCapture(config);
            }
            updateDroneStatusLed();
            return;
        }

        if (_droneState == DroneBootState::Waiting)
        {
            if (_context.isHeadlessCalibrationRunning())
            {
                updateDroneStatusLed();
                return;
            }
            const uint32_t now = _context.getUnixTime();
            if (now >= _context.session.startUnix)
            {
                LOG_LN("Drone: espera cumplida, iniciando captura.");
                _context.beginCapture();
                _droneState = DroneBootState::Capturing;
            }
            updateDroneStatusLed();
            return;
        }

        if (_droneState == DroneBootState::Capturing && _context.hasCaptureEnded())
        {
            _droneState = DroneBootState::Finished;
        }

        updateDroneStatusLed();

        if (_droneState == DroneBootState::Finished && !_droneFinishHandled)
        {
#ifdef FEATURE_MODEM
            if (!_context.logsIdle() ||
                _context.isUploadPending() || _context.isUploadActive() ||
                _context.components.modemService.pendingCount() > 0)
            {
                _context.components.modemService.shutdownWhenIdle();
                return;
            }
            if (_context.components.modemService.state() != ModemServiceState::Off)
            {
                _context.components.modemService.shutdownNow();
                return;
            }
#endif
            _droneFinishHandled = true;
            _context.clearSession();
            _context.components.motor.setPwmImmediate(0);
            setDroneLed(StatusLedPattern::Finished);
            _context.components.statusLed.poll(true);
#ifdef STATUS_LED_LOW_POWER
            delay(140);
            setDroneLed(StatusLedPattern::Off);
            _context.components.statusLed.poll(true);
#endif
            LOG_LN("Drone: captura finalizada; entrando en deep sleep hasta reset/power-cycle.");
            Serial.flush();
            esp_deep_sleep_start();
        }
    }
};

#else
#error "DronApplication requiere EOLO_TARGET_DRON con FEATURE_HEADLESS"
#endif

#endif
