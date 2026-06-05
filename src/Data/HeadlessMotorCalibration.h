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

#endif
