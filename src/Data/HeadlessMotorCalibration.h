#ifndef EOLO_HEADLESS_MOTOR_CALIBRATION_H
#define EOLO_HEADLESS_MOTOR_CALIBRATION_H

#include <Arduino.h>
#include <stdint.h>
#include "../Config.h"
#include "../Effectors/Motor.h"

struct Context;

struct HeadlessMotorCalibrationConfig
{
#if defined(FEATURE_MOTOR_PWM_8BIT)
    int pwmStart = 40;
    int pwmEnd = 255;
    int pwmStep = 8;
#else
    int pwmStart = 400;
    int pwmEnd = 1900;
    int pwmStep = 80;
#endif
    uint32_t settleMs = 3000;
    uint32_t sampleIntervalMs = 800;
    uint8_t samplesPerPoint = 5;
    float maxTargetFlow = 8.0f;
    float minValidFlow = 0.1f;
    float minFlowDelta = 0.05f;
    float maxFlowStddev = 0.15f;
};

struct HeadlessMotorCalibrationPoint
{
    int pwm = 0;
    float flow = 0.0f;
    float stddev = 0.0f;
    uint8_t samples = 0;
};

class HeadlessMotorCalibration
{
public:
    enum class State : uint8_t
    {
        Idle,
        Starting,
        SetPwm,
        Settling,
        Sampling,
        StorePoint,
        Validate,
        Done,
        Error,
        Aborted
    };

    static constexpr int MaxPoints = 64;

    bool start(const HeadlessMotorCalibrationConfig &config = HeadlessMotorCalibrationConfig());
    void update(Context &ctx);
    void abort(Context &ctx);
    void clear(Context &ctx);
    bool save(Context &ctx);

    bool isRunning() const;
    bool isFinished() const;
    bool hasValidCalibration() const { return _valid; }
    State state() const { return _state; }
    const char *stateText() const;
    const char *errorText() const { return _error; }
    const HeadlessMotorCalibrationConfig &config() const { return _config; }
    int currentPwm() const { return _currentPwm; }
    int pointCount() const { return _pointCount; }
    float minFlow() const;
    float maxFlow() const;
    const HeadlessMotorCalibrationPoint &point(int index) const { return _points[index]; }
    String statusJson() const;

    static bool validateConfig(const HeadlessMotorCalibrationConfig &config)
    {
        if (config.pwmStart < 0 || config.pwmEnd > MAX_PWM || config.pwmStart >= config.pwmEnd)
            return false;
        if (config.pwmStep <= 0 || config.pwmStep > MAX_PWM)
            return false;
        if (config.settleMs < 100 || config.sampleIntervalMs < 100 || config.samplesPerPoint == 0)
            return false;
        if (config.maxTargetFlow <= 0.0f || config.minValidFlow < 0.0f)
            return false;
        if (config.minFlowDelta < 0.0f || config.maxFlowStddev < 0.0f)
            return false;
        return true;
    }

    static int pwm8ToRuntime(int pwm8)
    {
        if (pwm8 < 0)
            pwm8 = 0;
        if (pwm8 > 255)
            pwm8 = 255;
        return (pwm8 * MAX_PWM + 127) / 255;
    }

private:
    HeadlessMotorCalibrationConfig _config;
    State _state = State::Idle;
    HeadlessMotorCalibrationPoint _points[MaxPoints];
    int _pointCount = 0;
    int _currentPwm = 0;
    uint32_t _phaseStartMs = 0;
    uint32_t _lastSampleMs = 0;
    uint8_t _sampleCount = 0;
    float _sampleSum = 0.0f;
    float _sampleSqSum = 0.0f;
    bool _valid = false;
    const char *_error = "";

    void setState(State state);
    void fail(Context &ctx, const char *error);
    bool storeCurrentPoint();
    bool validateAndApply(Context &ctx);
    void resetRun();
};

#endif // EOLO_HEADLESS_MOTOR_CALIBRATION_H


#if defined(CONTEXT_CLASS_DEFINED) && !defined(EOLO_HEADLESS_MOTOR_CALIBRATION_IMPL_DONE)
#define EOLO_HEADLESS_MOTOR_CALIBRATION_IMPL_DONE

#include "Context.h"
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
    if (_sampleCount == 0)
        return false;

    float mean = _sampleSum / _sampleCount;
    float variance = (_sampleSqSum / _sampleCount) - (mean * mean);
    if (variance < 0.0f)
        variance = 0.0f;
    float stddev = sqrtf(variance);

    if (mean < _config.minValidFlow || stddev > _config.maxFlowStddev)
        return false;
    if (_pointCount > 0 && mean - _points[_pointCount - 1].flow < _config.minFlowDelta)
        return false;
    if (_pointCount >= MaxPoints)
        return false;

    _points[_pointCount].pwm = _currentPwm;
    _points[_pointCount].flow = mean;
    _points[_pointCount].stddev = stddev;
    _points[_pointCount].samples = _sampleCount;
    _pointCount++;

    LOG_OUT("Calibracion punto: PWM ");
    LOG_OUT(_currentPwm);
    LOG_OUT(" -> ");
    LOG_OUT(mean, 2);
    LOG_OUT(" L/min sd ");
    LOG_OUT_LN(stddev, 3);
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

    uint32_t now = millis();
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

#endif
