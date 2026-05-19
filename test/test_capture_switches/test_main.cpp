#include <Arduino.h>
#include <unity.h>
#include "Board/CaptureSwitches.h"

void test_wait_table()
{
    CaptureSwitchSelection off = CaptureSwitches::decode(0b00, 0b01);
    TEST_ASSERT_FALSE(off.waitEnabled);
    TEST_ASSERT_FALSE(off.shouldStart);

    CaptureSwitchSelection five = CaptureSwitches::decode(0b01, 0b01);
    TEST_ASSERT_TRUE(five.waitEnabled);
    TEST_ASSERT_EQUAL_UINT32(5UL * MINUTE, five.waitSeconds);
    TEST_ASSERT_FALSE(five.instantStart);

    CaptureSwitchSelection fifteen = CaptureSwitches::decode(0b10, 0b01);
    TEST_ASSERT_TRUE(fifteen.waitEnabled);
    TEST_ASSERT_EQUAL_UINT32(15UL * MINUTE, fifteen.waitSeconds);
    TEST_ASSERT_FALSE(fifteen.instantStart);

    CaptureSwitchSelection instant = CaptureSwitches::decode(0b11, 0b01);
    TEST_ASSERT_TRUE(instant.waitEnabled);
    TEST_ASSERT_EQUAL_UINT32(0, instant.waitSeconds);
    TEST_ASSERT_TRUE(instant.instantStart);
}

void test_duration_table()
{
    CaptureSwitchSelection off = CaptureSwitches::decode(0b11, 0b00);
    TEST_ASSERT_FALSE(off.durationEnabled);
    TEST_ASSERT_FALSE(off.shouldStart);

    CaptureSwitchSelection five = CaptureSwitches::decode(0b11, 0b01);
    TEST_ASSERT_TRUE(five.durationEnabled);
    TEST_ASSERT_EQUAL_UINT32(5UL * MINUTE, five.durationSeconds);
    TEST_ASSERT_FALSE(five.infiniteDuration);

    CaptureSwitchSelection fifteen = CaptureSwitches::decode(0b11, 0b10);
    TEST_ASSERT_TRUE(fifteen.durationEnabled);
    TEST_ASSERT_EQUAL_UINT32(15UL * MINUTE, fifteen.durationSeconds);
    TEST_ASSERT_FALSE(fifteen.infiniteDuration);

    CaptureSwitchSelection infinite = CaptureSwitches::decode(0b11, 0b11);
    TEST_ASSERT_TRUE(infinite.durationEnabled);
    TEST_ASSERT_EQUAL_UINT32(DRONE_DURATION_INFINITE, infinite.durationSeconds);
    TEST_ASSERT_TRUE(infinite.infiniteDuration);
}

void test_sw0_is_lsb()
{
    CaptureSwitchSelection sw0Only = CaptureSwitches::decode(0b01, 0b01);
    TEST_ASSERT_EQUAL_UINT8(0b01, sw0Only.waitCode);
    TEST_ASSERT_EQUAL_UINT32(5UL * MINUTE, sw0Only.waitSeconds);

    CaptureSwitchSelection sw1Only = CaptureSwitches::decode(0b10, 0b10);
    TEST_ASSERT_EQUAL_UINT8(0b10, sw1Only.waitCode);
    TEST_ASSERT_EQUAL_UINT32(15UL * MINUTE, sw1Only.waitSeconds);
    TEST_ASSERT_EQUAL_UINT8(0b10, sw1Only.durationCode);
    TEST_ASSERT_EQUAL_UINT32(15UL * MINUTE, sw1Only.durationSeconds);
}

void setup()
{
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_wait_table);
    RUN_TEST(test_duration_table);
    RUN_TEST(test_sw0_is_lsb);
    UNITY_END();
}

void loop()
{
}
