#include <Arduino.h>
#include <unity.h>
#include "Data/Logging/LogSchema.h"

struct FakeSession
{
    bool usePlantower = true;
    float capturedVolume = 0.0f;
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

class StringPrint : public Print
{
public:
    size_t write(uint8_t value) override
    {
        output += static_cast<char>(value);
        return 1;
    }

    String output;
};

static int fieldCount(const String &csv)
{
    if (csv.length() == 0)
        return 0;

    int count = 1;
    for (size_t i = 0; i < csv.length(); ++i)
    {
        if (csv.charAt(i) == ',')
            ++count;
    }
    return count;
}

static String fieldAt(const String &csv, int targetIndex)
{
    int fieldIndex = 0;
    int start = 0;
    for (size_t i = 0; i <= csv.length(); ++i)
    {
        if (i == csv.length() || csv.charAt(i) == ',')
        {
            if (fieldIndex == targetIndex)
                return csv.substring(start, i);
            start = i + 1;
            ++fieldIndex;
        }
    }
    return String();
}

static int columnIndex(const String &header, const char *column)
{
    int count = fieldCount(header);
    for (int i = 0; i < count; ++i)
    {
        if (fieldAt(header, i) == column)
            return i;
    }
    return -1;
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

void test_captured_volume_is_mandatory_and_follows_flow_target()
{
    for (int flags = 0; flags < 16; ++flags)
    {
        String header = LogSchema::header(flags & 1, flags & 2, flags & 4, flags & 8);
        int targetIndex = columnIndex(header, "flow_target");
        TEST_ASSERT_GREATER_OR_EQUAL(0, targetIndex);
        TEST_ASSERT_EQUAL(targetIndex + 1, columnIndex(header, "captured_volume"));
    }
}

void test_log_record_row_matches_header_and_formats_volume()
{
    LogRecord record;
    record.timestampUnix = 1704067200UL;
    record.targetFlow = 12.5f;
    record.capturedVolume = 1.23456f;

    for (int flags = 0; flags < 16; ++flags)
    {
        bool includeState = flags & 1;
        bool includePlantower = flags & 2;
        bool includeAnemometer = flags & 4;
        bool includeNtc = flags & 8;
        String header = LogSchema::header(includeState, includePlantower,
                                          includeAnemometer, includeNtc);
        StringPrint output;
        LogSchema::writeRow(output, record, includeState, includePlantower,
                            includeAnemometer, includeNtc);
        output.output.trim();

        TEST_ASSERT_EQUAL(fieldCount(header), fieldCount(output.output));
        int volumeIndex = columnIndex(header, "captured_volume");
        TEST_ASSERT_EQUAL_STRING("1.235", fieldAt(output.output, volumeIndex).c_str());
    }
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
    RUN_TEST(test_captured_volume_is_mandatory_and_follows_flow_target);
    RUN_TEST(test_log_record_row_matches_header_and_formats_volume);
    UNITY_END();
}

void loop() {}
