#include <Arduino.h>
#include <unity.h>
#include "Data/HeadlessMotorCalibration.h"

void test_pwm8_to_runtime_scale()
{
    TEST_ASSERT_EQUAL_INT(0, HeadlessMotorCalibration::pwm8ToRuntime(0));
    TEST_ASSERT_EQUAL_INT(MAX_PWM, HeadlessMotorCalibration::pwm8ToRuntime(255));
    TEST_ASSERT_EQUAL_INT(1028, HeadlessMotorCalibration::pwm8ToRuntime(128));
}

void test_default_config_is_valid()
{
    HeadlessMotorCalibrationConfig config;
    TEST_ASSERT_TRUE(HeadlessMotorCalibration::validateConfig(config));
}

void test_rejects_invalid_pwm_range()
{
    HeadlessMotorCalibrationConfig config;
    config.pwmStart = 1900;
    config.pwmEnd = 400;
    TEST_ASSERT_FALSE(HeadlessMotorCalibration::validateConfig(config));

    config.pwmStart = 0;
    config.pwmEnd = MAX_PWM + 1;
    TEST_ASSERT_FALSE(HeadlessMotorCalibration::validateConfig(config));
}

void test_rejects_invalid_sampling()
{
    HeadlessMotorCalibrationConfig config;
    config.samplesPerPoint = 0;
    TEST_ASSERT_FALSE(HeadlessMotorCalibration::validateConfig(config));

    config.samplesPerPoint = 5;
    config.sampleIntervalMs = 50;
    TEST_ASSERT_FALSE(HeadlessMotorCalibration::validateConfig(config));
}

void setup()
{
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_pwm8_to_runtime_scale);
    RUN_TEST(test_default_config_is_valid);
    RUN_TEST(test_rejects_invalid_pwm_range);
    RUN_TEST(test_rejects_invalid_sampling);
    UNITY_END();
}

void loop()
{
}
