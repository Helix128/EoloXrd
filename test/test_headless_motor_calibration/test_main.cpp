#include <Arduino.h>
#include <unity.h>
#include "Data/MotorCaptureControl.h"
#include <Eolo/Core/Flow/SmartFlowController.h>

void test_default_pid_config_is_valid()
{
    FlowPidConfig config;
    TEST_ASSERT_TRUE(MotorCaptureControl::validatePidConfig(config));
}

void test_rejects_invalid_pid_timing()
{
    FlowPidConfig config;
    config.intervalMs = 50;
    TEST_ASSERT_FALSE(MotorCaptureControl::validatePidConfig(config));
}

void test_rejects_invalid_pid_gains()
{
    FlowPidConfig config;
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
    FlowPidConfig config;
    config.filterAlpha = 0.0f;
    TEST_ASSERT_FALSE(MotorCaptureControl::validatePidConfig(config));

    config.filterAlpha = 1.1f;
    TEST_ASSERT_FALSE(MotorCaptureControl::validatePidConfig(config));
}

void test_rejects_invalid_pid_fault_timing_config()
{
    FlowPidConfig config;
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
}

void test_flow_motor_controller_rejects_stale_sensor_without_pwm_update()
{
    FlowMotorController controller;
    FlowPidConfig config;
    FlowMotorInput input;
    input.nowMs = 100;
    input.currentPwm = 500;
    input.targetFlow = 5.0f;
    input.measuredFlow = 4.0f;
    input.flowValid = true;
    input.flowFresh = true;
    input.maxPwm = MAX_PWM;
    controller.update(input, config);

    input.nowMs = 250;
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
    FlowPidConfig config;
    FlowMotorInput input;
    input.nowMs = 100;
    input.currentPwm = 500;
    input.targetFlow = 5.0f;
    input.measuredFlow = 4.0f;
    input.flowValid = true;
    input.flowFresh = true;
    input.maxPwm = MAX_PWM;
    controller.update(input, config);

    input.nowMs = 5000;
    input.flowValid = false;
    input.flowFresh = false;
    FlowMotorOutput invalidOutput = controller.update(input, config);
    TEST_ASSERT_FALSE(invalidOutput.updated);
    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_SENSOR_INVALID, invalidOutput.fault);

    input.nowMs = 5200;
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

void test_smart_flow_controller_uses_bounded_static_memory()
{
    TEST_ASSERT_TRUE(sizeof(SmartFlowController) < 1024);
}

void setup()
{
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_default_pid_config_is_valid);
    RUN_TEST(test_rejects_invalid_pid_timing);
    RUN_TEST(test_rejects_invalid_pid_gains);
    RUN_TEST(test_rejects_invalid_pid_filter);
    RUN_TEST(test_rejects_invalid_pid_fault_timing_config);
    RUN_TEST(test_flow_pid_status_exposes_smart_model_fields);
    RUN_TEST(test_flow_motor_controller_rejects_stale_sensor_without_pwm_update);
    RUN_TEST(test_flow_motor_controller_caps_dt_after_invalid_sensor);
    RUN_TEST(test_smart_flow_controller_uses_bounded_static_memory);
    UNITY_END();
}

void loop()
{
}
