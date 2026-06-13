#ifndef RS485_BUS_HPP
#define RS485_BUS_HPP

#include <Arduino.h>
#include <ModbusMaster.h>
#include <SoftwareSerial.h>
#include <atomic>
#include <climits>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "../Config.h"
#include "../Utility/RS485Monitor.h"
#include "../Utility/RS485PatternValidator.h"

#define RS485_BAUD_RATE 4800
#define RS485_BUS_GUARD_TIME_MS 20     // Tiempo de silencio antes de cada transmisión
#define RS485_INTER_FRAME_TIME_US 100  // Tiempo entre fin de transmisión y bajada DE/RE

#define DEBUG true

// Estructura de solicitud al bus RS485
struct RS485Request {
    uint8_t slaveId;
    uint16_t startReg;
    uint8_t count;
    uint16_t* dataBuffer;
    TaskHandle_t callerTask;  // Tarea a notificar al terminar
};

class RS485Bus {
private:
    SoftwareSerial _serial;
    ModbusMaster _node;
    SemaphoreHandle_t _initMutex;
    std::atomic<bool> _initialized;
    static RS485Bus* _activeBus;
    uint8_t _consecutiveFailures = 0;

    TaskHandle_t _busTaskHandle = nullptr;
    QueueHandle_t _requestQueue = nullptr;

    static const int QUEUE_LENGTH = 8;
    static const int BUS_TASK_STACK_SIZE = 4096;
    static const int BUS_TASK_PRIORITY = 2;
    static const BaseType_t BUS_TASK_CORE = 0;
    static const BaseType_t UI_LOOP_CORE = 1;
    static const int MAX_RETRIES = 2;
    static const uint8_t MAX_READ_REGISTERS = 64;
    static const uint32_t MODBUS_RESPONSE_TIMEOUT_MS = 2000;
    static const uint32_t REQUEST_QUEUE_SEND_TIMEOUT_MS = 100;
    static const uint32_t RX_DRAIN_QUIET_MS = 12;
    static const uint32_t RX_DRAIN_MAX_MS = 80;
    static const uint8_t RECOVER_AFTER_FAILURES = 3;
    static const uint32_t READ_TIMEOUT_MS =
        ((MAX_RETRIES + 1) * (MODBUS_RESPONSE_TIMEOUT_MS + RS485_BUS_GUARD_TIME_MS)) +
        (MAX_RETRIES * 150) +
        1000;

    RS485Bus() : _serial(RS485_RX_PIN, RS485_TX_PIN), _initialized(false) {
        _initMutex = xSemaphoreCreateMutex();
    }

    RS485Bus(const RS485Bus&) = delete;
    RS485Bus& operator=(const RS485Bus&) = delete;

    static const char* getErrorString(uint8_t errorCode) {
        switch (errorCode) {
            case ModbusMaster::ku8MBSuccess: return "Exito";
            case ModbusMaster::ku8MBIllegalFunction: return "Funcion Ilegal";
            case ModbusMaster::ku8MBIllegalDataAddress: return "Direccion Ilegal";
            case ModbusMaster::ku8MBIllegalDataValue: return "Valor Ilegal";
            case ModbusMaster::ku8MBSlaveDeviceFailure: return "Fallo en Esclavo";
            case ModbusMaster::ku8MBInvalidSlaveID: return "ID Esclavo Invalido";
            case ModbusMaster::ku8MBInvalidFunction: return "Funcion Invalida";
            case ModbusMaster::ku8MBResponseTimedOut: return "Timeout (Sensor no responde)";
            case ModbusMaster::ku8MBInvalidCRC: return "CRC Invalido (Ruido en el bus)";
            default: return "Error Desconocido";
        }
    }

    static void _preTransmission() {
        digitalWrite(RS485_DE_RE_PIN, HIGH);
    }

    static void _postTransmission() {
        if (_activeBus != nullptr) {
            _activeBus->_serial.flush();
        }
        // A 4800 baudios, un bit dura ~208us. 
        // Damos tiempo a que el bit de parada salga antes de bajar el pin DE/RE.
        delayMicroseconds(RS485_INTER_FRAME_TIME_US); 
        digitalWrite(RS485_DE_RE_PIN, LOW);
    }

    static void _modbusIdle() {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    static uint32_t packNotificationValue(bool success, uint8_t errorCode) {
        return (success ? 0x80000000UL : 0) | (uint32_t)errorCode;
    }

    static void clearCurrentTaskNotification() {
        uint32_t discarded = 0;
        xTaskNotifyWait(0, ULONG_MAX, &discarded, 0);
    }

    void drainRx(uint32_t quietMs = RX_DRAIN_QUIET_MS, uint32_t maxMs = RX_DRAIN_MAX_MS) {
        unsigned long startMs = millis();
        unsigned long lastByteMs = millis();

        while ((millis() - startMs) < maxMs) {
            bool consumed = false;
            while (_serial.available() > 0) {
                (void)_serial.read();
                consumed = true;
                lastByteMs = millis();
            }

            if (!consumed && (millis() - lastByteMs) >= quietMs) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    void recoverBus() {
        digitalWrite(RS485_DE_RE_PIN, LOW);
        drainRx(20, 100);
        _serial.end();
        vTaskDelay(pdMS_TO_TICKS(20));
        _serial.begin(RS485_BAUD_RATE, SWSERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
        _node.begin(1, _serial);
        _node.preTransmission(_preTransmission);
        _node.postTransmission(_postTransmission);
        _node.idle(_modbusIdle);
        drainRx(20, 100);
    }

    // Task estática que procesa la cola de solicitudes
    static void _busTask(void* arg) {
        RS485Bus* self = (RS485Bus*)arg;
        RS485Request request;

        while (true) {
            // Esperar solicitud con timeout para permitir que la tarea sea eliminada
            if (xQueueReceive(self->_requestQueue, &request, pdMS_TO_TICKS(100)) == pdTRUE) {
                bool success = false;
                uint8_t errorCode = 0;

                uint32_t transactionStartMs = millis();
                self->_processRequest(&request, success, errorCode);
                uint32_t transactionMs = millis() - transactionStartMs;

                RS485Monitor::getInstance().recordRequestCompleted(success, errorCode, request.slaveId, transactionMs);
                RS485Monitor::getInstance().recordQueueDepth(uxQueueMessagesWaiting(self->_requestQueue));
            }
        }
    }

    // Procesar una solicitud individual
    void _processRequest(RS485Request* request, bool& outSuccess, uint8_t& outErrorCode) {
        if (!_initialized) {
            outSuccess = false;
            outErrorCode = ModbusMaster::ku8MBInvalidSlaveID;
            if (request->callerTask) {
                xTaskNotify(
                    request->callerTask,
                    packNotificationValue(false, outErrorCode),
                    eSetValueWithOverwrite
                );
            }
            return;
        }

        bool success = false;
        uint8_t result = ModbusMaster::ku8MBResponseTimedOut;
        int retries = MAX_RETRIES;

        while (retries >= 0) {
            // Tiempo de guarda: Silencio en el bus para que los esclavos se estabilicen
            vTaskDelay(pdMS_TO_TICKS(RS485_BUS_GUARD_TIME_MS)); 

            // Limpiar bytes tardios de una transaccion anterior antes de transmitir.
            drainRx();

            _node.clearResponseBuffer();
            _node.begin(request->slaveId, _serial);
            result = _node.readHoldingRegisters(request->startReg, request->count);

            if (result == _node.ku8MBSuccess) {
                for (uint8_t i = 0; i < request->count; i++) {
                    request->dataBuffer[i] = _node.getResponseBuffer(i);
                }
                success = true;
                break; // Éxito: salimos de los reintentos
            } 

            // Si hay error, registramos y esperamos un poco antes de reintentar
            retries--;
            if (retries >= 0) {
                drainRx();
                if (result == ModbusMaster::ku8MBResponseTimedOut) {
                    vTaskDelay(pdMS_TO_TICKS(150)); // Mayor espera para que el sensor se recupere
                } else {
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }

        if (!success) {
            drainRx(20, 100);
            if (++_consecutiveFailures >= RECOVER_AFTER_FAILURES) {
                recoverBus();
                _consecutiveFailures = 0;
            }

            static unsigned long lastFailureLogMs = 0;
            unsigned long nowMs = millis();
            if (nowMs - lastFailureLogMs > 10000) {
                LOG_F("RS485 FAILED: ID %d, Reg 0x%04X. Error: %s (0x%02X)\n",
                      request->slaveId, request->startReg, getErrorString(result), result);
                lastFailureLogMs = nowMs;
            }
        } else {
        if (EoloDebug::verboseLogsEnabled()) {
            LOG_F("RS485 OK: ID %d, Reg 0x%04X, Count %d\n", 
                  request->slaveId, request->startReg, request->count);
        }
        }
        if (success) {
            _consecutiveFailures = 0;
        }

        outSuccess = success;
        outErrorCode = result;

        // Notificar a la tarea que llamó con el resultado empaquetado
        // Usamos el bit 31 para success y los bits 0-7 para el errorCode
        if (request->callerTask != nullptr) {
            uint32_t notificationValue = packNotificationValue(success, result);
            xTaskNotify(request->callerTask, notificationValue, eSetValueWithOverwrite);
        }
    }

    public:
    static RS485Bus& getInstance() {
        static RS485Bus instance;
        return instance;
    }

    void begin() {
        if (xSemaphoreTake(_initMutex, pdMS_TO_TICKS(1000)) == pdFALSE) {
            return; // Ya en progreso
        }

        if (_initialized) {
            xSemaphoreGive(_initMutex);
            return;
        }

        // Inicializar cola de solicitudes
        if (_requestQueue == nullptr) {
            _requestQueue = xQueueCreate(QUEUE_LENGTH, sizeof(RS485Request));
        }

        pinMode(RS485_DE_RE_PIN, OUTPUT);
        digitalWrite(RS485_DE_RE_PIN, LOW);

        _activeBus = this;
        _serial.begin(RS485_BAUD_RATE, SWSERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
        drainRx(20, 100);

        _node.preTransmission(_preTransmission);
        _node.postTransmission(_postTransmission);
        _node.idle(_modbusIdle);

        // Crear task del bus
        if (_busTaskHandle == nullptr) {
            xTaskCreatePinnedToCore(
                _busTask,
                "RS485BusTask",
                BUS_TASK_STACK_SIZE,
                this,
                BUS_TASK_PRIORITY,
                &_busTaskHandle,
                BUS_TASK_CORE
            );
        }

        _initialized = true;
        xSemaphoreGive(_initMutex);
    }

    // API de lectura solicitando notificación al terminar
    bool readRegistersAsync(uint8_t slaveId, uint16_t startReg, uint8_t count,
                           uint16_t* dataBuffer, uint32_t timeoutMs = READ_TIMEOUT_MS) {
        if (!_initialized || !_requestQueue) {
            LOG_F("RS485: lectura rechazada sin bus inicializado (ID %d).\n", slaveId);
            return false;
        }

        if (dataBuffer == nullptr) {
            LOG_F("RS485: lectura rechazada por buffer nulo (ID %d, Reg 0x%04X).\n", slaveId, startReg);
            return false;
        }

        if (count == 0 || count > MAX_READ_REGISTERS) {
            LOG_F(
                "RS485: lectura rechazada por cantidad invalida (ID %d, Count %d, Max %d).\n",
                slaveId,
                count,
                MAX_READ_REGISTERS
            );
            return false;
        }

        RS485PatternValidator::getInstance().validateSyncReadContext();
        if (xPortGetCoreID() == UI_LOOP_CORE) {
            LOG_F("RS485: lectura bloqueante rechazada desde core UI (ID %d).\n", slaveId);
            return false;
        }

        RS485Request request = {
            .slaveId = slaveId,
            .startReg = startReg,
            .count = count,
            .dataBuffer = dataBuffer,
            .callerTask = xTaskGetCurrentTaskHandle() // Tarea actual para recibir notificación
        };

        // Limpiar cualquier notificación previa en esta tarea
        clearCurrentTaskNotification();

        // Enviar solicitud a la cola
        if (xQueueSend(_requestQueue, &request, pdMS_TO_TICKS(REQUEST_QUEUE_SEND_TIMEOUT_MS)) != pdTRUE) {
            LOG_LN("RS485: Cola de solicitudes llena");
            RS485Monitor::getInstance().recordQueueOverflow();
            return false;
        }

        (void)timeoutMs;

        // Esperar a que la tarea del bus procese la solicitud aceptada. No se retorna
        // por timeout externo para evitar dejar punteros del caller vivos en la cola.
        uint32_t notificationValue = 0;
        if (xTaskNotifyWait(0, ULONG_MAX, &notificationValue, portMAX_DELAY) != pdTRUE) {
            LOG_F("RS485: espera de notificación interrumpida (ID %d).\n", slaveId);
            return false;
        }

        // Desempaquetar éxito (bit 31)
        bool success = (notificationValue & 0x80000000) != 0;
        return success;
    }

    // API sincrónica para mantener compatibilidad
    bool readRegisters(uint8_t slaveId, uint16_t startReg, uint8_t count, uint16_t* dataBuffer) {
        return readRegistersAsync(slaveId, startReg, count, dataBuffer, READ_TIMEOUT_MS);
    }

    // Obtener la cantidad de solicitudes pendientes en la cola
    uint32_t getPendingRequests() {
        if (!_requestQueue) return 0;
        return uxQueueMessagesWaiting(_requestQueue);
    }
};

inline RS485Bus* RS485Bus::_activeBus = nullptr;

#endif
