#include <Arduino.h>
#include <unity.h>
#include "Data/MotorCaptureControl.h"
#include "Data/Context.h"
#include <Eolo/Core/Flow/SmartFlowController.h>

static FlowPidConfig validPidConfig()
{
    return {
        FLOW_PID_INTERVAL_MS, FLOW_PID_DEADBAND, FLOW_PID_KP, FLOW_PID_KI,
        FLOW_PID_INTEGRAL_LIMIT, FLOW_PID_MAX_STEP, FLOW_PID_FILTER_ALPHA,
        FLOW_PID_MIN_ACTIVE, FLOW_PID_KD, FLOW_PID_MAX_DT_MS,
        FLOW_PID_SENSOR_STALE_MS, FLOW_PID_KICK_PWM, FLOW_PID_KICK_MS,
        FLOW_PID_STALL_FLOW_LPM, FLOW_PID_RESTALL_COOLDOWN_MS,
        FLOW_PID_STALL_CONFIRM_MS};
}

void test_default_pid_config_is_valid()
{
    FlowPidConfig config = validPidConfig();
    TEST_ASSERT_TRUE(MotorCaptureControl::validatePidConfig(config));
}

void test_flow_pid_base_pwm_is_1660()
{
    TEST_ASSERT_EQUAL_INT(1660, FLOW_PID_BASE_PWM);
}

void test_rejects_invalid_pid_timing()
{
    FlowPidConfig config = validPidConfig();
    config.intervalMs = 50;
    TEST_ASSERT_FALSE(MotorCaptureControl::validatePidConfig(config));
}

void test_rejects_invalid_pid_gains()
{
    FlowPidConfig config = validPidConfig();
    config.kp = -1.0f;
    TEST_ASSERT_FALSE(MotorCaptureControl::validatePidConfig(config));

    config.kp = 80.0f;
    config.ki = 250.0f;
    TEST_ASSERT_FALSE(MotorCaptureControl::validatePidConfig(config));

    config.ki = 8.0f;
    config.kd = -0.1f;
    TEST_ASSERT_FALSE(MotorCaptureControl::validatePidConfig(config));
}

void test_rejects_invalid_pid_filter()
{
    FlowPidConfig config = validPidConfig();
    config.filterAlpha = 0.0f;
    TEST_ASSERT_FALSE(MotorCaptureControl::validatePidConfig(config));

    config.filterAlpha = 1.1f;
    TEST_ASSERT_FALSE(MotorCaptureControl::validatePidConfig(config));
}

void test_rejects_invalid_pid_fault_timing_config()
{
    FlowPidConfig config = validPidConfig();
    config.maxDtMs = config.intervalMs - 1;
    TEST_ASSERT_FALSE(MotorCaptureControl::validatePidConfig(config));

    config.maxDtMs = 1000;
    config.sensorStaleMs = config.intervalMs - 1;
    TEST_ASSERT_FALSE(MotorCaptureControl::validatePidConfig(config));
}

void test_flow_pid_status_exposes_smart_model_fields()
{
    FlowPidStatus status;
    TEST_ASSERT_EQUAL_INT(SMART_FLOW_PID_ONLY, status.mode);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, status.estimatedGain);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, status.confidence);
    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_NONE, status.fault);
    TEST_ASSERT_TRUE(status.timingOk);
    TEST_ASSERT_FALSE(status.modelValid);
    TEST_ASSERT_EQUAL_INT((int)IgnitionPhase::Off, (int)status.ignitionPhase);
    TEST_ASSERT_FALSE(status.kickActive);
    TEST_ASSERT_EQUAL_UINT16(0, status.kickCount);
}

void test_flow_motor_controller_starts_with_kick()
{
    FlowMotorController controller;
    FlowPidConfig config = validPidConfig();

    FlowMotorInput input;
    input.nowMs = 100;
    input.currentPwm = 0;
    input.targetFlow = 5.0f;
    input.measuredFlow = 0.0f;
    input.flowValid = true;
    input.flowFresh = true;
    input.maxPwm = MAX_PWM;
    FlowMotorOutput output = controller.update(input, config);

    TEST_ASSERT_TRUE(output.updated);
    TEST_ASSERT_EQUAL_INT(config.kickPwm, output.pwm);
    TEST_ASSERT_EQUAL_INT((int)IgnitionPhase::Kick, (int)output.ignitionPhase);
    TEST_ASSERT_TRUE(output.kickActive);
    TEST_ASSERT_EQUAL_UINT16(1, output.kickCount);
}

void test_flow_motor_controller_finishes_kick_after_configured_time()
{
    FlowMotorController controller;
    FlowPidConfig config = validPidConfig();

    FlowMotorInput input;
    input.nowMs = 100;
    input.currentPwm = 0;
    input.targetFlow = 5.0f;
    input.measuredFlow = 0.0f;
    input.flowValid = true;
    input.flowFresh = true;
    input.maxPwm = MAX_PWM;
    FlowMotorOutput kickOutput = controller.update(input, config);
    input.nowMs += config.kickMs;
    input.currentPwm = kickOutput.pwm;
    FlowMotorOutput runOutput = controller.update(input, config);

    TEST_ASSERT_TRUE(runOutput.updated);
    TEST_ASSERT_EQUAL_INT((int)IgnitionPhase::Run, (int)runOutput.ignitionPhase);
    TEST_ASSERT_FALSE(runOutput.kickActive);
}

void test_flow_motor_controller_rejects_stale_sensor_without_pwm_update()
{
    FlowMotorController controller;
    FlowPidConfig config = validPidConfig();
    FlowMotorInput input;
    input.nowMs = 100;
    input.currentPwm = 500;
    input.targetFlow = 5.0f;
    input.measuredFlow = 4.0f;
    input.flowValid = true;
    input.flowFresh = true;
    input.maxPwm = MAX_PWM;
    controller.update(input, config);
    input.nowMs += config.kickMs;
    input.currentPwm = config.kickPwm;
    controller.update(input, config);

    input.nowMs += config.intervalMs;
    input.flowFresh = false;
    input.flowStale = true;
    input.flowAgeMs = config.sensorStaleMs + 1;
    FlowMotorOutput output = controller.update(input, config);

    TEST_ASSERT_FALSE(output.updated);
    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_SENSOR_STALE, output.fault);
}

void test_flow_motor_controller_caps_dt_after_invalid_sensor()
{
    FlowMotorController controller;
    FlowPidConfig config = validPidConfig();
    FlowMotorInput input;
    input.nowMs = 100;
    input.currentPwm = 500;
    input.targetFlow = 5.0f;
    input.measuredFlow = 4.0f;
    input.flowValid = true;
    input.flowFresh = true;
    input.maxPwm = MAX_PWM;
    controller.update(input, config);
    input.nowMs += config.kickMs;
    input.currentPwm = config.kickPwm;
    controller.update(input, config);

    input.nowMs += config.maxDtMs + 1;
    input.flowValid = false;
    input.flowFresh = false;
    FlowMotorOutput invalidOutput = controller.update(input, config);
    TEST_ASSERT_FALSE(invalidOutput.updated);
    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_SENSOR_INVALID, invalidOutput.fault);

    input.nowMs += config.intervalMs;
    input.flowValid = true;
    input.flowFresh = true;
    input.flowStale = false;
    input.flowAgeMs = 0;
    FlowMotorOutput recoveredOutput = controller.update(input, config);
    FlowPidStatus status = controller.status(true, false, 5.0f);

    TEST_ASSERT_TRUE(recoveredOutput.updated);
    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_NONE, recoveredOutput.fault);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2f, status.dtSeconds);
}

#if defined(EOLO_TARGET_DRON) && defined(FEATURE_FLOW_PID)
void test_drone_capture_keeps_base_pwm_until_pid_has_fresh_flow()
{
    Context ctx;
    ctx.session.targetFlow = DRONE_TARGET_FLOW_LPM;
    ctx.components.motor.setPwmImmediate(FLOW_PID_BASE_PWM);

    ctx.updateMotors();

    TEST_ASSERT_EQUAL_INT(FLOW_PID_BASE_PWM, ctx.components.motor.getMotorPwm(0));
}
#endif

void test_smart_flow_controller_uses_bounded_static_memory()
{
    TEST_ASSERT_TRUE(sizeof(SmartFlowController) < 1024);
}

void setup()
{
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_default_pid_config_is_valid);
    RUN_TEST(test_flow_pid_base_pwm_is_1660);
    RUN_TEST(test_rejects_invalid_pid_timing);
    RUN_TEST(test_rejects_invalid_pid_gains);
    RUN_TEST(test_rejects_invalid_pid_filter);
    RUN_TEST(test_rejects_invalid_pid_fault_timing_config);
    RUN_TEST(test_flow_pid_status_exposes_smart_model_fields);
    RUN_TEST(test_flow_motor_controller_starts_with_kick);
    RUN_TEST(test_flow_motor_controller_finishes_kick_after_configured_time);
    RUN_TEST(test_flow_motor_controller_rejects_stale_sensor_without_pwm_update);
    RUN_TEST(test_flow_motor_controller_caps_dt_after_invalid_sensor);
#if defined(EOLO_TARGET_DRON) && defined(FEATURE_FLOW_PID)
    RUN_TEST(test_drone_capture_keeps_base_pwm_until_pid_has_fresh_flow);
#endif
    RUN_TEST(test_smart_flow_controller_uses_bounded_static_memory);
    UNITY_END();
}

void loop()
{
}
