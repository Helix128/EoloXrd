#include <Arduino.h>
#include <unity.h>
#include <atomic>
#include "../../src/Board/RS485Bus.h"
#include "../../src/Config/Legacy.h"

// Prueba de placa: no presupone que los dos instrumentos estén conectados.
// La matriz de disponibilidad se informa por las estadísticas de cada endpoint;
// sólo se verifica que ambos sean planificados y que el opcional no bloquee al
// AFM07. Las pruebas de parser/tramas se ejecutan en la suite nativa de core.

static std::atomic<uint32_t> anemometerCallbacks{0};
static std::atomic<uint32_t> afmCallbacks{0};

static void anemometerCallback(void*, bool, const uint16_t*, uint8_t, uint8_t) {
    anemometerCallbacks.fetch_add(1, std::memory_order_relaxed);
}
static void afmCallback(void*, bool, const uint16_t*, uint8_t, uint8_t) {
    afmCallbacks.fetch_add(1, std::memory_order_relaxed);
}

static void powerOnRs485Module() {
#if PPH_PWR_PIN >= 0
    pinMode(PPH_PWR_PIN, OUTPUT);
    digitalWrite(PPH_PWR_PIN, HIGH);
    delay(200);
#endif
}

void test_begin_keeps_transceiver_in_receive_mode() {
    powerOnRs485Module();
    RS485Bus::getInstance().begin();
    delay(20);
    TEST_ASSERT_EQUAL(LOW, digitalRead(RS485_DE_RE_PIN));
}

void test_legacy_blocking_api_is_rejected_immediately() {
    uint16_t data[2] = {0xAAAA, 0x5555};
    const uint32_t started = millis();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    TEST_ASSERT_FALSE(RS485Bus::getInstance().readRegisters(1, 0, 2, data));
#pragma GCC diagnostic pop
    TEST_ASSERT_LESS_THAN_UINT32(20, millis() - started);
    TEST_ASSERT_EQUAL_HEX16(0xAAAA, data[0]);
    TEST_ASSERT_EQUAL_HEX16(0x5555, data[1]);
}

void test_scheduler_accepts_only_fixed_endpoint_shapes() {
    TEST_ASSERT_FALSE(RS485Bus::getInstance().registerEndpoint(3, 0, 1, afmCallback, nullptr));
    TEST_ASSERT_FALSE(RS485Bus::getInstance().registerEndpoint(1, 1, 2, anemometerCallback, nullptr));
    TEST_ASSERT_FALSE(RS485Bus::getInstance().registerEndpoint(2, 0, 0, afmCallback, nullptr));
}

void test_mixed_availability_keeps_afm07_priority_and_bounded_transactions() {
    powerOnRs485Module();
    RS485Bus& bus = RS485Bus::getInstance();
    bus.resetSlaveStats();
    anemometerCallbacks.store(0, std::memory_order_relaxed);
    afmCallbacks.store(0, std::memory_order_relaxed);
    TEST_ASSERT_TRUE(bus.registerEndpoint(1, 0, 2, anemometerCallback, nullptr));
    TEST_ASSERT_TRUE(bus.registerEndpoint(2, 0, 1, afmCallback, nullptr));

    // Cubre dos sondas AFM y, si el anemómetro falta, su paso a retry de 5 s.
    delay(2200);
    RS485SlaveStats anem = bus.getSlaveStats(1);
    RS485SlaveStats afm = bus.getSlaveStats(2);
    TEST_ASSERT_TRUE_MESSAGE(afmCallbacks.load(std::memory_order_relaxed) >= 2,
                             "AFM07 debe seguir sondeandose aunque falte el anemometro.");
    TEST_ASSERT_TRUE_MESSAGE(anemometerCallbacks.load(std::memory_order_relaxed) >= 1,
                             "El endpoint opcional debe recibir su resultado.");
    // A 4800 baud el timeout de respuesta más la guarda de bus queda por
    // debajo de 400 ms; ningún endpoint puede retener el planificador más que
    // eso ni convertir una ausencia en una colisión sostenida.
    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(400, afm.lastLatencyMs,
                                             "Una transaccion AFM07 excedio su presupuesto.");
    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(400, anem.lastLatencyMs,
                                             "Una transaccion de anemometro excedio su presupuesto.");
    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(1200, afm.maxAttemptGapMs,
                                             "El intervalo de intentos AFM07 excedio 1.2 s.");
}

void setup() {
    delay(1200);
    UNITY_BEGIN();
    RUN_TEST(test_begin_keeps_transceiver_in_receive_mode);
    RUN_TEST(test_legacy_blocking_api_is_rejected_immediately);
    RUN_TEST(test_scheduler_accepts_only_fixed_endpoint_shapes);
    RUN_TEST(test_mixed_availability_keeps_afm07_priority_and_bounded_transactions);
    UNITY_END();
}

void loop() {}
