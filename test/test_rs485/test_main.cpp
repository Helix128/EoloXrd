#include <Arduino.h>
#include <unity.h>
#include "../../src/Board/RS485Bus.h"
#include "../../src/Config/Legacy.h"

// Por defecto, las pruebas no requieren tener el sensor real conectado.
// Para ejecutar las pruebas con sensor real:
//   -DRS485_TEST_ENABLE_REAL_SENSOR=1
//   -DRS485_TEST_VALID_SLAVE_ID=1 (o el ID real)

#ifndef RS485_TEST_ENABLE_REAL_SENSOR
#define RS485_TEST_ENABLE_REAL_SENSOR 1
#endif

#ifndef RS485_TEST_VALID_SLAVE_ID
#define RS485_TEST_VALID_SLAVE_ID 1
#endif

#ifndef RS485_TEST_VALID_START_REG
#define RS485_TEST_VALID_START_REG 0x0000
#endif

#ifndef RS485_TEST_VALID_REG_COUNT
#define RS485_TEST_VALID_REG_COUNT 2
#endif

#ifndef RS485_TEST_INVALID_SLAVE_ID
#define RS485_TEST_INVALID_SLAVE_ID 247
#endif

#ifndef RS485_TEST_MAX_INVALID_READ_MS
#define RS485_TEST_MAX_INVALID_READ_MS 8500UL
#endif

#ifndef RS485_TEST_POWER_PIN
#define RS485_TEST_POWER_PIN PPH_PWR_PIN
#endif

#ifndef RS485_TEST_POWER_ACTIVE_LEVEL
#define RS485_TEST_POWER_ACTIVE_LEVEL HIGH
#endif

#ifndef RS485_TEST_CONCURRENT_WAIT_MS
#define RS485_TEST_CONCURRENT_WAIT_MS 25000UL
#endif

#ifndef RS485_TEST_SINGLE_READ_WAIT_MS
#define RS485_TEST_SINGLE_READ_WAIT_MS 15000UL
#endif

#ifndef RS485_TEST_IDLE_WAIT_MS
#define RS485_TEST_IDLE_WAIT_MS 3000UL
#endif

static void powerOnRs485Module() {
#if RS485_TEST_POWER_PIN >= 0
	pinMode(RS485_TEST_POWER_PIN, OUTPUT);
	digitalWrite(RS485_TEST_POWER_PIN, RS485_TEST_POWER_ACTIVE_LEVEL);
	delay(200);
#endif
}

struct Rs485ReadJob {
	uint8_t slaveId;
	uint16_t startReg;
	uint8_t count;
	uint16_t* dataBuffer;
	TaskHandle_t caller;
	bool result;
	volatile bool done;
};

static void rs485ReadTask(void* arg) {
	Rs485ReadJob* job = (Rs485ReadJob*)arg;
	job->result = RS485Bus::getInstance().readRegisters(
		job->slaveId,
		job->startReg,
		job->count,
		job->dataBuffer
	);
	job->done = true;
	xTaskNotifyGive(job->caller);
	vTaskDelete(nullptr);
}

static bool readRegistersFromRs485Core(uint8_t slaveId, uint16_t startReg, uint8_t count, uint16_t* dataBuffer) {
	ulTaskNotifyTake(pdTRUE, 0);

	Rs485ReadJob* job = new Rs485ReadJob{
		slaveId,
		startReg,
		count,
		dataBuffer,
		xTaskGetCurrentTaskHandle(),
		false,
		false
	};

	TaskHandle_t taskHandle = nullptr;
	BaseType_t created = xTaskCreatePinnedToCore(rs485ReadTask, "rs485_read_test", 4096, job, 1, &taskHandle, 0);
	if (created != pdPASS) {
		delete job;
		return false;
	}

	uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(RS485_TEST_SINGLE_READ_WAIT_MS));
	if (notified == 0) {
		// No liberar job: el worker podria seguir usandolo si el bus esta bloqueado.
		return false;
	}

	bool result = job->result;
	delete job;
	return result;
}

static bool waitForBusIdle(uint32_t timeoutMs = RS485_TEST_IDLE_WAIT_MS) {
	unsigned long t0 = millis();
	while (millis() - t0 < timeoutMs) {
		if (RS485Bus::getInstance().getPendingRequests() == 0) {
			return true;
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	return RS485Bus::getInstance().getPendingRequests() == 0;
}

struct ConcurrentReadCtx {
	volatile bool done;
	volatile bool anySuccess;
};

void concurrentReadTask(void* arg) {
	ConcurrentReadCtx* ctx = (ConcurrentReadCtx*)arg;
	uint16_t buffer[2] = {0, 0};

	bool ok = RS485Bus::getInstance().readRegisters(RS485_TEST_INVALID_SLAVE_ID, 0x0000, 2, buffer);
	if (ok) {
		ctx->anySuccess = true;
	}

	ctx->done = true;
	vTaskDelete(nullptr);
}

void test_begin_is_idempotent_and_pin_state_is_rx_mode() {
	powerOnRs485Module();
	RS485Bus::getInstance().begin();
	TEST_ASSERT_TRUE_MESSAGE(waitForBusIdle(), "La cola RS485 debe iniciar vacía.");
	RS485Bus::getInstance().begin();

	int deReState = digitalRead(RS485_DE_RE_PIN);
	TEST_ASSERT_EQUAL_MESSAGE(LOW, deReState, "DE/RE debe quedar en LOW (modo recepción) tras begin().");
	TEST_ASSERT_TRUE_MESSAGE(waitForBusIdle(), "La cola RS485 debe quedar vacía tras begin().");
}

void test_invalid_slave_returns_false_and_buffer_is_not_overwritten() {
	powerOnRs485Module();
	RS485Bus::getInstance().begin();
	TEST_ASSERT_TRUE_MESSAGE(waitForBusIdle(), "La cola RS485 debe iniciar vacía.");

	uint16_t buffer[2] = {0xAAAA, 0x5555};
	bool ok = readRegistersFromRs485Core(RS485_TEST_INVALID_SLAVE_ID, 0x0000, 2, buffer);

	TEST_ASSERT_FALSE_MESSAGE(ok, "Con slave inexistente, readRegisters debe retornar false.");
	TEST_ASSERT_EQUAL_HEX16_MESSAGE(0xAAAA, buffer[0], "No debe sobreescribir buffer en fallo.");
	TEST_ASSERT_EQUAL_HEX16_MESSAGE(0x5555, buffer[1], "No debe sobreescribir buffer en fallo.");
	TEST_ASSERT_TRUE_MESSAGE(waitForBusIdle(), "La cola RS485 debe quedar vacía tras lectura inválida.");
}

void test_invalid_arguments_are_rejected_without_enqueue() {
	powerOnRs485Module();
	RS485Bus::getInstance().begin();
	TEST_ASSERT_TRUE_MESSAGE(waitForBusIdle(), "La cola RS485 debe iniciar vacía.");

	uint16_t buffer[2] = {0xAAAA, 0x5555};

	TEST_ASSERT_FALSE_MESSAGE(
		RS485Bus::getInstance().readRegisters(RS485_TEST_VALID_SLAVE_ID, 0x0000, 0, buffer),
		"count=0 debe rechazarse antes de encolar."
	);
	TEST_ASSERT_FALSE_MESSAGE(
		RS485Bus::getInstance().readRegisters(RS485_TEST_VALID_SLAVE_ID, 0x0000, 65, buffer),
		"count mayor al buffer de ModbusMaster debe rechazarse antes de encolar."
	);
	TEST_ASSERT_FALSE_MESSAGE(
		RS485Bus::getInstance().readRegisters(RS485_TEST_VALID_SLAVE_ID, 0x0000, 2, nullptr),
		"buffer nulo debe rechazarse antes de encolar."
	);

	TEST_ASSERT_EQUAL_HEX16_MESSAGE(0xAAAA, buffer[0], "Validación fallida no debe sobreescribir buffer.");
	TEST_ASSERT_EQUAL_HEX16_MESSAGE(0x5555, buffer[1], "Validación fallida no debe sobreescribir buffer.");
	TEST_ASSERT_EQUAL_MESSAGE(0, RS485Bus::getInstance().getPendingRequests(), "Entradas inválidas no deben encolar solicitudes.");
}

void test_invalid_slave_returns_within_reasonable_timeout() {
	powerOnRs485Module();
	RS485Bus::getInstance().begin();
	TEST_ASSERT_TRUE_MESSAGE(waitForBusIdle(), "La cola RS485 debe iniciar vacía.");

	uint16_t buffer[2] = {0, 0};
	unsigned long t0 = millis();
	bool ok = readRegistersFromRs485Core(RS485_TEST_INVALID_SLAVE_ID, 0x0000, 2, buffer);
	unsigned long dt = millis() - t0;

	TEST_ASSERT_FALSE_MESSAGE(ok, "Con slave inexistente no debe retornar éxito.");
	TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(
		RS485_TEST_MAX_INVALID_READ_MS,
		dt,
		"Timeout excesivo: posible bloqueo/espera anómala en lectura inválida."
	);
	TEST_ASSERT_TRUE_MESSAGE(waitForBusIdle(), "La cola RS485 debe quedar vacía tras timeout inválido.");
}

void test_concurrent_reads_do_not_deadlock() {
	powerOnRs485Module();
	RS485Bus::getInstance().begin();
	TEST_ASSERT_TRUE_MESSAGE(waitForBusIdle(), "La cola RS485 debe iniciar vacía.");

	ConcurrentReadCtx a = {false, false};
	ConcurrentReadCtx b = {false, false};

	TaskHandle_t ta = nullptr;
	TaskHandle_t tb = nullptr;

	BaseType_t ra = xTaskCreatePinnedToCore(concurrentReadTask, "rs485_ta", 4096, &a, 1, &ta, 0);
	BaseType_t rb = xTaskCreatePinnedToCore(concurrentReadTask, "rs485_tb", 4096, &b, 1, &tb, 0);

	TEST_ASSERT_EQUAL(pdPASS, ra);
	TEST_ASSERT_EQUAL(pdPASS, rb);

	unsigned long t0 = millis();
	while (!(a.done && b.done) && (millis() - t0 < RS485_TEST_CONCURRENT_WAIT_MS)) {
		vTaskDelay(pdMS_TO_TICKS(10));
	}

	TEST_ASSERT_TRUE_MESSAGE(a.done && b.done, "Lecturas concurrentes no deben quedar bloqueadas (deadlock). ");
	TEST_ASSERT_TRUE_MESSAGE(waitForBusIdle(), "La cola RS485 debe quedar vacía tras lecturas concurrentes.");
}

void test_ui_core_read_is_rejected_quickly() {
	powerOnRs485Module();
	RS485Bus::getInstance().begin();
	TEST_ASSERT_TRUE_MESSAGE(waitForBusIdle(), "La cola RS485 debe iniciar vacía.");

	uint16_t buffer[2] = {0, 0};
	unsigned long t0 = millis();
	bool ok = RS485Bus::getInstance().readRegisters(RS485_TEST_INVALID_SLAVE_ID, 0x0000, 2, buffer);
	unsigned long dt = millis() - t0;

	TEST_ASSERT_FALSE_MESSAGE(ok, "Una lectura RS485 desde core UI debe rechazarse.");
	TEST_ASSERT_LESS_THAN_UINT32_MESSAGE(100UL, dt, "La lectura desde core UI debe fallar rápido.");
	TEST_ASSERT_EQUAL_MESSAGE(0, RS485Bus::getInstance().getPendingRequests(), "La lectura rechazada desde UI no debe encolar solicitudes.");
}

void test_real_sensor_success_read_and_stability() {
#if RS485_TEST_ENABLE_REAL_SENSOR
	powerOnRs485Module();
	RS485Bus::getInstance().begin();
	TEST_ASSERT_TRUE_MESSAGE(waitForBusIdle(), "La cola RS485 debe iniciar vacía.");

	const int N = 60;
	int successCount = 0;
	int alternatingPatternHits = 0;
	bool lastOk = false;
	bool hasLast = false;

	uint16_t data[RS485_TEST_VALID_REG_COUNT];

	for (int i = 0; i < N; ++i) {
		bool ok = readRegistersFromRs485Core(
			RS485_TEST_VALID_SLAVE_ID,
			RS485_TEST_VALID_START_REG,
			RS485_TEST_VALID_REG_COUNT,
			data
		);

		if (ok) successCount++;

		if (hasLast && (ok != lastOk)) {
			alternatingPatternHits++;
		}

		lastOk = ok;
		hasLast = true;
		vTaskDelay(pdMS_TO_TICKS(50));
	}

	float successRate = (float)successCount / (float)N;

	TEST_ASSERT_TRUE_MESSAGE(successRate >= 0.95f, "Tasa de éxito RS485 insuficiente en prueba de estabilidad.");
	TEST_ASSERT_TRUE_MESSAGE(alternatingPatternHits < (N / 2), "Se detectó patrón alternado anómalo (ok/fail repetitivo).");
	TEST_ASSERT_EQUAL_MESSAGE(
		0,
		RS485Bus::getInstance().getPendingRequests(),
		"La cola de solicitudes debe estar vacía al finalizar las lecturas."
	);
	TEST_ASSERT_TRUE_MESSAGE(waitForBusIdle(), "La cola RS485 debe quedar vacía tras estabilidad.");
#else
	TEST_IGNORE_MESSAGE("Prueba de sensor real deshabilitada. Define RS485_TEST_ENABLE_REAL_SENSOR=1 para habilitarla.");
#endif
}

void test_sequential_reads_from_multiple_sensors() {
#if RS485_TEST_ENABLE_REAL_SENSOR
	powerOnRs485Module();
	RS485Bus::getInstance().begin();
	TEST_ASSERT_TRUE_MESSAGE(waitForBusIdle(), "La cola RS485 debe iniciar vacía.");

	uint16_t sensorA_data[2];
	uint16_t sensorB_data[2];

	// Simular lecturas secuenciales de dos sensores RS485
	for (int cycle = 0; cycle < 5; ++cycle) {
		// Leer sensor A (ID 1)
		bool okA = readRegistersFromRs485Core(1, 0x0000, 2, sensorA_data);
		TEST_ASSERT_TRUE_MESSAGE(okA, "Lectura sensor A (ID 1) falló");

		vTaskDelay(pdMS_TO_TICKS(10));

		// Leer sensor B (ID 2)
		bool okB = readRegistersFromRs485Core(2, 0x0000, 2, sensorB_data);
		TEST_ASSERT_TRUE_MESSAGE(okB, "Lectura sensor B (ID 2) falló");

		vTaskDelay(pdMS_TO_TICKS(50));
	}

	TEST_ASSERT_EQUAL_MESSAGE(
		0,
		RS485Bus::getInstance().getPendingRequests(),
		"La cola debe estar vacía tras lecturas secuenciales completadas."
	);
	TEST_ASSERT_TRUE_MESSAGE(waitForBusIdle(), "La cola RS485 debe quedar vacía tras lecturas secuenciales.");
#else
	TEST_IGNORE_MESSAGE("Prueba requiere sensor real. Define RS485_TEST_ENABLE_REAL_SENSOR=1 para habilitarla.");
#endif
}

void setup() {
	delay(1200);
	UNITY_BEGIN();

	RUN_TEST(test_begin_is_idempotent_and_pin_state_is_rx_mode);
	RUN_TEST(test_invalid_slave_returns_false_and_buffer_is_not_overwritten);
	RUN_TEST(test_invalid_arguments_are_rejected_without_enqueue);
	RUN_TEST(test_invalid_slave_returns_within_reasonable_timeout);
	RUN_TEST(test_concurrent_reads_do_not_deadlock);
	RUN_TEST(test_ui_core_read_is_rejected_quickly);
	RUN_TEST(test_real_sensor_success_read_and_stability);
	RUN_TEST(test_sequential_reads_from_multiple_sensors);

	UNITY_END();
}

void loop() {}
