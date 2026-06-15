#include <Arduino.h>
#include <unity.h>
#include "Effectors/Motor.h"
#include <Eolo/Core/Motor/PwmMath.h>
#include <Eolo/Core/Power/BatteryMath.h>
#include <Eolo/Core/Flow/FlowMotorController.h>

void test_next_ramped_pwm_steps_up_without_overshoot()
{
    TEST_ASSERT_EQUAL_INT(16, MotorManager::nextRampedPwm(0, 100, 16));
    TEST_ASSERT_EQUAL_INT(100, MotorManager::nextRampedPwm(96, 100, 16));
}

void test_next_ramped_pwm_decreases_immediately()
{
    TEST_ASSERT_EQUAL_INT(50, MotorManager::nextRampedPwm(100, 50, 16));
}

void test_next_ramped_pwm_constrains_values()
{
    TEST_ASSERT_EQUAL_INT(0, MotorManager::nextRampedPwm(-20, -1, 16));
    TEST_ASSERT_EQUAL_INT(MAX_PWM, MotorManager::nextRampedPwm(MAX_PWM - 1, MAX_PWM + 500, 16));
}

void test_pwm_math_core_limits_step_without_motor_hardware()
{
    TEST_ASSERT_EQUAL_INT(132, PwmMath::limitStep(100, 200, 32, MAX_PWM));
    TEST_ASSERT_EQUAL_INT(168, PwmMath::limitStep(200, 100, 32, MAX_PWM));
}

void test_battery_math_clamps_percentage()
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, BatteryMath::pctFromVoltage(-1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, BatteryMath::pctFromVoltage(8.4f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, BatteryMath::pctFromVoltage(20.0f));
}

void test_initial_motor_state_is_zero()
{
    MotorManager motor;
    TEST_ASSERT_EQUAL_INT(0, motor.getMotorPwm(0));
    TEST_ASSERT_EQUAL_INT(0, motor.getMotorTargetPwm(0));
}

void test_limit_pwm_step_limits_both_directions()
{
    TEST_ASSERT_EQUAL_INT(132, MotorManager::limitPwmStep(100, 200, 32));
    TEST_ASSERT_EQUAL_INT(168, MotorManager::limitPwmStep(200, 100, 32));
    TEST_ASSERT_EQUAL_INT(120, MotorManager::limitPwmStep(100, 120, 32));
}

void test_closed_loop_pwm_deadband_preserves_integral_correction()
{
    float integral = 2.0f;
    int pwm = MotorManager::nextClosedLoopPwm(1000, 1000, 5.0f, 4.96f, 1.0f, integral, 0.08f, 80.0f, 8.0f, 30.0f, 32);
    TEST_ASSERT_EQUAL_INT(1016, pwm);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, integral);
}

void test_closed_loop_pwm_increases_when_flow_is_low()
{
    float integral = 0.0f;
    int pwm = MotorManager::nextClosedLoopPwm(1000, 1000, 5.0f, 4.5f, 1.0f, integral, 0.08f, 80.0f, 8.0f, 30.0f, 32);
    TEST_ASSERT_EQUAL_INT(1032, pwm);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, integral);
}

void test_closed_loop_pwm_decreases_when_flow_is_high()
{
    float integral = 0.0f;
    int pwm = MotorManager::nextClosedLoopPwm(1000, 1000, 5.0f, 5.5f, 1.0f, integral, 0.08f, 80.0f, 8.0f, 30.0f, 32);
    TEST_ASSERT_EQUAL_INT(968, pwm);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.5f, integral);
}

void test_closed_loop_pwm_clamps_integral_and_pwm()
{
    float integral = 100.0f;
    int pwm = MotorManager::nextClosedLoopPwm(MAX_PWM - 4, MAX_PWM - 4, 10.0f, 0.0f, 10.0f, integral, 0.08f, 80.0f, 8.0f, 30.0f, 100);
    TEST_ASSERT_EQUAL_INT(MAX_PWM, pwm);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.0f, integral);
}

void test_flow_pid_responsive_preset_limits_pwm_growth()
{
    FlowPidConfig config;
    config.intervalMs = 800;
    config.sensorStaleMs = 2800;
    config.maxDtMs = 2800;
    config.kp = 28.0f;
    config.ki = 0.4f;
    config.kd = 5.0f;
    config.integralLimit = 6.0f;
    config.maxStep = 8;
    config.filterAlpha = 0.35f;
    config.deadband = 0.15f;
    config.minActive = 0.30f;

    FlowMotorController controller;
    FlowMotorInput input;
    input.targetFlow = 5.0f;
    input.measuredFlow = 0.0f;
    input.flowValid = true;
    input.flowFresh = true;
    input.flowStale = false;
    input.maxPwm = MAX_PWM;

    FlowMotorOutput output{};
    for (uint32_t t = 0; t <= 3200; t += 800)
    {
        input.nowMs = t;
        input.currentPwm = output.pwm;
        output = controller.update(input, config);
    }

    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_NONE, output.fault);
    TEST_ASSERT_TRUE(output.pwm <= 64);
    TEST_ASSERT_TRUE(output.smartStatus.integral <= config.integralLimit);
}

void test_flow_pid_accepts_delayed_sample_under_stale_limit()
{
    FlowPidConfig config;
    config.intervalMs = 800;
    config.sensorStaleMs = 2800;
    config.maxDtMs = 2800;
    config.kp = 28.0f;
    config.ki = 0.4f;
    config.kd = 5.0f;
    config.integralLimit = 6.0f;
    config.maxStep = 8;
    config.filterAlpha = 0.35f;
    config.deadband = 0.15f;
    config.minActive = 0.30f;

    FlowMotorController controller;
    FlowMotorInput input;
    input.targetFlow = 5.0f;
    input.measuredFlow = 1.0f;
    input.flowValid = true;
    input.flowFresh = true;
    input.flowStale = false;
    input.maxPwm = MAX_PWM;

    input.nowMs = 0;
    FlowMotorOutput output = controller.update(input, config);
    input.nowMs = 800;
    input.currentPwm = output.pwm;
    output = controller.update(input, config);

    input.nowMs = 1600;
    input.currentPwm = output.pwm;
    input.flowAgeMs = 1600;
    input.flowFresh = true;
    input.flowStale = false;
    output = controller.update(input, config);

    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_NONE, output.fault);
    TEST_ASSERT_TRUE(output.updated);
    TEST_ASSERT_TRUE(output.pwm > 0);
}

void setup()
{
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_next_ramped_pwm_steps_up_without_overshoot);
    RUN_TEST(test_next_ramped_pwm_decreases_immediately);
    RUN_TEST(test_next_ramped_pwm_constrains_values);
    RUN_TEST(test_pwm_math_core_limits_step_without_motor_hardware);
    RUN_TEST(test_battery_math_clamps_percentage);
    RUN_TEST(test_initial_motor_state_is_zero);
    RUN_TEST(test_limit_pwm_step_limits_both_directions);
    RUN_TEST(test_closed_loop_pwm_deadband_preserves_integral_correction);
    RUN_TEST(test_closed_loop_pwm_increases_when_flow_is_low);
    RUN_TEST(test_closed_loop_pwm_decreases_when_flow_is_high);
    RUN_TEST(test_closed_loop_pwm_clamps_integral_and_pwm);
    RUN_TEST(test_flow_pid_responsive_preset_limits_pwm_growth);
    RUN_TEST(test_flow_pid_accepts_delayed_sample_under_stale_limit);
    UNITY_END();
}

void loop()
{
}
