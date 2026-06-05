#ifndef EOLO_MOTOR_CAPTURE_CONTROL_H
#define EOLO_MOTOR_CAPTURE_CONTROL_H

#include <Arduino.h>
#include "../Config.h"

struct Context;

class MotorCaptureControl
{
#if MOTOR_FLOW_CONTROL_MODE == MOTOR_FLOW_CONTROL_CLOSED_LOOP
    struct MotorFlowController
    {
        bool initialized = false;
        int basePwm = 0;
        int currentPwm = 0;
        float integral = 0.0f;
        float targetFlow = -1.0f;
        uint32_t lastUpdateMs = 0;
    } motorFlowController;
#endif
    uint32_t lastMotorOverheatLogMs = 0;

#if MOTOR_FLOW_CONTROL_MODE == MOTOR_FLOW_CONTROL_CLOSED_LOOP
    void updateClosedLoopMotors(Context &ctx);
#endif

public:
    bool motorOverheatActive = false;
    bool motorThermalSensorValid = false;
    float motorThermalTemperature = -99.0f;

    bool updateThermalProtection(Context &ctx);
    void resetFlowController();
    void updateMotors(Context &ctx);
};

#endif
