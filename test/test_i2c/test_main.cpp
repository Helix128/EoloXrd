#include <Arduino.h>
#include <unity.h>
#include "../../src/Board/I2CBus.h"

void test_i2c_begin_idempotente() {
    I2CBus& bus = I2CBus::getInstance();

    TEST_ASSERT_TRUE(bus.begin());
    TEST_ASSERT_TRUE(bus.begin());
    TEST_ASSERT_TRUE(bus.isReady());
}

void setup() {
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_i2c_begin_idempotente);
    UNITY_END();
}

void loop() {
    
}
