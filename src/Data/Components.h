#ifndef EOLO_DATA_COMPONENTS_H
#define EOLO_DATA_COMPONENTS_H

#include "../Board/Input.h"
#include "../Effectors/Motor.h"
#include "../Effectors/StatusLed.h"
#include "../Board/RTCManager.h"
#include "../Board/I2CBus.h"
#include "../Sensors/AFM07.h"
#include "../Sensors/FS3000.h"
#include "../Sensors/Plantower.h"
#include "../Sensors/BME280.h"
#include "../Sensors/NTC.h"
#include "../Board/Battery.h"
#ifdef FEATURE_MODEM
#include "../Board/Modem.h"
#include "../Board/ModemService.h"
#include "../Utility/SensorAPI.h"
#endif
#include "../Sensors/Anemometer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Profiler.h"

struct Components {
#ifndef FEATURE_HEADLESS
  Input input;        // Manejo entradas (encoder/botón)
#endif
  MotorManager motor;
  StatusLed statusLed;

#ifdef FEATURE_ANEMOMETER
  Anemometer anemometer;
#endif

#ifdef FEATURE_MODEM
  Modem modem;
  ModemService modemService = ModemService(modem);
  SensorAPI api = SensorAPI(&modemService, 20);
#endif

#ifdef FEATURE_FLOW_AFM07
  AFM07 flowSensor;
#elif defined(FEATURE_FLOW_FS3000)
  FS3K flowSensor;
#endif

#ifdef FEATURE_PLANTOWER
  Plantower plantower;
#endif
  BME280 bme;
#ifdef FEATURE_NTC
  NTC ntc;
#endif
  Battery battery;
  RTCManager rtc;

private:
  TaskHandle_t _i2cTaskHandle = nullptr;

  static uint32_t retryDelayMs(uint8_t failures, uint32_t normalInterval) {
    (void)normalInterval;
    static constexpr uint32_t delays[] = {250UL, 500UL, 1000UL, 2000UL, 5000UL};
    uint8_t index = failures == 0 ? 0 : (uint8_t)(failures - 1);
    if (index >= 5) index = 4;
    return delays[index];
  }

  static void reportHealth(const char *name, bool success, uint8_t failures,
                           bool &degraded, uint32_t &lastLogMs) {
    uint32_t now = millis();
    if (!success) {
      if (!degraded) {
        LOG_F("I2C %s degradado; reintento con backoff\n", name);
        degraded = true;
        lastLogMs = now;
      } else if (now - lastLogMs >= 10000UL) {
        LOG_F("I2C %s sigue degradado (fallos=%u)\n", name, failures);
        lastLogMs = now;
      }
    } else if (degraded) {
      LOG_F("I2C %s recuperado\n", name);
      degraded = false;
      lastLogMs = now;
    }
  }

  void i2cWorker() {
    I2CBus &bus = I2CBus::getInstance();
    bus.begin();

    uint32_t warmupLogMs = 0;
    while (!bus.warmupComplete()) {
      uint32_t now = millis();
      if (now - warmupLogMs >= 1000UL) {
        LOG_F("I2C en warmup; faltan %lu ms\n",
              (unsigned long)bus.warmupRemainingMs());
        warmupLogMs = now;
      }
      vTaskDelay(pdMS_TO_TICKS(50));
    }
    LOG_LN("Warmup I2C completo; iniciando sondeo de periféricos");

    uint32_t now = millis();
    uint32_t nextInput = now;
    uint32_t nextFlow = now + 2UL;
    uint32_t nextBme = now + 350UL;
    uint32_t nextRtc = now + 100UL;
    uint32_t nextBattery = now + 650UL;
    uint32_t nextInputInit = now;
    uint32_t nextFlowInit = now;
    uint32_t nextBmeInit = now;
    uint32_t nextRtcInit = now;

    uint8_t inputFailures = 0;
    uint8_t flowFailures = 0;
    uint8_t bmeFailures = 0;
    uint8_t rtcFailures = 0;
    uint8_t batteryFailures = 0;
    bool inputDegraded = false;
    bool flowDegraded = false;
    bool bmeDegraded = false;
    bool rtcDegraded = false;
    bool batteryDegraded = false;
    uint32_t inputLastLog = 0;
    uint32_t flowLastLog = 0;
    uint32_t bmeLastLog = 0;
    uint32_t rtcLastLog = 0;
    uint32_t batteryLastLog = 0;
    uint32_t lastRecoveryMs = 0;
    uint32_t seenRecoveries = bus.getStats().recoveries;

#ifndef FEATURE_HEADLESS
    bool inputInitialized = false;
#endif
#ifdef FEATURE_FLOW_FS3000
    bool flowInitialized = false;
#endif
    bool bmeInitialized = false;
    bool rtcInitialized = false;

    auto maybeRecoverBus = [&]() {
      I2CBus::Stats stats = bus.getStats();
      bool transportFailure = stats.lastResult == I2CBus::Result::Timeout ||
                              stats.lastResult == I2CBus::Result::ShortRead ||
                              stats.lastResult == I2CBus::Result::Nack ||
                              stats.lastResult == I2CBus::Result::BusError;
      bool simultaneousFailures = bus.recentFailureCount(1000UL) >= 2;
      uint32_t current = millis();
      if ((transportFailure && bus.linesStuck()) || simultaneousFailures) {
        if (current - lastRecoveryMs < 5000UL)
          return;
        if (bus.recoverBus()) {
          lastRecoveryMs = current;
          seenRecoveries++;
          LOG_LN("I2C bus recuperado; se revalidarán los sensores");
          bme.isReady = false;
          bmeInitialized = false;
          rtcInitialized = false;
#ifdef FEATURE_FLOW_FS3000
          flowSensor.isReady = false;
          flowInitialized = false;
#endif
        }
      }
    };

    while (true) {
      now = millis();

      // Los diagnósticos solicitados desde la consola se ejecutan aquí,
      // junto con el resto de operaciones del bus. Nunca se hace scan/reset
      // sincrónico desde el loop de UI.
      bus.processQueuedCommands();

#ifndef FEATURE_HEADLESS
      if (!inputInitialized && (int32_t)(now - nextInputInit) >= 0) {
        input.begin();
        inputInitialized = true;
        nextInput = now + 8UL;
      }
      if (inputInitialized && (int32_t)(now - nextInput) >= 0) {
        bool success = input.pollHardware();
        if (success) inputFailures = 0;
        else if (inputFailures < 255) ++inputFailures;
        reportHealth("encoder", success, inputFailures, inputDegraded, inputLastLog);
        nextInput = now + (success ? 8UL : retryDelayMs(inputFailures, 8UL));
        maybeRecoverBus();
      }
#endif

#ifdef FEATURE_FLOW_FS3000
      if (!flowInitialized && (int32_t)(now - nextFlowInit) >= 0) {
        flowInitialized = flowSensor.begin();
        if (!flowInitialized && flowFailures < 255) ++flowFailures;
        if (flowInitialized) flowFailures = 0;
        reportHealth("FS3000", flowInitialized, flowFailures, flowDegraded, flowLastLog);
        nextFlow = now + (flowInitialized ? 10UL : retryDelayMs(flowFailures, 10UL));
        nextFlowInit = nextFlow;
        maybeRecoverBus();
      } else if (flowInitialized && (int32_t)(now - nextFlow) >= 0) {
        bool success = flowSensor.poll();
        if (success) flowFailures = 0;
        else if (flowFailures < 255) ++flowFailures;
        if (!success && !flowSensor.isReady) flowInitialized = false;
        reportHealth("FS3000", success, flowFailures, flowDegraded, flowLastLog);
        nextFlow = now + (success ? 10UL : retryDelayMs(flowFailures, 10UL));
        nextFlowInit = nextFlow;
        maybeRecoverBus();
      }
#endif

      if (!bmeInitialized && (int32_t)(now - nextBmeInit) >= 0) {
        bmeInitialized = bme.begin();
        if (bmeInitialized) bmeFailures = 0;
        else if (bmeFailures < 255) ++bmeFailures;
        reportHealth("BME280", bmeInitialized, bmeFailures, bmeDegraded, bmeLastLog);
        nextBme = now + (bmeInitialized ? 1000UL : retryDelayMs(bmeFailures, 1000UL));
        nextBmeInit = nextBme;
        maybeRecoverBus();
      } else if (bmeInitialized && (int32_t)(now - nextBme) >= 0) {
        bool success = bme.readData();
        if (success) bmeFailures = 0;
        else if (bmeFailures < 255) ++bmeFailures;
        if (!success && !bme.isReady) bmeInitialized = false;
        reportHealth("BME280", success, bmeFailures, bmeDegraded, bmeLastLog);
        nextBme = now + (success ? 1000UL : retryDelayMs(bmeFailures, 1000UL));
        nextBmeInit = nextBme;
        maybeRecoverBus();
      }

      if (!rtcInitialized && (int32_t)(now - nextRtcInit) >= 0) {
        rtcInitialized = rtc.begin();
        if (rtcInitialized) rtcFailures = 0;
        else if (rtcFailures < 255) ++rtcFailures;
        reportHealth("RTC", rtcInitialized, rtcFailures, rtcDegraded, rtcLastLog);
        nextRtc = now + (rtcInitialized ? 1000UL : retryDelayMs(rtcFailures, 1000UL));
        nextRtcInit = nextRtc;
        maybeRecoverBus();
      } else if (rtcInitialized && (int32_t)(now - nextRtc) >= 0) {
        bool success = rtc.poll();
        if (success) rtcFailures = 0;
        else if (rtcFailures < 255) ++rtcFailures;
        if (!success && !rtc.ok) rtcInitialized = false;
        reportHealth("RTC", success, rtcFailures, rtcDegraded, rtcLastLog);
        nextRtc = now + (success ? 1000UL : retryDelayMs(rtcFailures, 1000UL));
        nextRtcInit = nextRtc;
        maybeRecoverBus();
      }

#ifdef FEATURE_DUAL_BATTERY
      if ((int32_t)(now - nextBattery) >= 0) {
        bool success = battery.pollFromI2C();
        if (success) batteryFailures = 0;
        else if (batteryFailures < 255) ++batteryFailures;
        reportHealth("batería", success, batteryFailures, batteryDegraded, batteryLastLog);
        nextBattery = now + (success ? 1000UL : retryDelayMs(batteryFailures, 1000UL));
        maybeRecoverBus();
      }
#endif

      I2CBus::Stats stats = bus.getStats();
      if (stats.recoveries != seenRecoveries) {
        seenRecoveries = stats.recoveries;
        nextBmeInit = nextRtcInit = millis();
#ifdef FEATURE_FLOW_FS3000
        nextFlowInit = millis();
#endif
      }
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }

  static void i2cWorkerEntry(void *arg) {
    static_cast<Components *>(arg)->i2cWorker();
    vTaskDelete(nullptr);
  }

public:
  void begin() {
    LOG_LN("Iniciando componentes de hardware...");

    statusLed.begin();
    statusLed.setPattern(StatusLedPattern::Boot);

#ifdef FEATURE_FLOW_AFM07
    flowSensor.begin();
#endif
#ifdef FEATURE_NTC
    ntc.begin();
#endif
#ifdef FEATURE_PLANTOWER
    plantower.begin();
#endif
#ifdef FEATURE_ANEMOMETER
    anemometer.begin();
#endif

    // Solo la parte ADC/local se inicializa aquí. Las consultas I2C se
    // ejecutan exclusivamente en el worker fijado al core 0.
    battery.begin();
    int batteryLevel = battery.getRawLevel();
    float batteryPct = battery.getPct();
    LOG_OUT("Nivel de batería: ");
    LOG_OUT(batteryLevel);
    LOG_OUT(" (");
    LOG_OUT(batteryPct);
    LOG_OUT_LN("%)");

    if (_i2cTaskHandle == nullptr) {
      BaseType_t created = xTaskCreatePinnedToCore(
          i2cWorkerEntry, "EoloI2CTask", 4096, this, 2,
          &_i2cTaskHandle, 0);
      if (created != pdPASS) {
        _i2cTaskHandle = nullptr;
        LOG_LN("No se pudo crear EoloI2CTask; I2C queda deshabilitado");
      }
    }

    LOG_LN("Inicialización de componentes completa; sondeo I2C asíncrono activo");
  }

  void poll() {
    motor.updateRamp();
    statusLed.poll();
  }
};

#endif
