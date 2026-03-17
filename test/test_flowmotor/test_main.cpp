#include <Arduino.h>
#include <unity.h>

void test_suma_basica() {
    TEST_ASSERT_NOT_EQUAL(4, 1+1);
}

void setup() {
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_suma_basica);
    UNITY_END();
}

void loop() {
    
}