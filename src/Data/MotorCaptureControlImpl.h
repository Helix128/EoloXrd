#ifndef EOLO_MOTORCAPTURECONTROLIMPL_H
#define EOLO_MOTORCAPTURECONTROLIMPL_H

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
#if MOTOR_FLOW_CONTROL_MODE == MOTOR_FLOW_CONTROL_CLOSED_LOOP
    motorFlowController = MotorFlowController();
#endif
}

#if MOTOR_FLOW_CONTROL_MODE == MOTOR_FLOW_CONTROL_CLOSED_LOOP
inline void MotorCaptureControl::updateClosedLoopMotors(Context &ctx)
{
    unsigned long nowMs = millis();
    if (motorFlowController.initialized &&
        nowMs - motorFlowController.lastUpdateMs < MOTOR_FLOW_CONTROL_INTERVAL_MS)
        return;

    int p0 = 0, p1 = 0;
    if (!ctx.calibration.getMotorPwms(ctx.session.targetFlow, p0, p1))
    {
        resetFlowController();
        ctx.components.motor.setPwm(0);
        return;
    }

    if (!motorFlowController.initialized || fabsf(motorFlowController.targetFlow - ctx.session.targetFlow) > 0.001f)
    {
        motorFlowController.initialized = true;
        motorFlowController.basePwm = p0;
        motorFlowController.currentPwm = p0;
        motorFlowController.integral = 0.0f;
        motorFlowController.targetFlow = ctx.session.targetFlow;
        motorFlowController.lastUpdateMs = nowMs;
        ctx.components.motor.setMotorPwm(0, p0);
        LOG_OUT("Control flujo closed-loop: objetivo ");
        LOG_OUT(ctx.session.targetFlow, 2);
        LOG_OUT(" L/min -> PWM base ");
        LOG_OUT_LN(p0);
        return;
    }

    FlowData flowData;
    if (!ctx.components.flowSensor.getData(flowData) || !flowData.valid)
    {
        ctx.components.motor.setMotorPwm(0, motorFlowController.currentPwm);
        return;
    }

    float dtSeconds = (nowMs - motorFlowController.lastUpdateMs) / 1000.0f;
    motorFlowController.lastUpdateMs = nowMs;
    motorFlowController.basePwm = p0;
    motorFlowController.currentPwm = MotorManager::nextClosedLoopPwm(
        motorFlowController.basePwm,
        motorFlowController.currentPwm,
        ctx.session.targetFlow,
        flowData.flow,
        dtSeconds,
        motorFlowController.integral,
        MOTOR_FLOW_DEADBAND_LPM,
        MOTOR_FLOW_KP,
        MOTOR_FLOW_KI,
        MOTOR_FLOW_INTEGRAL_LIMIT,
        MOTOR_FLOW_MAX_STEP_PWM);

    LOG_OUT("Control flujo closed-loop: medido ");
    LOG_OUT(flowData.flow, 2);
    LOG_OUT(" L/min, PWM ");
    LOG_OUT(motorFlowController.currentPwm);
    LOG_OUT(" base ");
    LOG_OUT(motorFlowController.basePwm);
    LOG_OUT(" integral ");
    LOG_OUT_LN(motorFlowController.integral, 3);

    ctx.components.motor.setMotorPwm(0, motorFlowController.currentPwm);
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

#if MOTOR_FLOW_CONTROL_MODE == MOTOR_FLOW_CONTROL_CLOSED_LOOP
    updateClosedLoopMotors(ctx);
#else
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
#endif
}

#endif
