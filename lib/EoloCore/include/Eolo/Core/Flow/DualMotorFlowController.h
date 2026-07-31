#ifndef EOLO_CORE_FLOW_DUAL_MOTOR_FLOW_CONTROLLER_H
#define EOLO_CORE_FLOW_DUAL_MOTOR_FLOW_CONTROLLER_H

#include <stdint.h>
#include <math.h>
#include <Eolo/Core/Flow/FlowMotorController.h>

struct DualMotorFlowInput
{
    uint32_t nowMs = 0;
    float targetFlow = 0.0f;
    float measuredFlow = -1.0f;
    bool flowValid = false;
    bool flowFresh = false;
    bool flowStale = false;
    uint32_t flowAgeMs = 0;
    uint32_t sampleId = 0;
    int maxPwm = 4095;
    int primaryMotor = 0;
    bool hasFeedForward = false;
    int feedForwardPrimary = 0;
    int feedForwardSecondary = 0;
};

struct DualMotorFlowOutput
{
    bool updated = false;
    int virtualPwm = 0;
    int motor0Pwm = 0;
    int motor1Pwm = 0;
    FlowPidFault fault = FLOW_PID_FAULT_NONE;
    bool sensorGraceActive = false;
    bool stoppedForSensorFault = false;
    FlowMotorOutput controller;
    SmartFlowStatus smartStatus;
    bool kickActive = false;
    uint16_t kickCount = 0;
    bool stallDetected = false;
};

// Adapta FlowMotorController a dos bombas en serie. El PWM virtual va de
// 0..2*MAX: primero llena la bomba primaria y luego la secundaria. La curva
// guardada solo entrega la semilla inicial; el PID siempre conserva el ajuste.
class DualMotorFlowController
{
public:
    void reset()
    {
        _controller.reset();
        _lastSampleId = 0;
        _lastTarget = -1.0f;
        _virtualPwm = 0;
        _graceStartMs = 0;
        _graceActive = false;
        _faultStopped = false;
    }

    static int virtualFromMotors(int primary, int secondary, int maxPwm)
    {
        if (primary < 0) primary = 0;
        if (secondary < 0) secondary = 0;
        if (primary > maxPwm) primary = maxPwm;
        if (secondary > maxPwm) secondary = maxPwm;
        // La representación canónica no permite secundario antes de que la
        // primaria esté saturada.
        return secondary > 0 ? maxPwm + secondary : primary;
    }

    static void motorsFromVirtual(int virtualPwm, int maxPwm, int &primary, int &secondary)
    {
        if (virtualPwm < 0) virtualPwm = 0;
        if (virtualPwm > maxPwm * 2) virtualPwm = maxPwm * 2;
        primary = virtualPwm > maxPwm ? maxPwm : virtualPwm;
        secondary = virtualPwm > maxPwm ? virtualPwm - maxPwm : 0;
    }

    DualMotorFlowOutput update(const DualMotorFlowInput &in, const FlowPidConfig &config)
    {
        DualMotorFlowOutput out;
        const bool targetActive = in.targetFlow > config.minActive;
        const bool targetChanged = fabsf(_lastTarget - in.targetFlow) > 0.001f;
        _lastTarget = in.targetFlow;

        if (!targetActive)
        {
            _controller.reset();
            _virtualPwm = 0;
            _graceStartMs = 0;
            _graceActive = false;
            _faultStopped = false;
            out.updated = true;
            return mapOutput(out, in.primaryMotor, in.maxPwm);
        }

        if (targetChanged && in.hasFeedForward)
        {
            _virtualPwm = virtualFromMotors(in.feedForwardPrimary, in.feedForwardSecondary, in.maxPwm);
            _controller.reset();
            _controller.seedRunning(_virtualPwm);
            _graceStartMs = 0;
            _graceActive = false;
            _faultStopped = false;
        }

        const bool newSample = in.flowValid && in.flowFresh && in.sampleId != 0 && in.sampleId != _lastSampleId;
        if (newSample)
        {
            _lastSampleId = in.sampleId;
            _graceStartMs = 0;
            _graceActive = false;
            _faultStopped = false;
            FlowMotorInput pidIn;
            pidIn.nowMs = in.nowMs;
            pidIn.currentPwm = _virtualPwm;
            pidIn.targetFlow = in.targetFlow;
            pidIn.measuredFlow = in.measuredFlow;
            pidIn.flowValid = true;
            pidIn.flowFresh = true;
            pidIn.flowStale = false;
            pidIn.flowAgeMs = in.flowAgeMs;
            pidIn.maxPwm = in.maxPwm * 2;
            out.controller = _controller.update(pidIn, config);
            out.fault = out.controller.fault;
            out.smartStatus = out.controller.smartStatus;
            out.kickActive = out.controller.kickActive;
            out.kickCount = out.controller.kickCount;
            out.stallDetected = out.controller.stallDetected;
            if (out.controller.updated) _virtualPwm = out.controller.pwm;
            out.updated = out.controller.updated;
        }
        else
        {
            if (!_graceActive) { _graceStartMs = in.nowMs; _graceActive = true; }
            out.sensorGraceActive = true;
            out.fault = in.flowValid ? FLOW_PID_FAULT_SENSOR_STALE : FLOW_PID_FAULT_SENSOR_INVALID;
            if ((uint32_t)(in.nowMs - _graceStartMs) > 6000UL)
            {
                _virtualPwm = 0;
                _faultStopped = true;
                out.updated = true;
                out.stoppedForSensorFault = true;
            }
        }
        out.virtualPwm = _virtualPwm;
        return mapOutput(out, in.primaryMotor, in.maxPwm);
    }

    FlowPidStatus status(bool enabled, bool running, float target) const
    {
        FlowPidStatus result = _controller.status(enabled, running, target);
        result.pwm = _virtualPwm;
        return result;
    }

    void forceKick() { _controller.forceKick(); }

private:
    DualMotorFlowOutput mapOutput(DualMotorFlowOutput out, int primaryMotor, int maxPwm) const
    {
        int primary = 0, secondary = 0;
        motorsFromVirtual(_virtualPwm, maxPwm, primary, secondary);
        out.virtualPwm = _virtualPwm;
        if (primaryMotor == 1) { out.motor0Pwm = secondary; out.motor1Pwm = primary; }
        else { out.motor0Pwm = primary; out.motor1Pwm = secondary; }
        return out;
    }

    FlowMotorController _controller;
    uint32_t _lastSampleId = 0;
    uint32_t _graceStartMs = 0;
    bool _graceActive = false;
    float _lastTarget = -1.0f;
    int _virtualPwm = 0;
    bool _faultStopped = false;
};

#endif
