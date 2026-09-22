#ifndef EOLO_CORE_FLOW_FLOW_MOTOR_CONTROLLER_H
#define EOLO_CORE_FLOW_FLOW_MOTOR_CONTROLLER_H

#include <math.h>
#include <stdint.h>
#include <Eolo/Core/Flow/SmartFlowController.h>

enum class IgnitionPhase : uint8_t { Off, Kick, Run };

enum FlowPidFault : uint8_t
{
    FLOW_PID_FAULT_NONE = 0,
    FLOW_PID_FAULT_SENSOR_INVALID = 1,
    FLOW_PID_FAULT_SENSOR_STALE = 2,
    FLOW_PID_FAULT_TIMING = 3,
    // AFM07 válido sin caudal mientras la bomba está energizada.
    FLOW_PID_FAULT_ZERO_FLOW = 4,
    FLOW_PID_FAULT_PUMP_ZERO_FLOW = FLOW_PID_FAULT_ZERO_FLOW
};

struct FlowPidConfig
{
    uint32_t intervalMs;
    float deadband;
    float kp;
    float ki;
    float integralLimit;
    int maxStep;
    float filterAlpha;
    float minActive;
    float kd;
    uint32_t maxDtMs;
    uint32_t sensorStaleMs;
    // Ignition: kick alto en frío para vencer el capacitor
    int kickPwm;
    uint32_t kickMs;
    // Stall: detección de motor parado durante Run + re-kick automático
    float stallFlowLpm;
    uint32_t restallCooldownMs;
    uint32_t stallConfirmMs;
    // Overhaul: Centro y Trimming Suave
    int softTrimMax;
    int softMaxStep;
    float sensitivity;
    uint32_t recenterDelayMs;
    // Tiempo máximo que se conserva el último PWM tras perder el sensor.  El
    // perfil Dron lo fija en 1,5 s; los perfiles históricos conservan 6 s.
    uint32_t sensorFaultStopMs = 6000UL;
    // Cero desactiva la protección; los perfiles AFM07 de captura usan 5
    // muestras (~1,25 s con la cadencia nominal de 250 ms).
    uint8_t zeroFlowConfirmSamples = 0;
};

struct FlowPidStatus
{
    bool enabled = false;
    bool running = false;
    bool flowValid = false;
    float targetFlow = 0.0f;
    float measuredFlow = -1.0f;
    float filteredFlow = -1.0f;
    float error = 0.0f;
    float integral = 0.0f;
    float dtSeconds = 0.0f;
    float pTerm = 0.0f;
    float iTerm = 0.0f;
    float dTerm = 0.0f;
    int pwm = 0;
    int centerPwm = 0;
    int trimPwm = 0;
    bool centerFound = false;
    uint32_t lastUpdateMs = 0;
    SmartFlowMode mode = SMART_FLOW_SEEK_CENTER;
    FlowPidFault fault = FLOW_PID_FAULT_NONE;
    bool timingOk = true;
    bool outputLimited = false;
    float estimatedGain = 0.0f;
    float confidence = 0.0f;
    bool modelValid = false;
    IgnitionPhase ignitionPhase = IgnitionPhase::Off;
    bool kickActive = false;
    uint16_t kickCount = 0;
    uint32_t lastKickMs = 0;
    bool stallDetected = false;
    bool stoppedForSensorFault = false;
    uint8_t zeroFlowCount = 0;
    uint8_t zeroFlowConfirmSamples = 0;
    bool zeroFlowFailureLatched = false;
    int zeroFlowFailurePwm = 0;
};

struct FlowMotorInput
{
    uint32_t nowMs = 0;
    int currentPwm = 0;
    float targetFlow = 0.0f;
    float measuredFlow = -1.0f;
    bool flowValid = false;
    bool flowFresh = true;
    bool flowStale = false;
    uint32_t flowAgeMs = 0;
    int maxPwm = 4095;
};

struct FlowMotorOutput
{
    bool updated = false;
    bool initialized = false;
    int pwm = 0;
    bool zeroSecondaryMotors = true;
    FlowPidFault fault = FLOW_PID_FAULT_NONE;
    SmartFlowStatus smartStatus;
    IgnitionPhase ignitionPhase = IgnitionPhase::Off;
    bool kickActive = false;
    uint16_t kickCount = 0;
    bool stallDetected = false;
    bool stoppedForSensorFault = false;
    uint8_t zeroFlowCount = 0;
    bool zeroFlowFailureLatched = false;
    int zeroFlowFailurePwm = 0;
};

// Cotas de validación de FlowPidConfig.
// Los valores reflejan los límites físicos del actuador y del lazo de control.
namespace FlowPidLimits
{
    // Intervalo de ciclo: mínimo útil del sensor / máximo seguro sin watchdog
    static constexpr uint32_t INTERVAL_MIN_MS            = 50;
    static constexpr uint32_t INTERVAL_MAX_MS            = 10000;
    // Deadband en L/min: >5 supera el rango nominal del caudalímetro
    static constexpr float    DEADBAND_MAX                = 5.0f;
    // Kp: >1000 satura el actuador con error mínimo
    static constexpr float    KP_MAX                      = 1000.0f;
    // Ki: >200 produce wind-up violento en el rango PWM disponible
    static constexpr float    KI_MAX                      = 200.0f;
    // integralLimit: acumulador interno del PID; >500 excede la resolución del actuador
    static constexpr float    INTEGRAL_LIMIT_MAX          = 500.0f;
    // Kd: >500 amplifica el ruido del sensor hasta saturación
    static constexpr float    KD_MAX                      = 500.0f;
    // minActive en L/min: >5 L/min no tiene sentido como umbral de activación
    static constexpr float    MIN_ACTIVE_MAX              = 5.0f;
    // maxDtMs / sensorStaleMs: ventana máxima antes de considerar el ciclo inválido
    static constexpr uint32_t MAX_DT_MAX_MS               = 30000;
    static constexpr uint32_t SENSOR_STALE_MAX_MS         = 30000;
    // kickMs: <50 ms no supera la inercia del rotor; >5000 ms puede dañar el devanado
    static constexpr uint32_t KICK_MS_MIN                 = 50;
    static constexpr uint32_t KICK_MS_MAX                 = 5000;
    // stallFlowLpm: umbral de stall en L/min; >2 supera el mínimo nominal del motor
    static constexpr float    STALL_FLOW_MAX_LPM          = 2.0f;
    // restallCooldownMs: <1000 ms causaría ciclos de re-kick continuos
    static constexpr uint32_t RESTALL_COOLDOWN_MIN_MS     = 1000;
    static constexpr uint32_t RESTALL_COOLDOWN_MAX_MS     = 120000;
    // stallConfirmMs: tiempo de confirmación antes del re-kick
    static constexpr uint32_t STALL_CONFIRM_MIN_MS        = 100;
    static constexpr uint32_t STALL_CONFIRM_MAX_MS        = 10000;
    // softTrimMax: límite de excursión alrededor del centro
    static constexpr int      SOFT_TRIM_MAX_LIMIT         = 2000;
    // softMaxStep: límite de paso por ciclo en soft trim
    static constexpr int      SOFT_MAX_STEP_LIMIT         = 500;
    // sensitivity: factor de escala post-centro
    static constexpr float    SENSITIVITY_MAX             = 10.0f;
    // recenterDelayMs: retraso antes de deslizar centro
    static constexpr uint32_t RECENTER_DELAY_MIN_MS       = 500;
    static constexpr uint32_t RECENTER_DELAY_MAX_MS       = 60000;
}

class FlowMotorController
{
public:
    static bool validateConfig(const FlowPidConfig &config, int maxPwm)
    {
        if (config.intervalMs < FlowPidLimits::INTERVAL_MIN_MS || config.intervalMs > FlowPidLimits::INTERVAL_MAX_MS)
            return false;
        if (isnan(config.deadband) || config.deadband < 0.0f || config.deadband > FlowPidLimits::DEADBAND_MAX)
            return false;
        if (isnan(config.kp) || config.kp < 0.0f || config.kp > FlowPidLimits::KP_MAX)
            return false;
        if (isnan(config.ki) || config.ki < 0.0f || config.ki > FlowPidLimits::KI_MAX)
            return false;
        if (isnan(config.integralLimit) || config.integralLimit < 0.0f || config.integralLimit > FlowPidLimits::INTEGRAL_LIMIT_MAX)
            return false;
        if (isnan(config.kd) || config.kd < 0.0f || config.kd > FlowPidLimits::KD_MAX)
            return false;
        if (config.maxStep <= 0 || config.maxStep > maxPwm)
            return false;
        if (isnan(config.filterAlpha) || config.filterAlpha <= 0.0f || config.filterAlpha > 1.0f)
            return false;
        if (isnan(config.minActive) || config.minActive < 0.0f || config.minActive > FlowPidLimits::MIN_ACTIVE_MAX)
            return false;
        if (config.maxDtMs < config.intervalMs || config.maxDtMs > FlowPidLimits::MAX_DT_MAX_MS)
            return false;
        if (config.sensorStaleMs < config.intervalMs || config.sensorStaleMs > FlowPidLimits::SENSOR_STALE_MAX_MS)
            return false;
        if (config.kickPwm < 0 || config.kickPwm > maxPwm)
            return false;
        if (config.kickMs < FlowPidLimits::KICK_MS_MIN || config.kickMs > FlowPidLimits::KICK_MS_MAX)
            return false;
        if (isnan(config.stallFlowLpm) || config.stallFlowLpm < 0.0f || config.stallFlowLpm > FlowPidLimits::STALL_FLOW_MAX_LPM)
            return false;
        if (config.restallCooldownMs < FlowPidLimits::RESTALL_COOLDOWN_MIN_MS || config.restallCooldownMs > FlowPidLimits::RESTALL_COOLDOWN_MAX_MS)
            return false;
        if (config.stallConfirmMs < FlowPidLimits::STALL_CONFIRM_MIN_MS || config.stallConfirmMs > FlowPidLimits::STALL_CONFIRM_MAX_MS)
            return false;
        if (config.softTrimMax < 0 || config.softTrimMax > FlowPidLimits::SOFT_TRIM_MAX_LIMIT)
            return false;
        if (config.softMaxStep < 0 || config.softMaxStep > FlowPidLimits::SOFT_MAX_STEP_LIMIT)
            return false;
        if (isnan(config.sensitivity) || config.sensitivity < 0.0f || config.sensitivity > FlowPidLimits::SENSITIVITY_MAX)
            return false;
        if (config.recenterDelayMs > 0 && (config.recenterDelayMs < FlowPidLimits::RECENTER_DELAY_MIN_MS || config.recenterDelayMs > FlowPidLimits::RECENTER_DELAY_MAX_MS))
            return false;
        if (config.sensorFaultStopMs < config.intervalMs || config.sensorFaultStopMs > FlowPidLimits::SENSOR_STALE_MAX_MS)
            return false;
        return true;
    }

    static SmartFlowTune tuneFromConfig(const FlowPidConfig &config)
    {
        SmartFlowTune tune;
        tune.kp = config.kp;
        tune.ki = config.ki;
        tune.kd = config.kd;
        tune.integralLimit = config.integralLimit;
        tune.deadband = config.deadband;
        tune.minActive = config.minActive;
        tune.fastAlpha = config.filterAlpha;
        tune.slowAlpha = config.filterAlpha * 0.5f;
        if (tune.slowAlpha < 0.05f) tune.slowAlpha = 0.05f;
        if (tune.slowAlpha > 0.35f) tune.slowAlpha = 0.35f;
        tune.seekStep = config.maxStep;
        tune.maxStep = config.maxStep;
        tune.softTrimMax = config.softTrimMax > 0 ? config.softTrimMax : 64;
        tune.softStepLimit = config.softMaxStep > 0 ? config.softMaxStep : 2;
        tune.sensitivity = config.sensitivity > 0.05f ? config.sensitivity : 1.0f;
        tune.recenterDelayMs = config.recenterDelayMs >= 500 ? config.recenterDelayMs : 4000;
        return tune;
    }

    static const char *ignitionPhaseText(IgnitionPhase phase)
    {
        switch (phase)
        {
        case IgnitionPhase::Off:  return "off";
        case IgnitionPhase::Kick: return "kick";
        case IgnitionPhase::Run:  return "run";
        default:                  return "unknown";
        }
    }

    void reset()
    {
        _initialized = false;
        _smart = SmartFlowController();
        _smartStatus = SmartFlowStatus();
        _targetFlow = -1.0f;
        _measuredFlow = -1.0f;
        _filteredFlow = -1.0f;
        _currentPwm = 0;
        _lastUpdateMs = 0;
        _lastDtSeconds = 0.0f;
        _fault = FLOW_PID_FAULT_NONE;
        _timingOk = true;
        _ignPhase = IgnitionPhase::Off;
        _kickStartMs = 0;
        _kickCount = 0;
        _lastKickMs = 0;
        _stallStartMs = 0;
        _stallDetected = false;
        _forceKick = false;
        _sensorFaultActive = false;
        _sensorFaultStopped = false;
        _sensorFaultStartMs = 0;
        _zeroFlowCount = 0;
        _zeroFlowConfirmSamples = 0;
        _zeroFlowFailureLatched = false;
        _zeroFlowFailurePwm = 0;
    }

    bool isInitialized() const { return _initialized; }

    // Fuerza un kick de arranque en el próximo ciclo (para cambio de batería o prueba manual).
    void forceKick()
    {
        _forceKick = true;
    }

    // Arranca el lazo desde un mando externo (por ejemplo, feed-forward de
    // calibración) sin sustituirlo por un kick. El siguiente update busca
    // y enclava el centro alrededor de este valor inicial.
    void seedRunning(int pwm)
    {
        _initialized = true;
        _currentPwm = pwm < 0 ? 0 : pwm;
        _ignPhase = IgnitionPhase::Run;
        _lastUpdateMs = 0;
        _fault = FLOW_PID_FAULT_NONE;
        _smart.resetController(false);
        _sensorFaultActive = false;
        _sensorFaultStopped = false;
        _zeroFlowCount = 0;
    }

    FlowMotorOutput update(const FlowMotorInput &input, const FlowPidConfig &config)
    {
        _zeroFlowConfirmSamples = config.zeroFlowConfirmSamples;
        FlowMotorOutput output;
        output.pwm = _currentPwm;
        output.initialized = _initialized;
        output.fault = _fault;
        output.smartStatus = _smartStatus;
        output.ignitionPhase = _ignPhase;
        output.kickActive = (_ignPhase == IgnitionPhase::Kick);
        output.kickCount = _kickCount;
        output.stallDetected = _stallDetected;
        output.stoppedForSensorFault = _sensorFaultStopped;
        output.zeroFlowCount = _zeroFlowCount;
        output.zeroFlowFailureLatched = _zeroFlowFailureLatched;
        output.zeroFlowFailurePwm = _zeroFlowFailurePwm;

        // Procesar solicitud de kick forzado externo
        if (_forceKick && _initialized)
        {
            _ignPhase = IgnitionPhase::Kick;
            _kickStartMs = input.nowMs;
            _lastKickMs = input.nowMs;
            _kickCount++;
            _stallStartMs = 0;
            _stallDetected = false;
            _smart.resetController(true);
            _zeroFlowCount = 0;
        }
        _forceKick = false;

        // Throttling: Run respeta intervalMs; Kick se actualiza frecuente para detectar fin de pulso
        const uint32_t updateIntervalMs = (_ignPhase == IgnitionPhase::Run) ? config.intervalMs : 50u;
        if (_initialized && (input.nowMs - _lastUpdateMs) < updateIntervalMs)
        {
            output.ignitionPhase = _ignPhase;
            output.kickActive = (_ignPhase == IgnitionPhase::Kick);
            return output;
        }

        // Primera init o cambio de target: actualizar tune y target
        if (!_initialized || fabsf(_targetFlow - input.targetFlow) > 0.001f)
        {
            bool wasRun = _initialized && (_ignPhase == IgnitionPhase::Run);
            _initialized = true;
            _smart.setTune(tuneFromConfig(config));
            if (wasRun)
                _smart.uncenter();
            else if (!wasRun && _ignPhase == IgnitionPhase::Off)
                _smart.resetController(false);
            _targetFlow = input.targetFlow;
            _sensorFaultActive = false;
            _sensorFaultStopped = false;
            _sensorFaultStartMs = 0;
            _zeroFlowCount = 0;
        }

        // --- OFF: target inactivo ---
        bool targetActive = input.targetFlow > config.minActive;
        if (!targetActive)
        {
            _ignPhase = IgnitionPhase::Off;
            _stallStartMs = 0;
            _stallDetected = false;
            _currentPwm = 0;
            _fault = FLOW_PID_FAULT_NONE;
            _sensorFaultActive = false;
            _sensorFaultStopped = false;
            _zeroFlowCount = 0;
            _lastUpdateMs = input.nowMs;
            output.updated = true;
            output.initialized = true;
            output.pwm = 0;
            output.ignitionPhase = IgnitionPhase::Off;
            output.kickActive = false;
            output.kickCount = _kickCount;
            output.stallDetected = false;
            output.stoppedForSensorFault = false;
            output.zeroFlowCount = 0;
            output.zeroFlowFailureLatched = _zeroFlowFailureLatched;
            output.zeroFlowFailurePwm = _zeroFlowFailurePwm;
            return output;
        }

        // Un fallo de sensor que ya apagó el motor no puede reanudarlo por la
        // llegada de una muestra tardía. Solo reset() o un cambio de objetivo
        // autoriza un nuevo arranque.
        if (_sensorFaultStopped)
        {
            _currentPwm = 0;
            output.updated = true;
            output.pwm = 0;
            output.fault = _fault;
            output.ignitionPhase = IgnitionPhase::Off;
            output.kickActive = false;
            output.stoppedForSensorFault = true;
            return output;
        }

        // Un fallo de flujo cero es un enclavamiento del controlador. Sólo
        // reset() (o el inicio de una nueva captura en la capa superior) lo
        // libera; una muestra posterior no puede reactivar el PWM.
        if (_zeroFlowFailureLatched)
        {
            _currentPwm = 0;
            _ignPhase = IgnitionPhase::Off;
            _fault = FLOW_PID_FAULT_ZERO_FLOW;
            output.updated = true;
            output.pwm = 0;
            output.fault = _fault;
            output.ignitionPhase = IgnitionPhase::Off;
            output.kickActive = false;
            output.zeroFlowCount = _zeroFlowCount;
            output.zeroFlowFailureLatched = true;
            output.zeroFlowFailurePwm = _zeroFlowFailurePwm;
            return output;
        }

        // --- Transición Off → Kick ---
        if (_ignPhase == IgnitionPhase::Off)
        {
            _ignPhase = IgnitionPhase::Kick;
            _kickStartMs = input.nowMs;
            _lastKickMs = input.nowMs;
            _kickCount++;
            _stallStartMs = 0;
            _stallDetected = false;
            _smart.resetController(false);
        }

        // --- KICK: pulso alto para vencer el capacitor ---
        if (_ignPhase == IgnitionPhase::Kick)
        {
            _zeroFlowCount = 0;
            const int kp = (config.kickPwm < input.maxPwm) ? config.kickPwm : input.maxPwm;
            _currentPwm = kp;
            _lastUpdateMs = input.nowMs;
            if ((input.nowMs - _kickStartMs) >= config.kickMs)
            {
                // Kick completado → Run; preservar modelo aprendido
                _ignPhase = IgnitionPhase::Run;
                _stallStartMs = 0;
                _smart.resetController(true);
            }
            output.updated = true;
            output.initialized = true;
            output.pwm = _currentPwm;
            output.fault = FLOW_PID_FAULT_NONE;
            output.ignitionPhase = _ignPhase;
            output.kickActive = (_ignPhase == IgnitionPhase::Kick);
            output.kickCount = _kickCount;
            output.stallDetected = _stallDetected;
            output.zeroFlowCount = 0;
            return output;
        }

        // --- RUN: verificar sensor ---
        const bool sensorTooOld = input.flowStale || (input.flowAgeMs > config.sensorStaleMs);
        if (!input.flowValid || !input.flowFresh || sensorTooOld)
        {
            _measuredFlow = -1.0f;
            _fault = input.flowValid ? FLOW_PID_FAULT_SENSOR_STALE : FLOW_PID_FAULT_SENSOR_INVALID;
            _zeroFlowCount = 0;
            _lastUpdateMs = input.nowMs;
            _smart.resetController(true);
            if (!_sensorFaultActive)
            {
                _sensorFaultActive = true;
                _sensorFaultStartMs = input.nowMs;
            }
            if (static_cast<uint32_t>(input.nowMs - _sensorFaultStartMs) >= config.sensorFaultStopMs)
            {
                _currentPwm = 0;
                _ignPhase = IgnitionPhase::Off;
                _sensorFaultStopped = true;
                output.updated = true;
                output.stoppedForSensorFault = true;
                output.pwm = 0;
            }
            // Antes del límite se conserva el último PWM para tolerar una
            // lectura perdida; el adaptador Dron aborta la captura al detectar
            // el estado inválido y por eso nunca se rearranca automáticamente.
            output.fault = _fault;
            output.ignitionPhase = _ignPhase;
            output.kickActive = false;
            output.kickCount = _kickCount;
            output.stallDetected = _stallDetected;
            output.zeroFlowCount = 0;
            return output;
        }

        _sensorFaultActive = false;

        // Timing
        const uint32_t dtMs = input.nowMs - _lastUpdateMs;
        const float dtSeconds = dtMs / 1000.0f;
        _lastUpdateMs = input.nowMs;
        _lastDtSeconds = dtSeconds;
        _timingOk = (dtMs <= config.maxDtMs);
        if (!_timingOk)
        {
            _fault = FLOW_PID_FAULT_TIMING;
            _zeroFlowCount = 0;
            _smart.resetController(true);
            output.fault = _fault;
            output.ignitionPhase = _ignPhase;
            output.kickCount = _kickCount;
            output.stallDetected = _stallDetected;
            output.zeroFlowCount = 0;
            return output;
        }

        _fault = FLOW_PID_FAULT_NONE;
        _measuredFlow = input.measuredFlow;
        _smart.setTune(tuneFromConfig(config));

        // Sólo una muestra física válida/fresca en Run y con PWM efectivo
        // activo puede confirmar la protección. El adaptador dual descarta
        // sampleId repetidos antes de llegar aquí.
        if (config.zeroFlowConfirmSamples == 0 || _currentPwm <= 0)
        {
            _zeroFlowCount = 0;
        }
        else if (input.measuredFlow == 0.0f)
        {
            if (_zeroFlowCount < UINT8_MAX)
                ++_zeroFlowCount;
            if (_zeroFlowCount >= config.zeroFlowConfirmSamples)
            {
                _zeroFlowFailureLatched = true;
                _zeroFlowFailurePwm = _currentPwm;
                _currentPwm = 0;
                _ignPhase = IgnitionPhase::Off;
                _fault = FLOW_PID_FAULT_ZERO_FLOW;
                _smart.resetController(true);
                output.updated = true;
                output.initialized = true;
                output.pwm = 0;
                output.fault = _fault;
                output.ignitionPhase = IgnitionPhase::Off;
                output.kickActive = false;
                output.kickCount = _kickCount;
                output.stallDetected = _stallDetected;
                output.zeroFlowCount = _zeroFlowCount;
                output.zeroFlowFailureLatched = true;
                output.zeroFlowFailurePwm = _zeroFlowFailurePwm;
                return output;
            }
        }
        else
        {
            // Flujo positivo (o cualquier lectura distinta de cero) rompe la
            // consecutividad requerida por la protección.
            _zeroFlowCount = 0;
        }

        // --- Detección de stall durante Run ---
        if (config.stallFlowLpm > 0.0f && input.measuredFlow < config.stallFlowLpm)
        {
            if (_stallStartMs == 0)
                _stallStartMs = input.nowMs;

            const bool stallConfirmed = (input.nowMs - _stallStartMs) >= config.stallConfirmMs;
            const bool cooldownOk = (input.nowMs - _lastKickMs) >= config.restallCooldownMs;

            if (stallConfirmed && cooldownOk)
            {
                // Re-kick automático
                _stallDetected = true;
                _ignPhase = IgnitionPhase::Kick;
                _kickStartMs = input.nowMs;
                _lastKickMs = input.nowMs;
                _kickCount++;
                _stallStartMs = 0;
                _smart.resetController(true);
                _zeroFlowCount = 0;
                const int kp = (config.kickPwm < input.maxPwm) ? config.kickPwm : input.maxPwm;
                _currentPwm = kp;
                output.updated = true;
                output.initialized = true;
                output.pwm = _currentPwm;
                output.fault = FLOW_PID_FAULT_NONE;
                output.ignitionPhase = IgnitionPhase::Kick;
                output.kickActive = true;
                output.kickCount = _kickCount;
                output.stallDetected = _stallDetected;
                return output;
            }
        }
        else
        {
            _stallStartMs = 0;
            _stallDetected = false;
        }

        // --- PID / SmartFlow normal (Center-Seeking / Soft-Trim) ---
        _smartStatus = _smart.update(input.nowMs, input.currentPwm, input.targetFlow,
                                     input.measuredFlow, dtSeconds, input.maxPwm);
        _filteredFlow = _smartStatus.fastFlow;
        _currentPwm = _smartStatus.pwm;

        output.updated = true;
        output.initialized = true;
        output.pwm = _currentPwm;
        output.fault = _fault;
        output.smartStatus = _smartStatus;
        output.ignitionPhase = _ignPhase;
        output.kickActive = false;
        output.kickCount = _kickCount;
        output.stallDetected = _stallDetected;
        output.stoppedForSensorFault = false;
        output.zeroFlowCount = _zeroFlowCount;
        output.zeroFlowFailureLatched = _zeroFlowFailureLatched;
        output.zeroFlowFailurePwm = _zeroFlowFailurePwm;
        return output;
    }

    FlowPidStatus status(bool enabled, bool testRunning, float testTargetFlow) const
    {
        FlowPidStatus s;
        s.enabled = enabled;
        s.running = _initialized || testRunning;
        s.flowValid = _measuredFlow >= 0.0f;
        s.targetFlow = _targetFlow >= 0.0f ? _targetFlow : testTargetFlow;
        s.measuredFlow = _measuredFlow;
        s.filteredFlow = _filteredFlow;
        s.error = s.filteredFlow >= 0.0f ? s.targetFlow - s.filteredFlow : 0.0f;
        s.integral = _smartStatus.integral;
        s.dtSeconds = _lastDtSeconds;
        s.pTerm = _smartStatus.pTerm;
        s.iTerm = _smartStatus.iTerm;
        s.dTerm = _smartStatus.dTerm;
        s.pwm = _currentPwm;
        s.centerPwm = _smartStatus.centerPwm;
        s.trimPwm = _smartStatus.trimPwm;
        s.centerFound = _smartStatus.centerFound;
        s.lastUpdateMs = _lastUpdateMs;
        s.mode = _smartStatus.mode;
        s.fault = _fault;
        s.timingOk = _timingOk;
        s.outputLimited = _smartStatus.upperSaturation;
        s.estimatedGain = _smartStatus.estimatedGain;
        s.confidence = _smartStatus.confidence;
        s.modelValid = _smartStatus.modelValid;
        s.ignitionPhase = _ignPhase;
        s.kickActive = (_ignPhase == IgnitionPhase::Kick);
        s.kickCount = _kickCount;
        s.lastKickMs = _lastKickMs;
        s.stallDetected = _stallDetected;
        s.stoppedForSensorFault = _sensorFaultStopped;
        s.zeroFlowCount = _zeroFlowCount;
        s.zeroFlowConfirmSamples = _zeroFlowConfirmSamples;
        s.zeroFlowFailureLatched = _zeroFlowFailureLatched;
        s.zeroFlowFailurePwm = _zeroFlowFailurePwm;
        return s;
    }

    const SmartFlowStatus& smartStatus() const { return _smartStatus; }
    bool isKickActive() const { return _ignPhase == IgnitionPhase::Kick; }
    uint16_t kickCount() const { return _kickCount; }
    bool stallDetected() const { return _stallDetected; }
    uint8_t zeroFlowCount() const { return _zeroFlowCount; }
    bool zeroFlowFailureLatched() const { return _zeroFlowFailureLatched; }
    int zeroFlowFailurePwm() const { return _zeroFlowFailurePwm; }

    // Permite al adaptador dual descartar una confirmación incompleta cuando
    // una lectura física llega inválida u obsoleta sin liberar un fallo ya
    // enclavado.
    void resetZeroFlowConfirmation()
    {
        if (!_zeroFlowFailureLatched)
            _zeroFlowCount = 0;
    }

private:
    bool _initialized = false;
    SmartFlowController _smart;
    SmartFlowStatus _smartStatus;
    float _targetFlow = -1.0f;
    float _measuredFlow = -1.0f;
    float _filteredFlow = -1.0f;
    int _currentPwm = 0;
    uint32_t _lastUpdateMs = 0;
    float _lastDtSeconds = 0.0f;
    FlowPidFault _fault = FLOW_PID_FAULT_NONE;
    bool _timingOk = true;
    IgnitionPhase _ignPhase = IgnitionPhase::Off;
    uint32_t _kickStartMs = 0;
    uint16_t _kickCount = 0;
    uint32_t _lastKickMs = 0;
    uint32_t _stallStartMs = 0;
    bool _stallDetected = false;
    bool _forceKick = false;
    bool _sensorFaultActive = false;
    bool _sensorFaultStopped = false;
    uint32_t _sensorFaultStartMs = 0;
    uint8_t _zeroFlowCount = 0;
    uint8_t _zeroFlowConfirmSamples = 0;
    bool _zeroFlowFailureLatched = false;
    int _zeroFlowFailurePwm = 0;
};

#endif
