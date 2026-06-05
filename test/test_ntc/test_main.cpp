#include <Arduino.h>
#include <unity.h>
#include "Sensors/NTC.h"

void test_midscale_is_about_25c()
{
    float temp = 0.0f;
    float resistance = 0.0f;
    TEST_ASSERT_TRUE(NTC::computeTemperature(2048, temp, resistance));
    TEST_ASSERT_FLOAT_WITHIN(250.0f, 10000.0f, resistance);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 25.0f, temp);
}

void test_extreme_adc_values_are_invalid()
{
    float temp = 0.0f;
    float resistance = 0.0f;
    TEST_ASSERT_FALSE(NTC::computeTemperature(0, temp, resistance));
    TEST_ASSERT_FALSE(NTC::computeTemperature(4095, temp, resistance));
}

void setup()
{
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_midscale_is_about_25c);
    RUN_TEST(test_extreme_adc_values_are_invalid);
    UNITY_END();
}

void loop()
{
}
