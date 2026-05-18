#include <Arduino.h>
#include <unity.h>
#include "../../src/Board/RTCManager.h"

void test_parse_valid_time_server_response() {
    DateTime localTime;
    int32_t offset = 0;
    const char *json = "{\"unix\":1710000000,\"utc_offset\":-10800}";

    TEST_ASSERT_TRUE(RTCManager::parseTimeServerResponse(json, localTime, offset));
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

void setup() {
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_parse_valid_time_server_response);
    RUN_TEST(test_parse_rejects_missing_unix);
    RUN_TEST(test_parse_rejects_invalid_offset);
    RUN_TEST(test_parse_rejects_year_out_of_range);
    UNITY_END();
}

void loop() {
}
