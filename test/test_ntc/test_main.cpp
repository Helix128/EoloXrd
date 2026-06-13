#include <Arduino.h>
#include <unity.h>
#include "Sensors/NTC.h"
#include <Eolo/Core/Sensors/NtcThermistor.h>

void test_ntc_core_math_compiles_without_sensor_adapter()
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, NTC::rawToVoltage(2048), NtcThermistor::rawToVoltage(2048));
}

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

void test_motor_overheat_latches_above_high_threshold()
{
    TEST_ASSERT_TRUE(NTC::motorOverheatLatched(false, true, 70.0f, 70.0f, 60.0f));
    TEST_ASSERT_TRUE(NTC::motorOverheatLatched(false, true, 75.0f, 70.0f, 60.0f));
}

void test_motor_overheat_holds_between_thresholds()
{
    TEST_ASSERT_TRUE(NTC::motorOverheatLatched(true, true, 65.0f, 70.0f, 60.0f));
    TEST_ASSERT_FALSE(NTC::motorOverheatLatched(false, true, 65.0f, 70.0f, 60.0f));
}

void test_motor_overheat_releases_below_low_threshold()
{
    TEST_ASSERT_FALSE(NTC::motorOverheatLatched(true, true, 60.0f, 70.0f, 60.0f));
    TEST_ASSERT_FALSE(NTC::motorOverheatLatched(true, true, 55.0f, 70.0f, 60.0f));
}

void test_motor_overheat_invalid_read_keeps_current_state()
{
    TEST_ASSERT_TRUE(NTC::motorOverheatLatched(true, false, -99.0f, 70.0f, 60.0f));
    TEST_ASSERT_FALSE(NTC::motorOverheatLatched(false, false, 75.0f, 70.0f, 60.0f));
}

void setup()
{
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_ntc_core_math_compiles_without_sensor_adapter);
    RUN_TEST(test_midscale_is_about_25c);
    RUN_TEST(test_extreme_adc_values_are_invalid);
    RUN_TEST(test_motor_overheat_latches_above_high_threshold);
    RUN_TEST(test_motor_overheat_holds_between_thresholds);
    RUN_TEST(test_motor_overheat_releases_below_low_threshold);
    RUN_TEST(test_motor_overheat_invalid_read_keeps_current_state);
    UNITY_END();
}

void loop()
{
}
