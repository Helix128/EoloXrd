#include <Arduino.h>
#include <unity.h>
#include "Board/HeadlessSetupTypes.h"
#include <Eolo/Core/Flow/FlowSchedule.h>

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

void test_flow_schedule_uses_fixed_flow_without_sections()
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.2f, FlowSchedule::targetAtElapsed(4.2f, 0, nullptr, 100));
}

void test_flow_schedule_transitions_by_elapsed_time()
{
    FlowSection sections[2];
    sections[0].durationSeconds = 300;
    sections[0].targetFlow = 2.0f;
    sections[1].durationSeconds = 600;
    sections[1].targetFlow = 6.0f;

    TEST_ASSERT_TRUE(FlowSchedule::validate(2, sections, 900, DRONE_DURATION_INFINITE, 0.0f, 8.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, FlowSchedule::targetAtElapsed(5.0f, 2, sections, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, FlowSchedule::targetAtElapsed(5.0f, 2, sections, 299));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, FlowSchedule::targetAtElapsed(5.0f, 2, sections, 300));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, FlowSchedule::targetAtElapsed(5.0f, 2, sections, 1200));
}

void test_flow_schedule_validation_rejects_invalid_sections()
{
    FlowSection sections[1];
    sections[0].durationSeconds = 0;
    sections[0].targetFlow = 4.0f;
    TEST_ASSERT_FALSE(FlowSchedule::validate(1, sections, 900, DRONE_DURATION_INFINITE, 0.0f, 8.0f));

    sections[0].durationSeconds = 300;
    sections[0].targetFlow = 8.1f;
    TEST_ASSERT_FALSE(FlowSchedule::validate(1, sections, 900, DRONE_DURATION_INFINITE, 0.0f, 8.0f));

    sections[0].targetFlow = 4.0f;
    TEST_ASSERT_FALSE(FlowSchedule::validate(1, sections, 299, DRONE_DURATION_INFINITE, 0.0f, 8.0f));
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

void test_preset_name_sanitization()
{
    TEST_ASSERT_TRUE(HeadlessSetup::isSafePresetName("perfil_1"));
    TEST_ASSERT_TRUE(HeadlessSetup::isSafePresetName("perfil-rapido"));
    TEST_ASSERT_FALSE(HeadlessSetup::isSafePresetName(""));
    TEST_ASSERT_FALSE(HeadlessSetup::isSafePresetName("perfil malo"));
    TEST_ASSERT_FALSE(HeadlessSetup::isSafePresetName("../perfil"));
    TEST_ASSERT_FALSE(HeadlessSetup::isSafePresetName("perfil/uno"));
    TEST_ASSERT_FALSE(HeadlessSetup::isSafePresetName("nombre_demasiado_largo_123"));
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

    const uint32_t nowUnix = 1789819200UL;
    HeadlessSetup::applyToSession(config, session, nowUnix);

    TEST_ASSERT_FALSE(session.usePlantower);
    TEST_ASSERT_EQUAL_UINT32(nowUnix + 300, session.startUnix);
    TEST_ASSERT_EQUAL_UINT32(900, session.duration);
    TEST_ASSERT_EQUAL_UINT32(0, session.elapsedTime);
    TEST_ASSERT_EQUAL_UINT32(0, session.lastLog);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.2f, session.targetFlow);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, session.capturedVolume);
}

void test_config_maps_flow_sections_to_session()
{
    HeadlessSetupConfig config;
    config.waitSeconds = 0;
    config.durationSeconds = 900;
    config.targetFlow = 4.2f;
    config.flowSectionCount = 2;
    config.flowSections[0].durationSeconds = 300;
    config.flowSections[0].targetFlow = 2.0f;
    config.flowSections[1].durationSeconds = 600;
    config.flowSections[1].targetFlow = 6.0f;

    TEST_ASSERT_TRUE(HeadlessSetup::validateConfig(config));

    Session session;
    const uint32_t nowUnix = 1789819200UL;
    HeadlessSetup::applyToSession(config, session, nowUnix);

    TEST_ASSERT_EQUAL_UINT8(2, session.flowSectionCount);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, session.targetFlow);
    TEST_ASSERT_EQUAL_UINT32(300, session.flowSections[0].durationSeconds);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, session.flowSections[0].targetFlow);
    TEST_ASSERT_EQUAL_UINT32(600, session.flowSections[1].durationSeconds);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, session.flowSections[1].targetFlow);
}

void setup()
{
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_setup_activation_only_wait_off);
    RUN_TEST(test_config_validation);
    RUN_TEST(test_flow_schedule_uses_fixed_flow_without_sections);
    RUN_TEST(test_flow_schedule_transitions_by_elapsed_time);
    RUN_TEST(test_flow_schedule_validation_rejects_invalid_sections);
    RUN_TEST(test_log_basename_sanitization);
    RUN_TEST(test_preset_name_sanitization);
    RUN_TEST(test_config_maps_to_session);
    RUN_TEST(test_config_maps_flow_sections_to_session);
    UNITY_END();
}

void loop()
{
}
