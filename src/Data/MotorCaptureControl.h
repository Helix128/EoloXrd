#ifndef EOLO_MOTOR_CAPTURE_CONTROL_H
#define EOLO_MOTOR_CAPTURE_CONTROL_H

#include <Arduino.h>
#include <math.h>
#include "../Config/Legacy.h"
#include "../Effectors/Motor.h"
#include "CalibrationManager.h"
#include <Eolo/Core/Flow/DualMotorFlowController.h>
#include <Eolo/Core/Thermal/ThermalProtectionModel.h>
#include <Eolo/Types/FlowData.h>
#include <Eolo/Types/NTCData.h>

class MotorCaptureControl
{
#if defined(FEATURE_FLOW_PID) || defined(FEATURE_FLOW_CALIBRATION)
    DualMotorFlowController pid;

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
        FLOW_PID_SENSOR_STALE_MS,
        FLOW_PID_KICK_PWM,
        FLOW_PID_KICK_MS,
        FLOW_PID_STALL_FLOW_LPM,
        FLOW_PID_RESTALL_COOLDOWN_MS,
        FLOW_PID_STALL_CONFIRM_MS,
        FLOW_PID_SOFT_TRIM_MAX,
        FLOW_PID_SOFT_MAX_STEP,
        FLOW_PID_SENSITIVITY,
        FLOW_PID_RECENTER_DELAY_MS
    };
    bool pidTestRunning = false;
    float pidTestTargetFlow = DRONE_TARGET_FLOW_LPM;
    bool pidConfigLogged = false;
    uint16_t _lastLoggedKickCount = 0;
#endif
    uint32_t lastMotorOverheatLogMs = 0;

#if defined(FEATURE_FLOW_PID) || defined(FEATURE_FLOW_CALIBRATION)
    void updatePidMotors(MotorManager &motor, const FlowData &flowData,
                         bool flowReadValid, float targetFlow,
                         const CalibrationManager &calibration);
#endif

public:
    bool motorOverheatActive = false;
    bool motorThermalSensorValid = false;
    float motorThermalTemperature = -99.0f;

    static bool validatePidConfig(const FlowPidConfig &config)
    {
        return FlowMotorController::validateConfig(config, MAX_PWM);
    };
    bool updateThermalProtection(const NTCData &ntcData, bool ntcValid,
                                 MotorManager &motor);
    void resetFlowController();
    void updateMotors(MotorManager &motor, const FlowData &flowData,
                      bool flowReadValid, float targetFlow,
                      const CalibrationManager &calibration);

#if defined(FEATURE_FLOW_PID) || defined(FEATURE_FLOW_CALIBRATION)
    const FlowPidConfig &getPidConfig() const { return pidCfg; }
    void setPidConfig(const FlowPidConfig &config) { if (validatePidConfig(config)) pidCfg = config; }
    bool isPidTestRunning() const { return pidTestRunning; }
    void startPidTest(float targetFlow) { pidTestTargetFlow = targetFlow; pidTestRunning = true; resetFlowController(); }
    void stopPidTest(MotorManager &motor);
    FlowPidStatus getPidStatus() const;
    void forceIgnition() { pid.forceKick(); }
#endif
};

inline bool MotorCaptureControl::updateThermalProtection(const NTCData &ntcData,
                                                          bool ntcValid,
                                                          MotorManager &motor)
{
#ifdef FEATURE_NTC
    bool previousActive = motorOverheatActive;
    bool previousValid = motorThermalSensorValid;
    float previousTemperature = motorThermalTemperature;

    motorThermalSensorValid = ntcValid;
    motorThermalTemperature = motorThermalSensorValid ? ntcData.temperature : -99.0f;
    ThermalProtectionInput thermalInput;
    thermalInput.latched = motorOverheatActive;
    thermalInput.sensorValid = motorThermalSensorValid;
    thermalInput.temperature = motorThermalTemperature;
    thermalInput.highThreshold = NTC_MOTOR_OVERHEAT_HIGH_C;
    thermalInput.lowThreshold = NTC_MOTOR_OVERHEAT_LOW_C;
    const ThermalProtectionOutput thermalOutput = ThermalProtectionModel::update(thermalInput);
    motorOverheatActive = thermalOutput.latched;

    uint32_t nowMs = millis();
    if (motorOverheatActive)
    {
        motor.setPwmImmediate(0);
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

    (void)previousActive;
    (void)previousValid;
    (void)previousTemperature;
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
#if defined(FEATURE_FLOW_PID) || defined(FEATURE_FLOW_CALIBRATION)
    pid.reset();
#endif
}

#if defined(FEATURE_FLOW_PID) || defined(FEATURE_FLOW_CALIBRATION)
inline void MotorCaptureControl::stopPidTest(MotorManager &motor)
{
    pidTestRunning = false;
    resetFlowController();
    motor.setPwmImmediate(0);
}

inline FlowPidStatus MotorCaptureControl::getPidStatus() const
{
    return pid.status(true, pidTestRunning, pidTestTargetFlow);
}

inline void MotorCaptureControl::updatePidMotors(MotorManager &motor,
                                                 const FlowData &flowData,
                                                 bool flowReadValid,
                                                 float targetFlow,
                                                 const CalibrationManager &calibration)
{
    uint32_t nowMs = millis();
    targetFlow = pidTestRunning ? pidTestTargetFlow : targetFlow;
    if (!pidConfigLogged)
    {
        LOG_OUT("PID config: interval=");
        LOG_OUT(pidCfg.intervalMs);
        LOG_OUT("ms stale=");
        LOG_OUT(pidCfg.sensorStaleMs);
        LOG_OUT("ms maxDt=");
        LOG_OUT(pidCfg.maxDtMs);
        LOG_OUT("ms Kp/Ki/Kd=");
        LOG_OUT(pidCfg.kp, 2);
        LOG_OUT("/");
        LOG_OUT(pidCfg.ki, 2);
        LOG_OUT("/");
        LOG_OUT(pidCfg.kd, 2);
        LOG_OUT(" step=");
        LOG_OUT(pidCfg.maxStep);
        LOG_OUT(" deadband=");
        LOG_OUT(pidCfg.deadband, 2);
        LOG_OUT(" alpha=");
        LOG_OUT(pidCfg.filterAlpha, 2);
        LOG_OUT(" minActive=");
        LOG_OUT(pidCfg.minActive, 2);
        LOG_OUT(" kick=" );
        LOG_OUT(pidCfg.kickPwm);
        LOG_OUT(" kickMs=");
        LOG_OUT(pidCfg.kickMs);
        LOG_OUT("ms stallFlow=");
        LOG_OUT(pidCfg.stallFlowLpm, 2);
        LOG_OUT("L/min cooldown=");
        LOG_OUT(pidCfg.restallCooldownMs);
        LOG_OUT("ms stallConfirm=");
        LOG_OUT(pidCfg.stallConfirmMs);
        LOG_OUT_LN("ms");
        pidConfigLogged = true;
    }

    flowReadValid = flowReadValid && flowData.valid;
    bool flowFresh = flowReadValid && flowData.ageMs <= pidCfg.sensorStaleMs;

    int seed0 = 0, seed1 = 0;
    bool hasSeed = calibration.getMotorPwms(targetFlow, seed0, seed1);
    const bool useCalibrationSeed = EoloConfig::useCalibrationSeed && hasSeed;
    const int primaryMotor = useCalibrationSeed ? calibration.weakMotor : 0;

    DualMotorFlowInput input;
    input.nowMs = nowMs;
    input.targetFlow = targetFlow;
    input.measuredFlow = flowFresh ? flowData.flow : -1.0f;
    input.flowValid = flowReadValid;
    input.flowFresh = flowFresh;
    input.flowStale = flowReadValid && flowData.ageMs > pidCfg.sensorStaleMs;
    input.flowAgeMs = flowData.ageMs;
    input.sampleId = flowData.sampleId;
    input.maxPwm = MAX_PWM;
    input.primaryMotor = primaryMotor;
    input.hasFeedForward = useCalibrationSeed;
    input.feedForwardPrimary = useCalibrationSeed
                                   ? (primaryMotor == 0 ? seed0 : seed1)
                                   : 0;
    input.feedForwardSecondary = useCalibrationSeed
                                     ? (primaryMotor == 0 ? seed1 : seed0)
                                     : 0;

    DualMotorFlowOutput output = pid.update(input, pidCfg);

    // Log arranque/re-kick (una vez por evento)
    if (output.kickActive && output.kickCount > _lastLoggedKickCount)
    {
        _lastLoggedKickCount = output.kickCount;
        if (output.stallDetected)
        {
            LOG_OUT("Motor re-kick por stall (kick #");
            LOG_OUT(output.kickCount);
            LOG_OUT_LN(")");
        }
        else
        {
            LOG_OUT("Motor kick de arranque (kick #");
            LOG_OUT(output.kickCount);
            LOG_OUT_LN(")");
        }
    }

    // Aplicar también el mando retenido durante la gracia: ante una lectura
    // inválida no se debe cambiar el PWM hasta que venza la ventana segura.
    if (output.updated || output.sensorGraceActive)
    {
        motor.setMotorPwm(0, output.motor0Pwm);
        motor.setMotorPwm(1, output.motor1Pwm);
    }

    // Log de enclavamiento de centro con tiempo exacto de calibración
    static bool _lastLockedReported = false;
    static uint32_t _seekStartReportMs = 0;

    if (output.kickActive)
    {
        _seekStartReportMs = nowMs;
        _lastLockedReported = false;
    }
    else if (!output.smartStatus.centerFound)
    {
        if (_seekStartReportMs == 0) _seekStartReportMs = nowMs;
        _lastLockedReported = false;
    }
    else if (output.smartStatus.centerFound && !_lastLockedReported)
    {
        _lastLockedReported = true;
        uint32_t lockDurationMs = (_seekStartReportMs > 0 && nowMs >= _seekStartReportMs)
                                      ? (nowMs - _seekStartReportMs)
                                      : 0;
        LOG_OUT("[PID Flujo] ¡Centro enclavado en PWM ");
        LOG_OUT(output.smartStatus.centerPwm);
        LOG_OUT("! Tiempo de calibracion: ");
        LOG_OUT(lockDurationMs / 1000.0f, 2);
        LOG_OUT("s | Caudal: ");
        LOG_OUT(flowData.flow, 2);
        LOG_OUT_LN(" L/min");
    }

    // Log fault de sensor (con rate limit)
    if (output.fault != FLOW_PID_FAULT_NONE)
    {
        static uint32_t lastSensorFaultLogMs = 0;
        if (nowMs - lastSensorFaultLogMs >= 5000)
        {
            const char *faultName =
                output.fault == FLOW_PID_FAULT_SENSOR_STALE   ? "stale" :
                output.fault == FLOW_PID_FAULT_SENSOR_INVALID ? "invalid" : "timing";
            LOG_OUT("PID flujo sensor fault: ");
            LOG_OUT(faultName);
            LOG_OUT(" ageMs=");
            LOG_OUT_LN(flowData.ageMs);
            lastSensorFaultLogMs = nowMs;
        }
        return;
    }

    if (!output.updated)
        return;

    // Log PID periódico (solo en Run, no durante kick)
    if (!output.kickActive)
    {
        static uint32_t lastPidLogMs = 0;
        if (nowMs - lastPidLogMs >= 1000)
        {
            LOG_OUT("PID flujo: medido ");
            LOG_OUT(flowData.flow, 2);
            LOG_OUT(" filtrado ");
            LOG_OUT(output.smartStatus.fastFlow, 2);
            LOG_OUT(" L/min PWM ");
            LOG_OUT(output.virtualPwm);
            LOG_OUT(" centro=");
            LOG_OUT(output.smartStatus.centerPwm);
            LOG_OUT(" trim=");
            LOG_OUT(output.smartStatus.trimPwm);
            LOG_OUT(" P/I/D ");
            LOG_OUT(output.smartStatus.pTerm, 1);
            LOG_OUT("/");
            LOG_OUT(output.smartStatus.iTerm, 1);
            LOG_OUT("/");
            LOG_OUT(output.smartStatus.dTerm, 1);
            LOG_OUT(" modo ");
            LOG_OUT(static_cast<int>(output.smartStatus.mode));
            LOG_OUT(" locked=");
            LOG_OUT_LN(output.smartStatus.centerFound ? "si" : "no");
            lastPidLogMs = nowMs;
        }
    }
}
#endif

inline void MotorCaptureControl::updateMotors(MotorManager &motor,
                                              const FlowData &flowData,
                                              bool flowReadValid,
                                              float targetFlow,
                                              const CalibrationManager &calibration)
{
    if (motorOverheatActive)
    {
        motor.setPwmImmediate(0);
        resetFlowController();
        return;
    }

 #if defined(FEATURE_FLOW_PID) || defined(FEATURE_FLOW_CALIBRATION)
    updatePidMotors(motor, flowData, flowReadValid, targetFlow, calibration);
 #else
    (void)motor;
    (void)flowData;
    (void)flowReadValid;
    (void)targetFlow;
    (void)calibration;
 #endif
}

#endif // EOLO_MOTOR_CAPTURE_CONTROL_H
