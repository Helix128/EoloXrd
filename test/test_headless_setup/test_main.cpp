#include <Arduino.h>
#include <unity.h>
#include "Board/HeadlessSetupTypes.h"

void test_setup_activation_only_wait_off()
{
    TEST_ASSERT_TRUE(HeadlessSetup::shouldEnterWebSetup(0b00));
    TEST_ASSERT_FALSE(HeadlessSetup::shouldEnterWebSetup(0b01));
    TEST_ASSERT_FALSE(HeadlessSetup::shouldEnterWebSetup(0b10));
    TEST_ASSERT_FALSE(HeadlessSetup::shouldEnterWebSetup(0b11));
}

void test_config_validation()
{
    HeadlessSetupConfig config;
    config.waitSeconds = 0;
    config.durationSeconds = 5UL * MINUTE;
    config.targetFlow = 5.0f;
    TEST_ASSERT_TRUE(HeadlessSetup::validateConfig(config));

    config.waitSeconds = HeadlessSetup::kMaxWaitSeconds + 1;
    TEST_ASSERT_FALSE(HeadlessSetup::validateConfig(config));

    config.waitSeconds = 0;
    config.durationSeconds = 0;
    TEST_ASSERT_FALSE(HeadlessSetup::validateConfig(config));

    config.durationSeconds = DRONE_DURATION_INFINITE;
    TEST_ASSERT_TRUE(HeadlessSetup::validateConfig(config));

    config.targetFlow = -0.1f;
    TEST_ASSERT_FALSE(HeadlessSetup::validateConfig(config));

    config.targetFlow = 8.1f;
    TEST_ASSERT_FALSE(HeadlessSetup::validateConfig(config));

    config.targetFlow = 8.0f;
    TEST_ASSERT_TRUE(HeadlessSetup::validateConfig(config));
}

void test_log_basename_sanitization()
{
    TEST_ASSERT_TRUE(HeadlessSetup::isSafeLogBasename("log_2026_05_26T12_00_00.csv"));
    TEST_ASSERT_TRUE(HeadlessSetup::isSafeLogBasename("log_test-1.csv"));
    TEST_ASSERT_FALSE(HeadlessSetup::isSafeLogBasename("/EOLO/logs/log_a.csv"));
    TEST_ASSERT_FALSE(HeadlessSetup::isSafeLogBasename("../log_a.csv"));
    TEST_ASSERT_FALSE(HeadlessSetup::isSafeLogBasename("log_../a.csv"));
    TEST_ASSERT_FALSE(HeadlessSetup::isSafeLogBasename("notes.csv"));
    TEST_ASSERT_FALSE(HeadlessSetup::isSafeLogBasename("log_bad.txt"));
    TEST_ASSERT_FALSE(HeadlessSetup::isSafeLogBasename("log_bad name.csv"));
}

void test_config_maps_to_session()
{
    HeadlessSetupConfig config;
    config.waitSeconds = 300;
    config.durationSeconds = 900;
    config.targetFlow = 4.2f;

    Session session;
    session.usePlantower = true;
    session.capturedVolume = 99.0f;
    session.elapsedTime = 12;
    session.lastLog = 34;

    DateTime now(2026, 5, 26, 12, 0, 0);
    HeadlessSetup::applyToSession(config, session, now);

    TEST_ASSERT_FALSE(session.usePlantower);
    TEST_ASSERT_EQUAL_UINT32(now.unixtime() + 300, session.startDate.unixtime());
    TEST_ASSERT_EQUAL_UINT32(900, session.duration);
    TEST_ASSERT_EQUAL_UINT32(0, session.elapsedTime);
    TEST_ASSERT_EQUAL_UINT32(0, session.lastLog);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.2f, session.targetFlow);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, session.capturedVolume);
}

void setup()
{
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_setup_activation_only_wait_off);
    RUN_TEST(test_config_validation);
    RUN_TEST(test_log_basename_sanitization);
    RUN_TEST(test_config_maps_to_session);
    UNITY_END();
}

void loop()
{
}
