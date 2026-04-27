#ifndef RS485_PATTERN_VALIDATOR_HPP
#define RS485_PATTERN_VALIDATOR_HPP

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Validador de patrones de uso correcto
class RS485PatternValidator {
private:
    TaskHandle_t _lastTaskHandle = nullptr;
    unsigned long _lastSyncReadTime = 0;
    uint32_t _syncReadCount = 0;
    bool _syncReadFromCorrectCore = true;
    SemaphoreHandle_t _validateMutex = nullptr;
    
    // Detectar si estamos en Core 0 (loop) o Core 1 (tasks)
    bool isCore0() {
        return xPortGetCoreID() == 0;
    }

    RS485PatternValidator() {
        _validateMutex = xSemaphoreCreateMutex();
    }

public:
    static RS485PatternValidator& getInstance() {
        static RS485PatternValidator instance;
        return instance;
    }

    // Validar que RS485::readRegisters() no se llama desde Core 0
    void validateSyncReadContext() {
        if (xSemaphoreTake(_validateMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return;
        }

        if (isCore0()) {
            _syncReadFromCorrectCore = false;
            LOG_LN("VIOLATION: RS485 sync read called from Core 0 (Loop). This will block the UI!");
            LOG_LN("Use flowSensor.getData() or anemometer.getData() instead.");
        } else {
            _syncReadFromCorrectCore = true;
        }
        
        _syncReadCount++;
        _lastSyncReadTime = millis();

        xSemaphoreGive(_validateMutex);
    }

    // Validar que getData tiene timeout razonable
    void validateGetDataTimeout(uint32_t timeoutMs, const char* sensorName) {
        if (timeoutMs > 50) {
            LOG_F("WARNING: %s.getData() called with timeout %ums (recommended <= 50ms)\n", 
                  sensorName, timeoutMs);
        }
    }

    // Validar que no hay reads sincronicos desde Core 0 recientemente
    bool checkSyncReadViolation() {
        if (xSemaphoreTake(_validateMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return false;
        }

        bool violation = false;
        // Si hay un sync read en los ultimos 100ms y fue desde Core 0, es violation
        if (!_syncReadFromCorrectCore && (millis() - _lastSyncReadTime < 100)) {
            violation = true;
        }

        xSemaphoreGive(_validateMutex);
        return violation;
    }

    // Reportar violaciones detectadas
    void reportViolations() {
        if (checkSyncReadViolation()) {
            LOG_LN("PATTERN VIOLATION: Sincronous RS485 read detected on Core 0 (main loop)");
            LOG_LN("This will cause frame drops and UI freezing.");
        }
    }
};

#endif
