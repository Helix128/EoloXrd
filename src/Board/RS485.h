#ifndef RS485_MANAGER_HPP
#define RS485_MANAGER_HPP

#include <Arduino.h>
#include <ModbusMaster.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "./Config.h"
#include "../Utility/RS485Monitor.h"

// Forward declaration para monitor (evitar dependencia circular)
class RS485Monitor;
extern RS485Monitor* g_rs485Monitor;

#define RS485_RX_PIN 35
#define RS485_TX_PIN 33
#define RS485_DE_RE_PIN 26
#define RS485_BAUD_RATE 4800
#define RS485_BUS_GUARD_TIME_MS 20     // Tiempo de silencio antes de cada transmisión
#define RS485_INTER_FRAME_TIME_US 250  // Tiempo entre fin de transmisión y bajada DE/RE

#define DEBUG true

// Estructura de solicitud al bus RS485
struct RS485Request {
    uint8_t slaveId;
    uint16_t startReg;
    uint8_t count;
    uint16_t* dataBuffer;
    TaskHandle_t callerTask;  // Tarea a notificar al terminar
};

class RS485 {
private:
    HardwareSerial& _serial;
    ModbusMaster _node;
    SemaphoreHandle_t _initMutex;
    bool _initialized;

    TaskHandle_t _managerTaskHandle = nullptr;
    QueueHandle_t _requestQueue = nullptr;

    static const int QUEUE_LENGTH = 8;
    static const int MANAGER_STACK_SIZE = 4096;
    static const int MANAGER_PRIORITY = 2;

    RS485() : _serial(Serial1), _initialized(false) {
        _initMutex = xSemaphoreCreateMutex();
    }

    RS485(const RS485&) = delete;
    RS485& operator=(const RS485&) = delete;

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
        Serial1.flush();
        // A 4800 baudios, un bit dura ~208us. 
        // Damos tiempo a que el bit de parada salga antes de bajar el pin DE/RE.
        delayMicroseconds(RS485_INTER_FRAME_TIME_US); 
        digitalWrite(RS485_DE_RE_PIN, LOW);
    }

    // Task estática que procesa la cola de solicitudes
    static void _busManagerTask(void* arg) {
        RS485* self = (RS485*)arg;
        RS485Request request;

        while (true) {
            // Esperar solicitud con timeout para permitir que la tarea sea eliminada
            if (xQueueReceive(self->_requestQueue, &request, pdMS_TO_TICKS(100)) == pdTRUE) {
                bool success = false;
                uint8_t errorCode = 0;

                self->_processRequest(&request, success, errorCode);

                // Registrar estadisticas si el monitor esta disponible
                if (g_rs485Monitor != nullptr) {
                    g_rs485Monitor->recordRequestCompleted(success, errorCode);
                    g_rs485Monitor->recordQueueDepth(uxQueueMessagesWaiting(self->_requestQueue));
                }
            }
        }
    }

    // Procesar una solicitud individual
    void _processRequest(RS485Request* request, bool& outSuccess, uint8_t& outErrorCode) {
        if (!_initialized) {
            outSuccess = false;
            outErrorCode = ModbusMaster::ku8MBInvalidSlaveID;
            if (request->callerTask) xTaskNotify(request->callerTask, outErrorCode, eSetValueWithOverwrite);
            return;
        }

        bool success = false;
        uint8_t result = ModbusMaster::ku8MBResponseTimedOut;
        int retries = 2; // Total 3 intentos (1 original + 2 reintentos)

        while (retries >= 0) {
            // Tiempo de guarda: Silencio en el bus para que los esclavos se estabilicen
            vTaskDelay(pdMS_TO_TICKS(RS485_BUS_GUARD_TIME_MS)); 

            // Limpiar buffer de entrada
            while(_serial.available()) {
                _serial.read();
            }

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
                if (result == ModbusMaster::ku8MBResponseTimedOut) {
                    vTaskDelay(pdMS_TO_TICKS(150)); // Mayor espera para que el sensor se recupere
                } else {
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }

        if (!success) {
            LOG_F("RS485 FAILED: ID %d, Reg 0x%04X. Error: %s (0x%02X)\n", 
                  request->slaveId, request->startReg, getErrorString(result), result);
        } else {
            LOG_F("RS485 OK: ID %d, Reg 0x%04X, Count %d\n", 
                  request->slaveId, request->startReg, request->count);
        }

        outSuccess = success;
        outErrorCode = result;

        // Notificar a la tarea que llamó con el resultado empaquetado
        // Usamos el bit 31 para success y los bits 0-7 para el errorCode
        if (request->callerTask != nullptr) {
            uint32_t notificationValue = (success ? 0x80000000 : 0) | (uint32_t)result;
            xTaskNotify(request->callerTask, notificationValue, eSetValueWithOverwrite);
        }
    }

    public:
    static RS485& getInstance() {
        static RS485 instance;
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

        _serial.begin(RS485_BAUD_RATE, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

        _node.preTransmission(_preTransmission);
        _node.postTransmission(_postTransmission);

        // Crear task gestor del bus
        if (_managerTaskHandle == nullptr) {
            xTaskCreatePinnedToCore(
                _busManagerTask,
                "RS485Manager",
                MANAGER_STACK_SIZE,
                this,
                MANAGER_PRIORITY,
                &_managerTaskHandle,
                1  // Core 1 (deja Core 0 para otras tareas críticas)
            );
        }

        _initialized = true;
        xSemaphoreGive(_initMutex);
    }

    // API de lectura solicitando notificación al terminar
    bool readRegistersAsync(uint8_t slaveId, uint16_t startReg, uint8_t count, 
                           uint16_t* dataBuffer, uint32_t timeoutMs = 5000) {        if (!_initialized || !_requestQueue) {
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
        ulTaskNotifyTake(pdTRUE, 0);

        // Enviar solicitud a la cola
        if (xQueueSend(_requestQueue, &request, pdMS_TO_TICKS(100)) != pdTRUE) {
            LOG_LN("RS485: Cola de solicitudes llena");
            return false;
        }

        // Esperar notificación de la tarea gestora (se suspende la tarea eficientemente)
        uint32_t notificationValue = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeoutMs));

        if (notificationValue == 0) {
            LOG_F("RS485: Timeout esperando notificación (ID %d). Bus saturado o bloqueado.\n", slaveId);
            return false;
        }

        // Desempaquetar éxito (bit 31)
        bool success = (notificationValue & 0x80000000) != 0;
        return success;
    }

    // API sincrónica para mantener compatibilidad
    bool readRegisters(uint8_t slaveId, uint16_t startReg, uint8_t count, uint16_t* dataBuffer) {
        return readRegistersAsync(slaveId, startReg, count, dataBuffer, 3000);
    }

    // Obtener la cantidad de solicitudes pendientes en la cola
    uint32_t getPendingRequests() {
        if (!_requestQueue) return 0;
        return uxQueueMessagesWaiting(_requestQueue);
    }
};

#endif
