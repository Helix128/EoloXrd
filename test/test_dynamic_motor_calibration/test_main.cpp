#include <Arduino.h>
#include <unity.h>
#include "Effectors/Motor.h"
#include <Eolo/Core/Flow/SmartFlowController.h>

static int runClosedLoopStep(int basePwm, int &currentPwm, float targetFlow, float measuredFlow, float &integral)
{
    currentPwm = MotorManager::nextClosedLoopPwm(
        basePwm,
        currentPwm,
        targetFlow,
        measuredFlow,
        1.0f,
        integral,
        0.08f,
        80.0f,
        8.0f,
        30.0f,
        32);
    return currentPwm;
}

void test_dynamic_control_raises_pwm_until_flow_reaches_target()
{
    int currentPwm = 1000;
    float integral = 0.0f;

    TEST_ASSERT_EQUAL_INT(1032, runClosedLoopStep(1000, currentPwm, 5.0f, 4.40f, integral));
    TEST_ASSERT_EQUAL_INT(1064, runClosedLoopStep(1000, currentPwm, 5.0f, 4.00f, integral));
    TEST_ASSERT_TRUE(integral > 0.0f);
}

void test_dynamic_control_reduces_pwm_when_flow_overshoots()
{
    int currentPwm = 1100;
    float integral = 0.0f;

    TEST_ASSERT_EQUAL_INT(1068, runClosedLoopStep(1000, currentPwm, 5.0f, 5.45f, integral));
    TEST_ASSERT_EQUAL_INT(1036, runClosedLoopStep(1000, currentPwm, 5.0f, 5.30f, integral));
    TEST_ASSERT_TRUE(integral < 0.0f);
}

void test_dynamic_control_does_not_chatter_inside_deadband()
{
    int currentPwm = 1000;
    float integral = 0.0f;

    TEST_ASSERT_EQUAL_INT(1000, runClosedLoopStep(1000, currentPwm, 5.0f, 5.03f, integral));
    TEST_ASSERT_EQUAL_INT(1000, runClosedLoopStep(1000, currentPwm, 5.0f, 4.95f, integral));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, integral);
}

void test_dynamic_control_limits_pwm_step_for_large_error()
{
    int currentPwm = 900;
    float integral = 0.0f;

    TEST_ASSERT_EQUAL_INT(932, runClosedLoopStep(1000, currentPwm, 8.0f, 0.5f, integral));
    TEST_ASSERT_EQUAL_INT(964, runClosedLoopStep(1000, currentPwm, 8.0f, 0.5f, integral));
}

void test_dynamic_control_can_follow_target_change_after_reset()
{
    int currentPwm = 1000;
    float integral = 0.0f;

    runClosedLoopStep(1000, currentPwm, 5.0f, 4.2f, integral);
    TEST_ASSERT_TRUE(currentPwm > 1000);
    TEST_ASSERT_TRUE(integral > 0.0f);

    currentPwm = 760;
    integral = 0.0f;
    TEST_ASSERT_EQUAL_INT(792, runClosedLoopStep(760, currentPwm, 3.0f, 2.5f, integral));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, integral);
}

static float simulatedFlowForPwm(int pwm)
{
    if (pwm < 400)
        return 0.0f;
    return (pwm - 400) * 0.010f;
}

void test_smart_controller_can_learn_runtime_gain()
{
    SmartFlowController controller;
    SmartFlowTune tune;
    tune.maxStep = 24;
    tune.maxFeedForwardStep = 16;
    controller.setTune(tune);

    int pwm = 500;
    SmartFlowStatus status;
    for (int i = 0; i < 30; i++)
    {
        float flow = simulatedFlowForPwm(pwm);
        status = controller.update(i * 200, pwm, 5.0f, flow, 0.2f, MAX_PWM);
        pwm = status.pwm;
    }

    TEST_ASSERT_TRUE(status.estimatedGain > 0.001f);
    TEST_ASSERT_TRUE(status.confidence > 0.0f);
}

void test_smart_controller_uses_full_range_downward_after_target_change()
{
    SmartFlowController controller;
    SmartFlowTune tune;
    tune.maxStep = 24;
    tune.maxFeedForwardStep = 16;
    controller.setTune(tune);

    int pwm = 1100;
    SmartFlowStatus status;
    for (int i = 0; i < 12; i++)
    {
        status = controller.update(i * 200, pwm, 5.0f, simulatedFlowForPwm(pwm), 0.2f, MAX_PWM);
        pwm = status.pwm;
    }

    int before = pwm;
    controller.resetController(true);
    for (int i = 0; i < 14; i++)
    {
        status = controller.update(3000 + i * 200, pwm, 1.0f, simulatedFlowForPwm(pwm), 0.2f, MAX_PWM);
        pwm = status.pwm;
    }

    TEST_ASSERT_TRUE(pwm < before - 100);
}

void test_smart_controller_reset_clears_runtime_model()
{
    SmartFlowController controller;
    int pwm = 500;
    SmartFlowStatus status;
    for (int i = 0; i < 20; i++)
    {
        status = controller.update(i * 200, pwm, 4.0f, simulatedFlowForPwm(pwm), 0.2f, MAX_PWM);
        pwm = status.pwm;
    }

    controller.resetAll();
    status = controller.update(5000, 500, 4.0f, simulatedFlowForPwm(500), 0.2f, MAX_PWM);

    TEST_ASSERT_FALSE(status.modelValid);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, status.estimatedGain);
}

void setup()
{
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_dynamic_control_raises_pwm_until_flow_reaches_target);
    RUN_TEST(test_dynamic_control_reduces_pwm_when_flow_overshoots);
    RUN_TEST(test_dynamic_control_does_not_chatter_inside_deadband);
    RUN_TEST(test_dynamic_control_limits_pwm_step_for_large_error);
    RUN_TEST(test_dynamic_control_can_follow_target_change_after_reset);
    RUN_TEST(test_smart_controller_can_learn_runtime_gain);
    RUN_TEST(test_smart_controller_uses_full_range_downward_after_target_change);
    RUN_TEST(test_smart_controller_reset_clears_runtime_model);
    UNITY_END();
}

void loop()
{
}
