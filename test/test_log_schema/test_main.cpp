#include <Arduino.h>
#include <unity.h>
#include "Data/Logging/LogSchema.h"

struct FakeSession
{
    bool usePlantower = true;
};

#if defined(FEATURE_FLOW_PID)
struct FakePidStatus
{
    bool kickActive = false;
};

struct FakeMotorCapture
{
    FakePidStatus status;
    FakePidStatus getPidStatus() const { return status; }
};
#endif

struct FakeContext
{
    FakeSession session;
#if defined(FEATURE_FLOW_PID)
    FakeMotorCapture motorCapture;
#endif
};

static bool hasColumn(const String &header, const char *column)
{
    String token = String(column);
    return header == token ||
           header.startsWith(token + ",") ||
           header.endsWith("," + token) ||
           header.indexOf("," + token + ",") >= 0;
}

void test_log_missing_value_is_minus_one()
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, LogSchema::LogMissingValue);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, LogSchema::missingIfInvalid(false, 42.0f));
}

void test_ntc_column_matches_feature_flag()
{
    FakeContext ctx;
    String header = LogSchema::header(ctx);
#ifdef FEATURE_NTC
    TEST_ASSERT_TRUE(hasColumn(header, "ntc_temperature"));
#else
    TEST_ASSERT_FALSE(hasColumn(header, "ntc_temperature"));
#endif
}

void test_state_column_follows_pid_feature_flag()
{
    FakeContext ctx;
#ifdef FEATURE_FLOW_PID
    TEST_ASSERT_TRUE(hasColumn(LogSchema::header(ctx), "state"));
#else
    TEST_ASSERT_FALSE(hasColumn(LogSchema::header(ctx), "state"));
#endif
}

void test_capture_state_text()
{
    FakeContext ctx;
    TEST_ASSERT_EQUAL_STRING("Capturando", LogSchema::captureState(ctx));
#if defined(FEATURE_FLOW_PID)
    ctx.motorCapture.status.kickActive = true;
    TEST_ASSERT_EQUAL_STRING("Arrancando", LogSchema::captureState(ctx));
#endif
}

void test_plantower_columns_follow_session_flag()
{
    FakeContext ctx;
    ctx.session.usePlantower = false;
    String disabledHeader = LogSchema::header(ctx);
    TEST_ASSERT_FALSE(hasColumn(disabledHeader, "pm1"));
    TEST_ASSERT_FALSE(hasColumn(disabledHeader, "pm25"));
    TEST_ASSERT_FALSE(hasColumn(disabledHeader, "pm10"));

    ctx.session.usePlantower = true;
    String enabledHeader = LogSchema::header(ctx);
#ifdef FEATURE_PLANTOWER
    TEST_ASSERT_TRUE(hasColumn(enabledHeader, "pm1"));
    TEST_ASSERT_TRUE(hasColumn(enabledHeader, "pm25"));
    TEST_ASSERT_TRUE(hasColumn(enabledHeader, "pm10"));
#else
    TEST_ASSERT_FALSE(hasColumn(enabledHeader, "pm1"));
    TEST_ASSERT_FALSE(hasColumn(enabledHeader, "pm25"));
    TEST_ASSERT_FALSE(hasColumn(enabledHeader, "pm10"));
#endif
}

void test_bme_invalid_maps_to_missing_value()
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, LogSchema::bmeValue(false, 25.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, LogSchema::bmeValue(true, -1000.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, LogSchema::bmeValue(true, 25.0f));
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_log_missing_value_is_minus_one);
    RUN_TEST(test_ntc_column_matches_feature_flag);
    RUN_TEST(test_state_column_follows_pid_feature_flag);
    RUN_TEST(test_capture_state_text);
    RUN_TEST(test_plantower_columns_follow_session_flag);
    RUN_TEST(test_bme_invalid_maps_to_missing_value);
    UNITY_END();
}

void loop() {}
