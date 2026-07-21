#include <Arduino.h>
#include <unity.h>
#include "Sensors/AFM07.h"
#include "Effectors/Motor.h"
#include "Config/Legacy.h"

// Hacer el sensor global para que la tarea de FreeRTOS persista entre tests
AFM07 sensor;
MotorManager motors;

void test_afm07_reading() {
    TEST_MESSAGE("Esperando datos reales del AFM07 (15 segundos)...");
    
    FlowData data;
    bool found = false;
    unsigned long start = millis();
    
    while (millis() - start < 15000) { 
        if (sensor.getData(data)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "LECTURA AFM07: Flujo=%.2f L/min, Vel=%.2f m/s", 
                     data.flow, data.velocity);
            TEST_MESSAGE(msg);
            found = true;
            break;
        }
        delay(500); // Dar más tiempo entre intentos de lectura del buffer
    }
    
    TEST_ASSERT_TRUE_MESSAGE(found, "No se recibió ninguna trama válida del AFM07. Revisa conexión RS485 e ID (debe ser 2).");
}

void test_afm07_with_motor() {
    TEST_MESSAGE("Encendiendo motores al 100%...");
    motors.setPowerPct(100);
    
    TEST_MESSAGE("Esperando 5 segundos a que el flujo sea máximo...");
    delay(5000); 

    FlowData data;
    bool validRead = false;
    float currentFlow = 0.0;

    // Intentamos captar el flujo durante 5 segundos
    unsigned long start = millis();
    while (millis() - start < 5000) {
        if (sensor.getData(data)) {
            validRead = true;
            currentFlow = data.flow;
            char msg[128];
            snprintf(msg, sizeof(msg), "Flujo detectado a máxima potencia: %.2f L/min", currentFlow);
            TEST_MESSAGE(msg);
            if (currentFlow > 0.5) break;
        }
        delay(200);
    }

    motors.setPowerPct(0);
    
    TEST_ASSERT_TRUE_MESSAGE(validRead, "El sensor no devolvió datos válidos durante la prueba de motor");
    TEST_ASSERT_GREATER_THAN_FLOAT_MESSAGE(0.5, currentFlow, "FLUJO INSUFICIENTE: Motores al 100% pero el sensor marca casi 0. ¿Están las mangueras conectadas?");
}

void setup() {
    delay(2000);
    
    // Encendido crítico de periféricos
    #if PPH_PWR_PIN >= 0
    pinMode(PPH_PWR_PIN, OUTPUT);
    digitalWrite(PPH_PWR_PIN, HIGH);
    #endif
    
    LOG_OUT_LN("--- ENERGÍA PERIFÉRICOS ACTIVADA ---");
    delay(3000); // 3 segundos para que el sensor AFM07 arranque su firmware

    sensor.begin();
    motors.begin();

    UNITY_BEGIN();
    RUN_TEST(test_afm07_reading);
    RUN_TEST(test_afm07_with_motor);
    UNITY_END();
}

void loop() {}
