#ifndef EOLO_CORE_FLOW_FLOW_MOTOR_CONTROLLER_H
#define EOLO_CORE_FLOW_FLOW_MOTOR_CONTROLLER_H

#include <math.h>
#include <stdint.h>
#include <Eolo/Core/Flow/SmartFlowController.h>

enum FlowPidFault : uint8_t
{
    FLOW_PID_FAULT_NONE = 0,
    FLOW_PID_FAULT_SENSOR_INVALID = 1,
    FLOW_PID_FAULT_SENSOR_STALE = 2,
    FLOW_PID_FAULT_TIMING = 3
};

struct FlowPidConfig
{
    uint32_t intervalMs = 100;
    float deadband = 0.08f;
    float kp = 80.0f;
    float ki = 8.0f;
    float integralLimit = 30.0f;
    int maxStep = 32;
    float filterAlpha = 0.30f;
    float minActive = 0.20f;
    float kd = 6.0f;
    uint32_t maxDtMs = 1000;
    uint32_t sensorStaleMs = 1200;
    float discoveryThresholdLpm = 1.0f;
    int discoveryStepPwm = 100;
    uint32_t discoveryIntervalMs = 100;
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
    uint32_t lastUpdateMs = 0;
    SmartFlowMode mode = SMART_FLOW_PID_ONLY;
    FlowPidFault fault = FLOW_PID_FAULT_NONE;
    bool timingOk = true;
    bool outputLimited = false;
    float estimatedGain = 0.0f;
    float confidence = 0.0f;
    bool modelValid = false;
    bool discovering = false;
    int discoveredPwm = -1;
    float discoveryThresholdLpm = 0.0f;
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
};

class FlowMotorController
{
public:
    static bool validateConfig(const FlowPidConfig &config, int maxPwm)
    {
        if (config.intervalMs < 100 || config.intervalMs > 10000)
            return false;
        if (isnan(config.deadband) || config.deadband < 0.0f || config.deadband > 5.0f)
            return false;
        if (isnan(config.kp) || config.kp < 0.0f || config.kp > 1000.0f)
            return false;
        if (isnan(config.ki) || config.ki < 0.0f || config.ki > 200.0f)
            return false;
        if (isnan(config.integralLimit) || config.integralLimit < 0.0f || config.integralLimit > 500.0f)
            return false;
        if (isnan(config.kd) || config.kd < 0.0f || config.kd > 500.0f)
            return false;
        if (config.maxStep <= 0 || config.maxStep > maxPwm)
            return false;
        if (isnan(config.filterAlpha) || config.filterAlpha <= 0.0f || config.filterAlpha > 1.0f)
            return false;
        if (isnan(config.minActive) || config.minActive < 0.0f || config.minActive > 5.0f)
            return false;
        if (config.maxDtMs < config.intervalMs || config.maxDtMs > 30000)
            return false;
        if (config.sensorStaleMs < config.intervalMs || config.sensorStaleMs > 30000)
            return false;
        if (isnan(config.discoveryThresholdLpm) || config.discoveryThresholdLpm < 0.0f || config.discoveryThresholdLpm > 5.0f)
            return false;
        if (config.discoveryStepPwm <= 0 || config.discoveryStepPwm > maxPwm)
            return false;
        if (config.discoveryIntervalMs < 50 || config.discoveryIntervalMs > config.intervalMs)
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
        tune.maxStep = config.maxStep;
        tune.maxFeedForwardStep = config.maxStep;
        return tune;
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
        _discovering = false;
        _discoveredPwm = -1;
        _discoveryThresholdLpm = 0.0f;
    }

    bool isInitialized() const
    {
        return _initialized;
    }

    FlowMotorOutput update(const FlowMotorInput &input, const FlowPidConfig &config)
    {
        FlowMotorOutput output;
        output.pwm = _currentPwm;
        output.initialized = _initialized;
        output.fault = _fault;
        output.smartStatus = _smartStatus;

        uint32_t updateIntervalMs = _discovering ? config.discoveryIntervalMs : config.intervalMs;
        if (_initialized && input.nowMs - _lastUpdateMs < updateIntervalMs)
            return output;

        if (!_initialized || fabsf(_targetFlow - input.targetFlow) > 0.001f)
        {
            bool targetChanged = _initialized;
            _initialized = true;
            _smart.setTune(tuneFromConfig(config));
            if (targetChanged)
                _smart.resetController(true);
            _smartStatus = SmartFlowStatus();
            _targetFlow = input.targetFlow;
            _discovering = config.discoveryThresholdLpm > 0.0f && input.targetFlow > config.discoveryThresholdLpm;
            _discoveredPwm = -1;
            _discoveryThresholdLpm = config.discoveryThresholdLpm;
            _lastUpdateMs = input.nowMs;
            _currentPwm = input.currentPwm;
            output.updated = true;
            output.initialized = true;
            output.pwm = _currentPwm;
            output.smartStatus = _smartStatus;
            return output;
        }

        bool sensorTooOld = input.flowStale || input.flowAgeMs > config.sensorStaleMs;
        if (!input.flowValid || !input.flowFresh || sensorTooOld)
        {
            _measuredFlow = -1.0f;
            _fault = input.flowValid ? FLOW_PID_FAULT_SENSOR_STALE : FLOW_PID_FAULT_SENSOR_INVALID;
            _lastUpdateMs = input.nowMs;
            _smart.resetController(true);
            _discovering = config.discoveryThresholdLpm > 0.0f && input.targetFlow > config.discoveryThresholdLpm;
            _discoveredPwm = -1;
            _discoveryThresholdLpm = config.discoveryThresholdLpm;
            output.fault = _fault;
            return output;
        }

        _measuredFlow = input.measuredFlow;
        uint32_t dtMs = input.nowMs - _lastUpdateMs;
        float dtSeconds = dtMs / 1000.0f;
        _lastUpdateMs = input.nowMs;
        _lastDtSeconds = dtSeconds;
        _timingOk = dtMs <= config.maxDtMs;
        if (!_timingOk)
        {
            _fault = FLOW_PID_FAULT_TIMING;
            _smart.resetController(true);
            output.fault = _fault;
            return output;
        }
        _fault = FLOW_PID_FAULT_NONE;
        _smart.setTune(tuneFromConfig(config));

        if (_discovering)
        {
            if (input.measuredFlow >= config.discoveryThresholdLpm)
            {
                _discovering = false;
                _discoveredPwm = input.currentPwm;
                _smart.resetController(true);
            }
            else
            {
                int nextPwm = input.currentPwm + config.discoveryStepPwm;
                if (nextPwm > input.maxPwm)
                    nextPwm = input.maxPwm;
                _currentPwm = nextPwm;
                _filteredFlow = input.measuredFlow;
                _smartStatus = SmartFlowStatus();
                _smartStatus.pwm = _currentPwm;
                _smartStatus.rawFlow = input.measuredFlow;
                _smartStatus.fastFlow = input.measuredFlow;
                _smartStatus.slowFlow = input.measuredFlow;
                _smartStatus.errorFast = input.targetFlow - input.measuredFlow;
                _smartStatus.errorSlow = input.targetFlow - input.measuredFlow;
                _smartStatus.step = config.discoveryStepPwm;
                _smartStatus.mode = SMART_FLOW_MIN_ACTIVE_BOOST;

                output.updated = true;
                output.initialized = true;
                output.pwm = _currentPwm;
                output.fault = _fault;
                output.smartStatus = _smartStatus;
                return output;
            }
        }

        _smartStatus = _smart.update(input.nowMs, input.currentPwm, input.targetFlow, input.measuredFlow, dtSeconds, input.maxPwm);
        _filteredFlow = _smartStatus.fastFlow;
        _currentPwm = _smartStatus.pwm;

        output.updated = true;
        output.initialized = true;
        output.pwm = _currentPwm;
        output.fault = _fault;
        output.smartStatus = _smartStatus;
        return output;
    }

    FlowPidStatus status(bool enabled, bool testRunning, float testTargetFlow) const
    {
        FlowPidStatus status;
        status.enabled = enabled;
        status.running = _initialized || testRunning;
        status.flowValid = _measuredFlow >= 0.0f;
        status.targetFlow = _targetFlow >= 0.0f ? _targetFlow : testTargetFlow;
        status.measuredFlow = _measuredFlow;
        status.filteredFlow = _filteredFlow;
        status.error = status.filteredFlow >= 0.0f ? status.targetFlow - status.filteredFlow : 0.0f;
        status.integral = _smartStatus.integral;
        status.dtSeconds = _lastDtSeconds;
        status.pTerm = _smartStatus.pTerm;
        status.iTerm = _smartStatus.iTerm;
        status.dTerm = _smartStatus.dTerm;
        status.pwm = _currentPwm;
        status.lastUpdateMs = _lastUpdateMs;
        status.mode = _smartStatus.mode;
        status.fault = _fault;
        status.timingOk = _timingOk;
        status.outputLimited = _smartStatus.lowerClamp || _smartStatus.upperSaturation;
        status.estimatedGain = _smartStatus.estimatedGain;
        status.confidence = _smartStatus.confidence;
        status.modelValid = _smartStatus.modelValid;
        status.discovering = _discovering;
        status.discoveredPwm = _discoveredPwm;
        status.discoveryThresholdLpm = _discoveryThresholdLpm;
        return status;
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
    bool _discovering = false;
    int _discoveredPwm = -1;
    float _discoveryThresholdLpm = 0.0f;
};

#endif
