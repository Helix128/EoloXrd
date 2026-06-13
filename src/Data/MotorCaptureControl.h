#ifndef EOLO_MOTOR_CAPTURE_CONTROL_H
#define EOLO_MOTOR_CAPTURE_CONTROL_H

#include <Arduino.h>
#include <math.h>
#include "../Config.h"
#include "../Effectors/Motor.h"
#include <Eolo/Core/Flow/FlowMotorController.h>

struct Context;

class MotorCaptureControl
{
#if defined(FEATURE_FLOW_PID)
    FlowMotorController pid;

    FlowPidConfig pidCfg = {
        FLOW_PID_INTERVAL_MS,
        FLOW_PID_DEADBAND,
        FLOW_PID_KP,
        FLOW_PID_KI,
        FLOW_PID_INTEGRAL_LIMIT,
        FLOW_PID_MAX_STEP,
        FLOW_PID_FILTER_ALPHA,
        FLOW_PID_MIN_ACTIVE,
        FLOW_PID_KD,
        FLOW_PID_MAX_DT_MS,
        FLOW_PID_SENSOR_STALE_MS
    };
    bool pidTestRunning = false;
    float pidTestTargetFlow = DRONE_TARGET_FLOW_LPM;
#endif
    uint32_t lastMotorOverheatLogMs = 0;

#if defined(FEATURE_FLOW_PID)
    void updatePidMotors(Context &ctx);
#endif

public:
    bool motorOverheatActive = false;
    bool motorThermalSensorValid = false;
    float motorThermalTemperature = -99.0f;

    static bool validatePidConfig(const FlowPidConfig &config)
    {
        return FlowMotorController::validateConfig(config, MAX_PWM);
    };
    bool updateThermalProtection(Context &ctx);
    void resetFlowController();
    void updateMotors(Context &ctx);

#if defined(FEATURE_FLOW_PID)
    const FlowPidConfig &getPidConfig() const { return pidCfg; }
    void setPidConfig(const FlowPidConfig &config) { if (validatePidConfig(config)) pidCfg = config; }
    bool isPidTestRunning() const { return pidTestRunning; }
    void startPidTest(float targetFlow) { pidTestTargetFlow = targetFlow; pidTestRunning = true; resetFlowController(); }
    void stopPidTest(Context &ctx);
    FlowPidStatus getPidStatus() const;
#endif
};

#endif // EOLO_MOTOR_CAPTURE_CONTROL_H


#if defined(CONTEXT_CLASS_DEFINED) && !defined(EOLO_MOTOR_CAPTURE_CONTROL_IMPL_DONE)
#define EOLO_MOTOR_CAPTURE_CONTROL_IMPL_DONE

#include "Context.h"

inline bool MotorCaptureControl::updateThermalProtection(Context &ctx)
{
#ifdef FEATURE_NTC
    bool previousActive = motorOverheatActive;
    bool previousValid = motorThermalSensorValid;
    float previousTemperature = motorThermalTemperature;

    NTCData ntcData;
    motorThermalSensorValid = ctx.components.ntc.getData(ntcData);
    motorThermalTemperature = motorThermalSensorValid ? ntcData.temperature : -99.0f;
    motorOverheatActive = NTC::motorOverheatLatched(
        motorOverheatActive,
        motorThermalSensorValid,
        motorThermalTemperature,
        NTC_MOTOR_OVERHEAT_HIGH_C,
        NTC_MOTOR_OVERHEAT_LOW_C);

    uint32_t nowMs = millis();
    if (motorOverheatActive)
    {
        ctx.components.motor.setPwmImmediate(0);
        resetFlowController();
    }

    if (!previousActive && motorOverheatActive)
    {
        LOG_OUT("ERROR MOTOR OVERHEAT: NTC ");
        if (motorThermalSensorValid)
            LOG_OUT(motorThermalTemperature, 1);
        else
            LOG_OUT("INVALID");
        LOG_OUT(" C >= ");
        LOG_OUT(NTC_MOTOR_OVERHEAT_HIGH_C, 1);
        LOG_OUT_LN(" C; motor OFF");
        lastMotorOverheatLogMs = nowMs;
    }
    else if (previousActive && !motorOverheatActive)
    {
        resetFlowController();
        LOG_OUT("Motor thermal cooldown OK: NTC ");
        LOG_OUT(motorThermalTemperature, 1);
        LOG_OUT(" C <= ");
        LOG_OUT(NTC_MOTOR_OVERHEAT_LOW_C, 1);
        LOG_OUT_LN(" C; motor enabled");
    }
    else if (motorOverheatActive && nowMs - lastMotorOverheatLogMs >= NTC_MOTOR_OVERHEAT_LOG_INTERVAL_MS)
    {
        LOG_OUT("ERROR MOTOR OVERHEAT activo: ");
        if (motorThermalSensorValid)
        {
            LOG_OUT("NTC ");
            LOG_OUT(motorThermalTemperature, 1);
            LOG_OUT(" C, cooldown <= ");
            LOG_OUT(NTC_MOTOR_OVERHEAT_LOW_C, 1);
            LOG_OUT_LN(" C; motor OFF");
        }
        else
        {
            LOG_OUT_LN("NTC invalido; motor OFF");
        }
        lastMotorOverheatLogMs = nowMs;
    }

    if (previousActive != motorOverheatActive ||
        previousValid != motorThermalSensorValid ||
        fabsf(previousTemperature - motorThermalTemperature) > 0.1f)
    {
        ctx.markUiDirty();
    }
    return motorOverheatActive;
#else
    motorOverheatActive = false;
    motorThermalSensorValid = false;
    motorThermalTemperature = -1.0f;
    return false;
#endif
}

inline void MotorCaptureControl::resetFlowController()
{
#if defined(FEATURE_FLOW_PID)
    pid.reset();
#endif
}

#if defined(FEATURE_FLOW_PID)
inline void MotorCaptureControl::stopPidTest(Context &ctx)
{
    pidTestRunning = false;
    resetFlowController();
    ctx.components.motor.setPwmImmediate(0);
}

inline FlowPidStatus MotorCaptureControl::getPidStatus() const
{
    return pid.status(true, pidTestRunning, pidTestTargetFlow);
}

inline void MotorCaptureControl::updatePidMotors(Context &ctx)
{
    uint32_t nowMs = millis();
    float targetFlow = pidTestRunning ? pidTestTargetFlow : ctx.session.targetFlow;
    int currentPwm = ctx.components.motor.getMotorPwm(0);

    FlowData flowData;
    bool flowReadValid = ctx.components.flowSensor.getData(flowData) && flowData.valid;
    bool flowFresh = flowReadValid && flowData.fresh && !flowData.stale && flowData.ageMs <= pidCfg.sensorStaleMs;
    if (!flowFresh)
    {
        resetFlowController();
        ctx.components.motor.setPwmImmediate(0);
        static uint32_t lastSensorFaultLogMs = 0;
        if (nowMs - lastSensorFaultLogMs >= 5000)
        {
            LOG_OUT("PID flujo sensor no fresco: valid=");
            LOG_OUT(flowReadValid ? "si" : "no");
            LOG_OUT(" ageMs=");
            LOG_OUT_LN(flowData.ageMs);
            lastSensorFaultLogMs = nowMs;
        }
        return;
    }

    FlowMotorInput input;
    input.nowMs = nowMs;
    input.currentPwm = currentPwm;
    input.targetFlow = targetFlow;
    input.measuredFlow = flowFresh ? flowData.flow : -1.0f;
    input.flowValid = flowReadValid;
    input.flowFresh = flowFresh;
    input.flowStale = flowReadValid && !flowFresh;
    input.flowAgeMs = flowData.ageMs;
    input.maxPwm = MAX_PWM;

    FlowMotorOutput output = pid.update(input, pidCfg);
    if (!output.updated)
        return;

    if (output.fault != FLOW_PID_FAULT_NONE)
    {
        ctx.components.motor.setPwmImmediate(0);
        return;
    }

    ctx.components.motor.setMotorPwm(0, output.pwm);
    for (int i = 1; i < MotorManager::motorCount; i++)
        ctx.components.motor.setMotorPwm(i, 0);

    static uint32_t lastPidLogMs = 0;
    if (nowMs - lastPidLogMs >= 1000)
    {
        LOG_OUT("PID flujo: medido ");
        LOG_OUT(flowData.flow, 2);
        LOG_OUT(" filtrado ");
        LOG_OUT(output.smartStatus.fastFlow, 2);
        LOG_OUT(" L/min PWM ");
        LOG_OUT(output.pwm);
        LOG_OUT(" P/I/D ");
        LOG_OUT(output.smartStatus.pTerm, 1);
        LOG_OUT("/");
        LOG_OUT(output.smartStatus.iTerm, 1);
        LOG_OUT("/");
        LOG_OUT(output.smartStatus.dTerm, 1);
        LOG_OUT(" modo ");
        LOG_OUT(static_cast<int>(output.smartStatus.mode));
        LOG_OUT(" gain ");
        LOG_OUT(output.smartStatus.estimatedGain, 5);
        LOG_OUT(" conf ");
        LOG_OUT_LN(output.smartStatus.confidence, 2);
        lastPidLogMs = nowMs;
    }
}
#endif

inline void MotorCaptureControl::updateMotors(Context &ctx)
{
    if (motorOverheatActive)
    {
        ctx.components.motor.setPwmImmediate(0);
        resetFlowController();
        return;
    }

#if defined(FEATURE_FLOW_PID)
    updatePidMotors(ctx);
#elif defined(FEATURE_FLOW_CALIBRATION)
    static float lastTargetFlow = -1.0f;

    int p0 = 0, p1 = 0;
    if (ctx.calibration.getMotorPwms(ctx.session.targetFlow, p0, p1))
    {
        if (lastTargetFlow != ctx.session.targetFlow)
        {
            LOG_OUT("Flujo objetivo: ");
            LOG_OUT(ctx.session.targetFlow, 2);
            LOG_OUT(" L/min -> PWM[");
            LOG_OUT(p0);
            LOG_OUT(", ");
            LOG_OUT(p1);
            LOG_OUT_LN("]");
            lastTargetFlow = ctx.session.targetFlow;
        }

        ctx.components.motor.setMotorPwm(0, p0);
        ctx.components.motor.setMotorPwm(1, p1);
    }
    else
    {
        ctx.components.motor.setPwm(0);
    }
#else
    ctx.components.motor.setPwm(0);
#endif
}

#endif
