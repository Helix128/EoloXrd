#include <Arduino.h>
#include <unity.h>
#include "Effectors/Motor.h"

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

void setup()
{
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_dynamic_control_raises_pwm_until_flow_reaches_target);
    RUN_TEST(test_dynamic_control_reduces_pwm_when_flow_overshoots);
    RUN_TEST(test_dynamic_control_does_not_chatter_inside_deadband);
    RUN_TEST(test_dynamic_control_limits_pwm_step_for_large_error);
    RUN_TEST(test_dynamic_control_can_follow_target_change_after_reset);
    UNITY_END();
}

void loop()
{
}
