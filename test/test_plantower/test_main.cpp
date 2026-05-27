#include <Arduino.h>
#include <unity.h>
#include "Sensors/Plantower.h"
#include "Config.h"

void test_parser_valid_frame() {
    PlantowerParser parser;
    // Trama de ejemplo (32 bytes total, longitud 28 en datos)
    // PM1.0 = 10 (0x000A), PM2.5 = 25 (0x0019), PM10 = 100 (0x0064)
    // El buffer en processByte[6..11] mapea a estos valores.
    // La trama típica de Plantower es de 32 bytes:
    // 0: 0x42 
    // 1: 0x4D
    // 2-3: Longitud (0x00 0x1C = 28)
    // 4-5: PM1.0 CF=1
    // 6-7: PM2.5 CF=1
    // 8-9: PM10 CF=1
    // 10-11: PM1.0 atmospheric (Lo que lee el parser actualmente buffer[6-7])
    // 12-13: PM2.5 atmospheric (Lo que lee el parser actualmente buffer[8-9])
    // 14-15: PM10 atmospheric (Lo que lee el parser actualmente buffer[10-11])
    // ...
    // 30-31: Checksum

    uint8_t frame[32] = {0};
    frame[0] = 0x42;
    frame[1] = 0x4D;
    frame[2] = 0x00;
    frame[3] = 0x1C; // 28 bytes de datos
    
    // El parser usa buffer[6-7], [8-9], [10-11]
    // Estos corresponden a los bytes 10-11, 12-13, 14-15 de la trama completa (porque buffer empieza en byte 4)
    frame[10] = 0x00; frame[11] = 0x0A; // PM1.0 = 10
    frame[12] = 0x00; frame[13] = 0x19; // PM2.5 = 25
    frame[14] = 0x00; frame[15] = 0x64; // PM10 = 100

    uint16_t checksum = 0;
    for(int i=0; i<30; i++) checksum += frame[i];
    frame[30] = (checksum >> 8);
    frame[31] = (checksum & 0xFF);

    bool complete = false;
    for(int i=0; i<32; i++) {
        if (parser.processByte(frame[i])) {
            complete = true;
        }
    }

    TEST_ASSERT_TRUE(complete);
    PlantowerData data = parser.getData();
    TEST_ASSERT_TRUE(data.valid);
    TEST_ASSERT_EQUAL_UINT16(10, data.pm1_0);
    TEST_ASSERT_EQUAL_UINT16(25, data.pm2_5);
    TEST_ASSERT_EQUAL_UINT16(100, data.pm10_0);
}

void test_parser_invalid_checksum() {
    PlantowerParser parser;
    uint8_t frame[32] = {0};
    frame[0] = 0x42; frame[1] = 0x4D;
    frame[2] = 0x00; frame[3] = 0x1C;
    frame[30] = 0xFF; frame[31] = 0xFF; // Checksum erróneo

    bool complete = false;
    for(int i=0; i<32; i++) {
        if (parser.processByte(frame[i])) complete = true;
    }

    TEST_ASSERT_FALSE(complete);
    TEST_ASSERT_FALSE(parser.getData().valid);
}

void test_parser_resync_after_noise() {
    PlantowerParser parser;
    
    // Enviar basura
    parser.processByte(0xFF);
    parser.processByte(0x42); // Posible inicio pero falso
    parser.processByte(0x01);

    // Enviar trama válida
    uint8_t frame[32] = {0};
    frame[0] = 0x42; frame[1] = 0x4D;
    frame[2] = 0x00; frame[3] = 0x1C;
    uint16_t checksum = 0;
    for(int i=0; i<30; i++) checksum += frame[i];
    frame[30] = (checksum >> 8);
    frame[31] = (checksum & 0xFF);

    bool complete = false;
    for(int i=0; i<32; i++) {
        if (parser.processByte(frame[i])) complete = true;
    }

    TEST_ASSERT_TRUE(complete);
    TEST_ASSERT_TRUE(parser.getData().valid);
}

void test_real_sensor_reading() {
    // Encender periféricos
    #if PPH_PWR_PIN >= 0
    pinMode(PPH_PWR_PIN, OUTPUT);
    digitalWrite(PPH_PWR_PIN, HIGH);
    #endif
    
    TEST_MESSAGE("Encendiendo periféricos y esperando estabilización (2s)...");
    delay(2000); // Tiempo para que el sensor arranque

    Plantower sensor;
    sensor.begin();
    
    TEST_MESSAGE("Esperando datos reales del sensor (10 segundos)...");
    
    PlantowerData data;
    bool found = false;
    unsigned long start = millis();
    
    while (millis() - start < 10000) { // Aumentamos a 10s por si tarda en estabilizarse el flujo
        if (sensor.getData(data)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "LECTURA REAL: PM1.0=%d, PM2.5=%d, PM10.0=%d", 
                     data.pm1_0, data.pm2_5, data.pm10_0);
            TEST_MESSAGE(msg);
            found = true;
            break;
        }
        delay(100);
    }
    
    // Opcional: Apagar periféricos al terminar el test
    // digitalWrite(PPH_PWR_PIN, LOW);
    
    TEST_ASSERT_TRUE_MESSAGE(found, "No se recibió ninguna trama válida del sensor. ¿Está el sensor conectado a los pines correctos?");
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_parser_valid_frame);
    RUN_TEST(test_parser_invalid_checksum);
    RUN_TEST(test_parser_resync_after_noise);
    RUN_TEST(test_real_sensor_reading);
    UNITY_END();
}

void loop() {}
