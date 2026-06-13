#include <Arduino.h>
#include <unity.h>
#include "../../src/Board/RTCManager.h"
#include <Eolo/Core/Time/RtcTimeParser.h>

void test_parse_valid_time_server_response() {
    DateTime localTime;
    int32_t offset = 0;
    const char *json = "{\"unix\":1710000000,\"utc_offset\":-10800}";

    TEST_ASSERT_TRUE(RtcTimeParser::parseTimeServerResponse(json, localTime, offset));
    TEST_ASSERT_EQUAL_INT32(-10800, offset);
    TEST_ASSERT_EQUAL_UINT32(1709989200UL, localTime.unixtime());
}

void test_parse_rejects_missing_unix() {
    DateTime localTime;
    int32_t offset = 0;

    TEST_ASSERT_FALSE(RTCManager::parseTimeServerResponse("{\"utc_offset\":-10800}", localTime, offset));
}

void test_parse_rejects_invalid_offset() {
    DateTime localTime;
    int32_t offset = 0;

    TEST_ASSERT_FALSE(RTCManager::parseTimeServerResponse("{\"unix\":1710000000,\"utc_offset\":999999}", localTime, offset));
}

void test_parse_rejects_year_out_of_range() {
    DateTime localTime;
    int32_t offset = 0;

    TEST_ASSERT_FALSE(RTCManager::parseTimeServerResponse("{\"unix\":4102444800,\"utc_offset\":0}", localTime, offset));
}

void test_parse_manual_space_datetime() {
    DateTime localTime;

    TEST_ASSERT_TRUE(RtcTimeParser::parseDateTime("2026-05-26 14:30:45", localTime));
    TEST_ASSERT_EQUAL_UINT16(2026, localTime.year());
    TEST_ASSERT_EQUAL_UINT8(5, localTime.month());
    TEST_ASSERT_EQUAL_UINT8(26, localTime.day());
    TEST_ASSERT_EQUAL_UINT8(14, localTime.hour());
    TEST_ASSERT_EQUAL_UINT8(30, localTime.minute());
    TEST_ASSERT_EQUAL_UINT8(45, localTime.second());
}

void test_parse_manual_iso_datetime() {
    DateTime localTime;

    TEST_ASSERT_TRUE(RtcTimeParser::parseDateTime("2026-05-26T14:30:45", localTime));
    TEST_ASSERT_EQUAL_UINT16(2026, localTime.year());
    TEST_ASSERT_EQUAL_UINT8(5, localTime.month());
    TEST_ASSERT_EQUAL_UINT8(26, localTime.day());
    TEST_ASSERT_EQUAL_UINT8(14, localTime.hour());
    TEST_ASSERT_EQUAL_UINT8(30, localTime.minute());
    TEST_ASSERT_EQUAL_UINT8(45, localTime.second());
}

void test_parse_manual_rejects_invalid_datetime() {
    DateTime localTime;

    TEST_ASSERT_FALSE(RtcTimeParser::parseDateTime("2023-12-31 23:59:59", localTime));
    TEST_ASSERT_FALSE(RtcTimeParser::parseDateTime("2026-05-26 25:30:45", localTime));
    TEST_ASSERT_FALSE(RtcTimeParser::parseDateTime("2026/05/26 14:30:45", localTime));
}

void test_epoch_with_offset() {
    DateTime localTime;

    TEST_ASSERT_TRUE(RtcTimeParser::fromUnixWithOffset(1710000000UL, -10800, localTime));
    TEST_ASSERT_EQUAL_UINT32(1709989200UL, localTime.unixtime());
}

void test_epoch_rejects_invalid_offset() {
    DateTime localTime;

    TEST_ASSERT_FALSE(RtcTimeParser::fromUnixWithOffset(1710000000UL, 999999, localTime));
}

void setup() {
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_parse_valid_time_server_response);
    RUN_TEST(test_parse_rejects_missing_unix);
    RUN_TEST(test_parse_rejects_invalid_offset);
    RUN_TEST(test_parse_rejects_year_out_of_range);
    RUN_TEST(test_parse_manual_space_datetime);
    RUN_TEST(test_parse_manual_iso_datetime);
    RUN_TEST(test_parse_manual_rejects_invalid_datetime);
    RUN_TEST(test_epoch_with_offset);
    RUN_TEST(test_epoch_rejects_invalid_offset);
    UNITY_END();
}

void loop() {
}
