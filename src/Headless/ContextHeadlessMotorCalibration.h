#ifndef EOLO_DATA_CONTEXT_HEADLESS_MOTOR_CALIBRATION_H
#define EOLO_DATA_CONTEXT_HEADLESS_MOTOR_CALIBRATION_H

// Kept separate from the product flow: these legacy calibration bindings are
// available to existing callers only after Context is complete. They do not
// re-enable calibration in the Dron application.
#include "HeadlessMotorCalibration.h"
#include <Preferences.h>
#include <math.h>

inline void HeadlessMotorCalibration::resetRun()
{
    _pointCount = 0;
    _currentPwm = 0;
    _phaseStartMs = 0;
    _lastSampleMs = 0;
    _sampleCount = 0;
    _sampleSum = 0.0f;
    _sampleSqSum = 0.0f;
    _valid = false;
    _error = "";
}

inline bool HeadlessMotorCalibration::start(const HeadlessMotorCalibrationConfig &config)
{
    if (isRunning() || !validateConfig(config))
        return false;

    _config = config;
    resetRun();
    setState(State::Starting);
    return true;
}

inline void HeadlessMotorCalibration::setState(State state)
{
    _state = state;
    _phaseStartMs = millis();
}

inline bool HeadlessMotorCalibration::isRunning() const
{
    return _state == State::Starting || _state == State::SetPwm || _state == State::Settling ||
           _state == State::Sampling || _state == State::StorePoint || _state == State::Validate;
}

inline bool HeadlessMotorCalibration::isFinished() const
{
    return _state == State::Done || _state == State::Error || _state == State::Aborted;
}

inline const char *HeadlessMotorCalibration::stateText() const
{
    switch (_state)
    {
    case State::Idle: return "idle";
    case State::Starting: return "starting";
    case State::SetPwm: return "set_pwm";
    case State::Settling: return "settling";
    case State::Sampling: return "sampling";
    case State::StorePoint: return "store_point";
    case State::Validate: return "validate";
    case State::Done: return "done";
    case State::Error: return "error";
    case State::Aborted: return "aborted";
    default: return "unknown";
    }
}

inline float HeadlessMotorCalibration::minFlow() const
{
    return _pointCount > 0 ? _points[0].flow : 0.0f;
}

inline float HeadlessMotorCalibration::maxFlow() const
{
    return _pointCount > 0 ? _points[_pointCount - 1].flow : 0.0f;
}

inline void HeadlessMotorCalibration::fail(Context &ctx, const char *error)
{
    _error = error;
    ctx.components.motor.setPwmImmediate(0);
    setState(State::Error);
    LOG_OUT("Calibracion motor Dron fallida: ");
    LOG_OUT_LN(error);
}

inline void HeadlessMotorCalibration::abort(Context &ctx)
{
    if (!isRunning())
        return;
    ctx.components.motor.setPwmImmediate(0);
    _error = "abortada";
    setState(State::Aborted);
    LOG_LN("Calibracion motor Dron abortada.");
}

inline void HeadlessMotorCalibration::clear(Context &ctx)
{
    resetRun();
    setState(State::Idle);
    ctx.calibration.numPoints = 0;
    ctx.calibration.isLoaded = false;
    Preferences preferences;
    preferences.begin("eolo_calib", false);
    preferences.clear();
    preferences.end();
    ctx.components.motor.setPwmImmediate(0);
    LOG_LN("Calibracion motor Dron borrada.");
}

inline bool HeadlessMotorCalibration::save(Context &ctx)
{
    if (!ctx.calibration.validate())
        return false;
    ctx.calibration.save();
    ctx.calibration.isLoaded = true;
    return true;
}

inline bool HeadlessMotorCalibration::storeCurrentPoint()
{
    MotorCalibrationSampleStats stats;
    stats.count = _sampleCount;
    stats.sum = _sampleSum;
    stats.squareSum = _sampleSqSum;

    MotorCalibrationPoint point;
    MotorCalibrationPoint previousPoint;
    const MotorCalibrationPoint *previous = nullptr;
    if (_pointCount > 0)
    {
        previousPoint.pwm = _points[_pointCount - 1].pwm;
        previousPoint.flow = _points[_pointCount - 1].flow;
        previousPoint.stddev = _points[_pointCount - 1].stddev;
        previousPoint.samples = _points[_pointCount - 1].samples;
        previous = &previousPoint;
    }
    if (_pointCount >= MaxPoints ||
        !MotorCalibrationModel::makePoint(stats, _currentPwm,
                                          _config.minValidFlow,
                                          _config.minFlowDelta,
                                          _config.maxFlowStddev,
                                          previous, point))
        return false;

    _points[_pointCount].pwm = point.pwm;
    _points[_pointCount].flow = point.flow;
    _points[_pointCount].stddev = point.stddev;
    _points[_pointCount].samples = point.samples;
    _pointCount++;

    LOG_OUT("Calibracion punto: PWM ");
    LOG_OUT(_currentPwm);
    LOG_OUT(" -> ");
    LOG_OUT(point.flow, 2);
    LOG_OUT(" L/min sd ");
    LOG_OUT_LN(point.stddev, 3);
    return true;
}

inline bool HeadlessMotorCalibration::validateAndApply(Context &ctx)
{
    ctx.calibration.numPoints = _pointCount;
    ctx.calibration.weakMotor = 0;
    for (int i = 0; i < _pointCount; i++)
    {
        ctx.calibration.pwm0[i] = _points[i].pwm;
        ctx.calibration.pwm1[i] = 0;
        ctx.calibration.flows[i] = _points[i].flow;
    }
    ctx.calibration.sortByFlow();
    ctx.calibration.isLoaded = ctx.calibration.validate();
    if (!ctx.calibration.isLoaded)
        return false;

    for (int i = 0; i < ctx.calibration.numPoints; i++)
    {
        _points[i].pwm = ctx.calibration.pwm0[i];
        _points[i].flow = ctx.calibration.flows[i];
    }
    _valid = true;
    ctx.calibration.save();
    return true;
}

inline void HeadlessMotorCalibration::update(Context &ctx)
{
    if (!isRunning())
        return;

    const uint32_t now = millis();
    switch (_state)
    {
    case State::Starting:
        ctx.components.motor.setPwmImmediate(0);
        _currentPwm = _config.pwmStart;
        setState(State::SetPwm);
        break;

    case State::SetPwm:
        ctx.components.motor.setMotorPwm(0, _currentPwm);
        _sampleCount = 0;
        _sampleSum = 0.0f;
        _sampleSqSum = 0.0f;
        _lastSampleMs = 0;
        setState(State::Settling);
        break;

    case State::Settling:
        if (now - _phaseStartMs >= _config.settleMs)
            setState(State::Sampling);
        break;

    case State::Sampling:
        if (_lastSampleMs == 0 || now - _lastSampleMs >= _config.sampleIntervalMs)
        {
            _lastSampleMs = now;
            FlowData flowData;
            if (ctx.components.flowSensor.getData(flowData) && flowData.valid)
            {
                _sampleCount++;
                _sampleSum += flowData.flow;
                _sampleSqSum += flowData.flow * flowData.flow;
            }
        }
        if (_sampleCount >= _config.samplesPerPoint)
            setState(State::StorePoint);
        else if (now - _phaseStartMs > (_config.sampleIntervalMs * (_config.samplesPerPoint + 3UL)))
            fail(ctx, "sin lecturas AFM07 validas");
        break;

    case State::StorePoint:
        storeCurrentPoint();
        if (_pointCount >= MaxPoints || _currentPwm >= _config.pwmEnd || maxFlow() >= _config.maxTargetFlow)
        {
            setState(State::Validate);
            break;
        }
        _currentPwm += _config.pwmStep;
        if (_currentPwm > _config.pwmEnd)
            _currentPwm = _config.pwmEnd;
        setState(State::SetPwm);
        break;

    case State::Validate:
        ctx.components.motor.setPwmImmediate(0);
        if (!validateAndApply(ctx))
        {
            fail(ctx, "curva invalida o rango insuficiente");
            break;
        }
        LOG_OUT("Calibracion motor Dron OK: ");
        LOG_OUT(_pointCount);
        LOG_OUT(" puntos, rango ");
        LOG_OUT(minFlow(), 2);
        LOG_OUT(" - ");
        LOG_OUT(maxFlow(), 2);
        LOG_OUT_LN(" L/min");
        setState(State::Done);
        break;

    default:
        break;
    }
}

inline String HeadlessMotorCalibration::statusJson() const
{
    String json = "{";
    json += "\"state\":\"" + String(stateText()) + "\"";
    json += ",\"pwm\":" + String(currentPwm());
    json += ",\"points\":" + String(pointCount());
    json += ",\"valid\":" + String(_valid ? "true" : "false");
    json += ",\"error\":\"" + String(errorText()) + "\"";
    json += "}";
    return json;
}

#endif // EOLO_DATA_CONTEXT_HEADLESS_MOTOR_CALIBRATION_H
