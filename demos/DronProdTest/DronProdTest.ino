/*
 * Prueba maestra de calificación de producción EOLO Dron.
 *
 * Este sketch es únicamente una raíz de composición alternativa. Los
 * protocolos, sensores, PID, protección térmica y CSV productivo permanecen
 * en Context y sus componentes de producción.
 */

#define EOLO_I2C_DIRECT_DRIVERS 1 
#include "../EoloDemoPinout.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <esp_system.h>
#include <math.h>

#include "../../src/Data/Context.h"
#include "../../src/Utility/SystemDiagnostics.h"
#include "../../src/Utility/SerialOutput.h"

namespace {

constexpr uint32_t kPreflightTimeoutMs = 30000UL;
constexpr uint32_t kStabilizationMs = 30000UL;
constexpr uint32_t kSoakMs = 120000UL;
constexpr uint32_t kCaptureSeconds = 10UL * 60UL;
constexpr uint32_t kHeartbeatTimeoutMs = 5000UL;
constexpr uint32_t kFlushTimeoutMs = 30000UL;
constexpr uint32_t kSampleMs = 1000UL;
constexpr float kTargetFlowLpm = 5.0f;
constexpr float kBandLowLpm = 4.5f;
constexpr float kBandHighLpm = 5.5f;

enum class QaState : uint8_t {
  Boot,
  Idle,
  Preflight,
  Stabilizing,
  Soak,
  WaitStart,
  Capture,
  Flushing,
  Ready,
  NotReady,
};

const char *stateName(QaState state) {
  switch (state) {
    case QaState::Boot: return "BOOT";
    case QaState::Idle: return "IDLE";
    case QaState::Preflight: return "PREFLIGHT";
    case QaState::Stabilizing: return "STABILIZING";
    case QaState::Soak: return "SOAK";
    case QaState::WaitStart: return "WAIT_START_LONG";
    case QaState::Capture: return "CAPTURE";
    case QaState::Flushing: return "FLUSHING";
    case QaState::Ready: return "READY_FOR_LONG_MEASUREMENTS";
    case QaState::NotReady: return "NOT_READY";
  }
  return "UNKNOWN";
}

Context context;
QaState state = QaState::Boot;
uint32_t stateStartedMs = 0;
uint32_t lastSampleMs = 0;
uint32_t lastHeartbeatMs = 0;
uint32_t captureStartedMs = 0;
uint32_t captureStartUnix = 0;
uint32_t previousRtcUnix = 0;
uint32_t captureSamples = 0;
uint32_t samplesAfterBand = 0;
uint32_t samplesInBand = 0;
uint32_t consecutiveOutsideBand = 0;
uint32_t maxConsecutiveOutsideBand = 0;
bool reachedBand = false;
uint32_t reachedBandMs = 0;
double integratedVolumeL = 0.0;
uint32_t lastIntegratedMs = 0;
float lastIntegratedFlowLpm = 0.0f;
bool hasIntegratedFlow = false;
I2CBus::Stats i2cBaseline;
bool diagnosticRequested = false;
uint32_t preflightFlowSampleId = 0;
bool resultEmitted = false;
String captureBasename;
uint32_t finalCsvRows = 0;
bool finalCsvFinalized = false;
bool finalMasterIndexUpdated = false;
bool finalCsvReopenable = false;
bool finalCsvCadenceOk = false;
char commandBuffer[48] = {};
size_t commandLength = 0;

template <size_t Capacity>
void emitJson(const char *prefix, StaticJsonDocument<Capacity> &doc) {
  Serial.print(prefix);
  Serial.print(' ');
  serializeJson(doc, Serial);
  Serial.println();
}

void addIdentity(JsonObject root) {
  const uint64_t chip = ESP.getEfuseMac();
  char mac[18];
  snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
           static_cast<unsigned>((chip >> 40) & 0xFF),
           static_cast<unsigned>((chip >> 32) & 0xFF),
           static_cast<unsigned>((chip >> 24) & 0xFF),
           static_cast<unsigned>((chip >> 16) & 0xFF),
           static_cast<unsigned>((chip >> 8) & 0xFF),
           static_cast<unsigned>(chip & 0xFF));
  root["mac"] = mac;
  root["pollGapMs"] = EoloCore::RS485TimingModel::kAfmPollGapMs;
  root["i2cDirectDrivers"] = EOLO_I2C_DIRECT_DRIVERS == 1;
  root["metrologicalAccuracyValidated"] = false;
  root["resetReason"] = SystemDiagnostics::resetReasonName(esp_reset_reason());
}

uint32_t exceptionCount(const RS485SlaveStats &stats) {
  uint32_t total = stats.otherExceptionErrors;
  for (uint8_t code = 1; code <= 11; ++code)
    total += stats.modbusExceptions[code];
  return total;
}

bool rs485Clean(const RS485SlaveStats &stats) {
  RS485Bus &bus = RS485Bus::getInstance();
  return stats.failures == 0 && stats.timeoutErrors == 0 && stats.crcErrors == 0 &&
         stats.incompleteFrames == 0 && stats.malformedFrames == 0 &&
         stats.busBusyErrors == 0 && stats.unexpectedFrameErrors == 0 &&
         stats.deadlineMisses == 0 && stats.lateBytes == 0 &&
         stats.unexpectedFrames == 0 && exceptionCount(stats) == 0 &&
         bus.busBusyDeferrals() == 0 && bus.lateBytes() == 0 &&
         bus.unexpectedFrames() == 0 && bus.transportRecoveries() == 0;
}

bool i2cCleanSinceBaseline() {
  const I2CBus::Stats current = I2CBus::getInstance().getStats();
  return current.failures == i2cBaseline.failures &&
         current.nacks == i2cBaseline.nacks &&
         current.timeouts == i2cBaseline.timeouts &&
         current.shortReads == i2cBaseline.shortReads &&
         current.recoveries == i2cBaseline.recoveries;
}

bool motorsExactlyZero() {
  for (int i = 0; i < MotorManager::motorCount; ++i)
    if (context.components.motor.getMotorPwm(i) != 0 ||
        context.components.motor.getMotorTargetPwm(i) != 0)
      return false;
  return true;
}

struct SensorSnapshot {
  FlowData flow;
  BME280Data bme;
  NTCData ntc;
  bool flowValid = false;
  bool bmeValid = false;
  bool ntcValid = false;
  bool rtcValid = false;
  uint32_t rtcUnix = 0;
};

SensorSnapshot readSensors() {
  SensorSnapshot value;
  value.flowValid = context.components.flowSensor.getData(value.flow) &&
                    value.flow.valid && value.flow.fresh && !value.flow.stale;
  value.bmeValid = context.components.bme.getData(value.bme) && value.bme.valid;
  value.ntcValid = context.components.ntc.getData(value.ntc) && value.ntc.valid &&
                   isfinite(value.ntc.temperature);
  const DateTime now = context.components.rtc.now();
  value.rtcValid = context.components.rtc.isPresent() && context.components.rtc.isValid(now);
  value.rtcUnix = value.rtcValid ? now.unixtime() : 0;
  return value;
}

void emitEvent(const char *event, const char *reason = nullptr) {
  StaticJsonDocument<768> doc;
  JsonObject root = doc.to<JsonObject>();
  root["event"] = event;
  root["state"] = stateName(state);
  root["uptimeMs"] = millis();
  if (reason != nullptr)
    root["reason"] = reason;
  addIdentity(root);
  emitJson("EOLO_QA_EVENT", doc);
}

void emitResult(const char *result, const char *reason) {
  if (resultEmitted)
    return;
  resultEmitted = true;
  StaticJsonDocument<1024> doc;
  JsonObject root = doc.to<JsonObject>();
  root["result"] = result;
  root["state"] = stateName(state);
  root["reason"] = reason == nullptr ? "" : reason;
  root["captureSeconds"] = kCaptureSeconds;
  root["reachedBandMs"] = reachedBand ? reachedBandMs : 0;
  root["samplesAfterBand"] = samplesAfterBand;
  root["samplesInBand"] = samplesInBand;
  root["inBandRatio"] = samplesAfterBand == 0 ? 0.0 :
      static_cast<double>(samplesInBand) / static_cast<double>(samplesAfterBand);
  root["maxConsecutiveOutsideBand"] = maxConsecutiveOutsideBand;
  root["integratedVolumeL"] = integratedVolumeL;
  root["productVolumeL"] = context.session.capturedVolume;
  root["volumeDifferenceL"] = fabs(integratedVolumeL - context.session.capturedVolume);
  root["captureFile"] = captureBasename;
  root["csvRows"] = finalCsvRows;
  root["csvFinalized"] = finalCsvFinalized;
  root["masterIndexUpdated"] = finalMasterIndexUpdated;
  root["csvReopenable"] = finalCsvReopenable;
  root["csvCadenceOk"] = finalCsvCadenceOk;
  root["pwmFinalZero"] = motorsExactlyZero();
  addIdentity(root);
  emitJson("EOLO_QA_RESULT", doc);
}

void latchFailure(const char *reason) {
  if (state == QaState::NotReady || state == QaState::Ready)
    return;
  context.components.motor.setPwmImmediate(0);
  context.resetMotorFlowController();
  if (context.isCaptureActive())
    context.capture.abort(context, reason);
  state = QaState::NotReady;
  stateStartedMs = millis();
  emitEvent("NOT_READY", reason);
  emitResult("NOT_READY", reason);
}

void emitSample(const SensorSnapshot &s) {
  const RS485SlaveStats afm = RS485Bus::getInstance().getSlaveStats(
      EoloCore::ModbusRtuProtocol::Afm07SlaveId);
  const I2CBus::Stats i2c = I2CBus::getInstance().getStats();
  StaticJsonDocument<2048> doc;
  JsonObject root = doc.to<JsonObject>();
  root["state"] = stateName(state);
  root["uptimeMs"] = millis();
  root["elapsedMs"] = millis() - stateStartedMs;
  root["rtcUnix"] = s.rtcUnix;
  root["rtcValid"] = s.rtcValid;
  root["rtcMonotonic"] = previousRtcUnix == 0 || s.rtcUnix >= previousRtcUnix;
  root["flow"] = s.flow.flow;
  root["flowValid"] = s.flowValid;
  root["flowFresh"] = s.flow.fresh;
  root["flowAgeMs"] = s.flow.ageMs;
  root["flowSampleId"] = s.flow.sampleId;
  root["bmeValid"] = s.bmeValid;
  root["ntcValid"] = s.ntcValid;
  root["ntcC"] = s.ntcValid ? s.ntc.temperature : -99.0f;
  JsonArray pwm = root.createNestedArray("pwm");
  for (int i = 0; i < MotorManager::motorCount; ++i)
    pwm.add(context.components.motor.getMotorPwm(i));
  root["capturedVolumeL"] = context.session.capturedVolume;
  root["integratedVolumeL"] = integratedVolumeL;
  JsonObject rs = root.createNestedObject("rs485");
  rs["successes"] = afm.successes;
  rs["failures"] = afm.failures;
  rs["timeouts"] = afm.timeoutErrors;
  rs["crc"] = afm.crcErrors;
  rs["incomplete"] = afm.incompleteFrames;
  rs["malformed"] = afm.malformedFrames;
  rs["busBusy"] = afm.busBusyErrors + RS485Bus::getInstance().busBusyDeferrals();
  rs["lateBytes"] = afm.lateBytes + RS485Bus::getInstance().lateBytes();
  rs["unexpected"] = afm.unexpectedFrameErrors + afm.unexpectedFrames +
                     RS485Bus::getInstance().unexpectedFrames();
  rs["exceptions"] = exceptionCount(afm);
  rs["deadlineMisses"] = afm.deadlineMisses;
  rs["maxSuccessGapMs"] = afm.maxSuccessGapMs;
  rs["minRestMs"] = afm.minRestMs == UINT32_MAX ? 0 : afm.minRestMs;
  JsonObject i2cJson = root.createNestedObject("i2c");
  i2cJson["failures"] = i2c.failures - i2cBaseline.failures;
  i2cJson["nacks"] = i2c.nacks - i2cBaseline.nacks;
  i2cJson["timeouts"] = i2c.timeouts - i2cBaseline.timeouts;
  i2cJson["shortReads"] = i2c.shortReads - i2cBaseline.shortReads;
  i2cJson["recoveries"] = i2c.recoveries - i2cBaseline.recoveries;
  emitJson("EOLO_QA_SAMPLE", doc);
  if (s.rtcValid)
    previousRtcUnix = s.rtcUnix;
}

bool baseSensorsReady(const SensorSnapshot &s) {
  return context.isSdReady() && s.flowValid && s.bmeValid && s.ntcValid &&
         s.ntc.temperature < NTC_MOTOR_OVERHEAT_HIGH_C && s.rtcValid &&
         !RS485Bus::getInstance().isAfmSafetyBlocked();
}

bool rs485AcceptableForCapture(const RS485SlaveStats &stats) {
  if (stats.successes == 0 && stats.failures == 0)
    return true;
  return stats.state != RS485EndpointState::Offline &&
         stats.consecutiveFailures < 3 &&
         exceptionCount(stats) == 0;
}

bool runtimeGate(const SensorSnapshot &s, bool motorMustBeOff, const char *&reason) {
  if (!context.isSdReady()) { reason = "SD_INVALID"; return false; }
  if (!s.flowValid) { reason = "AFM07_INVALID_OR_STALE"; return false; }
  if (RS485Bus::getInstance().isAfmSafetyBlocked()) { reason = "AFM07_DIAGNOSTIC_LATCH"; return false; }
  if (!s.ntcValid) { reason = "NTC_INVALID"; return false; }
  if (s.ntc.temperature >= NTC_MOTOR_OVERHEAT_HIGH_C) { reason = "NTC_OVER_70C"; return false; }
  if (!s.bmeValid) { reason = "BME280_INVALID"; return false; }
  if (!s.rtcValid) { reason = "RTC_INVALID"; return false; }
  if (previousRtcUnix != 0 && s.rtcUnix < previousRtcUnix) { reason = "RTC_NOT_MONOTONIC"; return false; }
  const RS485SlaveStats afmStats = RS485Bus::getInstance().getSlaveStats(
      EoloCore::ModbusRtuProtocol::Afm07SlaveId);
  if (motorMustBeOff) {
    if (!rs485Clean(afmStats)) { reason = "RS485_FAILURE"; return false; }
    if (!i2cCleanSinceBaseline()) { reason = "I2C_FAILURE"; return false; }
    if (!motorsExactlyZero()) { reason = "MOTOR_NOT_ZERO"; return false; }
  } else {
    if (!rs485AcceptableForCapture(afmStats)) { reason = "RS485_FAILURE"; return false; }
    if (I2CBus::getInstance().getStats().failures > i2cBaseline.failures + 3) {
      reason = "I2C_FAILURE"; return false;
    }
  }
  return true;
}

String basenameForStart(uint32_t startUnix) {
  String stamp = DateTime(startUnix).timestamp();
  stamp.replace(":", "_");
  stamp.replace("-", "_");
  return String("log_") + stamp + ".csv";
}

bool parseRowUnix(const String &row, uint32_t &unixTime) {
  if (row.length() < 19)
    return false;
  int year, month, day, hour, minute, second;
  if (sscanf(row.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d",
             &year, &month, &day, &hour, &minute, &second) != 6)
    return false;
  DateTime parsed(year, month, day, hour, minute, second);
  if (!context.components.rtc.isValid(parsed))
    return false;
  unixTime = parsed.unixtime();
  return true;
}

bool verifyClosedCapture(uint32_t &rowCount, bool &finalized,
                         bool &indexed, bool &reopenable, bool &cadenceOk) {
  rowCount = 0;
  finalized = indexed = reopenable = cadenceOk = false;
  const String path = String(context.logsDirectory()) + "/" + captureBasename;
  SPIBus::Guard guard;
  File file = SD.open(path.c_str(), FILE_READ);
  if (!file)
    return false;
  const String header = file.readStringUntil('\n');
  if (header.indexOf("state") < 0) {
    file.close();
    return false;
  }
  String lastRow;
  uint32_t previous = captureStartUnix;
  cadenceOk = true;
  while (file.available()) {
    String row = file.readStringUntil('\n');
    row.trim();
    if (row.length() == 0)
      continue;
    ++rowCount;
    uint32_t rowUnix = 0;
    if (!parseRowUnix(row, rowUnix) || rowUnix < previous || rowUnix - previous < 9 ||
        rowUnix - previous > 11)
      cadenceOk = false;
    previous = rowUnix;
    lastRow = row;
  }
  file.close();
  finalized = lastRow.indexOf(",Finalizado,") >= 0;
  File reopened = SD.open(path.c_str(), FILE_READ);
  reopenable = static_cast<bool>(reopened);
  if (reopened)
    reopened.close();

  File index = SD.open(LogIndexService::CsvPath, FILE_READ);
  while (index && index.available()) {
    String line = index.readStringUntil('\n');
    if (line.startsWith(captureBasename + ",")) {
      indexed = true;
      break;
    }
  }
  if (index)
    index.close();
  return rowCount == kCaptureSeconds / CaptureController::CAPTURE_INTERVAL &&
         finalized && indexed && reopenable && cadenceOk;
}

void startQualification() {
  context.components.motor.setPwmImmediate(0);
  RS485Bus::getInstance().resetSlaveStats();
  diagnosticRequested = false;
  preflightFlowSampleId = 0;
  previousRtcUnix = 0;
  resultEmitted = false;
  state = QaState::Preflight;
  stateStartedMs = millis();
  lastSampleMs = 0;
  emitEvent("PREFLIGHT_STARTED");
}

void startLongCapture() {
  const SensorSnapshot sensors = readSensors();
  const char *failure = nullptr;
  RS485Bus::getInstance().resetSlaveStats();
  i2cBaseline = I2CBus::getInstance().getStats();
  previousRtcUnix = 0;
  if (!runtimeGate(sensors, true, failure)) {
    latchFailure(failure);
    return;
  }

  captureStartUnix = sensors.rtcUnix;
  captureBasename = basenameForStart(captureStartUnix);
  {
    SPIBus::Guard guard;
    const String path = String(context.logsDirectory()) + "/" + captureBasename;
    if (SD.exists(path.c_str())) {
      latchFailure("CAPTURE_FILE_ALREADY_EXISTS");
      return;
    }
  }
  context.session = Session();
  context.session.startUnix = captureStartUnix;
  context.session.duration = kCaptureSeconds;
  context.session.lastLog = captureStartUnix;
  context.session.targetFlow = kTargetFlowLpm;
  context.session.usePlantower = false;
  context.clearSession();
  context.saveSession();
  if (!context.beginCapture()) {
    latchFailure("CAPTURE_START_REJECTED");
    return;
  }

  state = QaState::Capture;
  stateStartedMs = captureStartedMs = millis();
  lastHeartbeatMs = millis();
  lastSampleMs = 0;
  captureSamples = samplesAfterBand = samplesInBand = 0;
  consecutiveOutsideBand = maxConsecutiveOutsideBand = 0;
  reachedBand = false;
  reachedBandMs = 0;
  integratedVolumeL = 0.0;
  lastIntegratedMs = captureStartedMs;
  lastIntegratedFlowLpm = 0.0f;
  hasIntegratedFlow = false;
  finalCsvRows = 0;
  finalCsvFinalized = false;
  finalMasterIndexUpdated = false;
  finalCsvReopenable = false;
  finalCsvCadenceOk = false;
  emitEvent("LONG_CAPTURE_STARTED");
}

void handleCommand(const char *raw) {
  String command(raw);
  command.trim();
  command.toUpperCase();
  if (command.length() == 0)
    return;
  if (command == "PING") {
    lastHeartbeatMs = millis();
    return;
  }
  if (command == "STATUS") {
    emitEvent("STATUS");
    emitSample(readSensors());
    return;
  }
  if (command == "ABORT") {
    latchFailure("HOST_ABORT");
    return;
  }
  if (command == "QUALIFY") {
    if (state == QaState::Idle)
      startQualification();
    else
      emitEvent("COMMAND_REJECTED", "QUALIFY_NOT_ALLOWED");
    return;
  }
  if (command == "START_LONG") {
    if (state == QaState::WaitStart)
      startLongCapture();
    else
      emitEvent("COMMAND_REJECTED", "START_LONG_NOT_ALLOWED");
    return;
  }
  emitEvent("COMMAND_REJECTED", "UNKNOWN_COMMAND");
}

void pollSerial() {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r')
      continue;
    if (c == '\n') {
      commandBuffer[commandLength] = '\0';
      handleCommand(commandBuffer);
      commandLength = 0;
      continue;
    }
    if (commandLength + 1 < sizeof(commandBuffer))
      commandBuffer[commandLength++] = c;
    else
      commandLength = 0;
  }
}

void updatePreflight() {
  RS485Bus &bus = RS485Bus::getInstance();
  if (!diagnosticRequested) {
    FlowData flow;
    (void)context.components.flowSensor.getData(flow);
    preflightFlowSampleId = flow.sampleId;
    diagnosticRequested = bus.startAfmDiagnostic();
    if (!diagnosticRequested) {
      latchFailure("AFM07_DIAGNOSTIC_REJECTED");
      return;
    }
  }
  const RS485DiagnosticStatus diagnostic = bus.getAfmDiagnosticStatus();
  if (diagnostic.state == RS485DiagnosticState::Failed) {
    latchFailure("AFM07_DIAGNOSTIC_FAILED");
    return;
  }
  if (diagnostic.state == RS485DiagnosticState::Complete) {
    if (!diagnostic.valueValid || diagnostic.register0004 != 0 || bus.isAfmSafetyBlocked()) {
      latchFailure("AFM07_REGISTER_0004_NOT_ZERO");
      return;
    }
    const SensorSnapshot sensors = readSensors();
    // El diagnóstico ocupa una ranura AFM07 y reancla su calendario. Esperar
    // la siguiente muestra de caudal evita iniciar el soak con una muestra
    // que caducará antes de la siguiente consulta periódica.
    if (baseSensorsReady(sensors) &&
        sensors.flow.sampleId != preflightFlowSampleId && motorsExactlyZero()) {
      bus.resetSlaveStats();
      i2cBaseline = I2CBus::getInstance().getStats();
      previousRtcUnix = 0;
      state = QaState::Stabilizing;
      stateStartedMs = millis();
      lastSampleMs = 0;
      emitEvent("PREFLIGHT_OK");
      return;
    }
  }
  if (millis() - stateStartedMs >= kPreflightTimeoutMs)
    latchFailure("PREFLIGHT_TIMEOUT");
}

void updateMotorOffWindow() {
  const SensorSnapshot sensors = readSensors();
  const char *failure = nullptr;
  if (!runtimeGate(sensors, true, failure)) {
    latchFailure(failure);
    return;
  }
  if (millis() - lastSampleMs >= kSampleMs) {
    lastSampleMs = millis();
    emitSample(sensors);
  }
  if (state == QaState::Stabilizing && millis() - stateStartedMs >= kStabilizationMs) {
    RS485Bus::getInstance().resetSlaveStats();
    i2cBaseline = I2CBus::getInstance().getStats();
    previousRtcUnix = 0;
    state = QaState::Soak;
    stateStartedMs = millis();
    lastSampleMs = 0;
    emitEvent("SOAK_STARTED");
    return;
  }
  if (state == QaState::Soak && millis() - stateStartedMs >= kSoakMs) {
    const RS485SlaveStats afm = RS485Bus::getInstance().getSlaveStats(
        EoloCore::ModbusRtuProtocol::Afm07SlaveId);
    const uint32_t minRest = afm.minRestMs == UINT32_MAX ? 0 : afm.minRestMs;
    if (afm.successes < 2 || afm.maxSuccessGapMs > 1500UL || minRest == 0 ||
        minRest + 5UL < EoloCore::RS485TimingModel::kAfmPollGapMs) {
      latchFailure("AFM07_SOAK_TIMING_GATE");
      return;
    }
    state = QaState::WaitStart;
    stateStartedMs = millis();
    emitEvent("SOAK_APPROVED");
    startLongCapture();
  }
}

void updateCapture() {
  if (millis() - lastHeartbeatMs >= kHeartbeatTimeoutMs) {
    latchFailure("HEARTBEAT_TIMEOUT");
    return;
  }
  const SensorSnapshot sensors = readSensors();
  const char *failure = nullptr;
  if (!runtimeGate(sensors, false, failure)) {
    latchFailure(failure);
    return;
  }
  if (!context.isCaptureActive()) {
    if (!context.hasCaptureEnded() || context.session.elapsedTime < kCaptureSeconds) {
      latchFailure("CAPTURE_ABORTED_BY_PRODUCT_SAFETY");
      return;
    }
    state = QaState::Flushing;
    stateStartedMs = millis();
    context.components.motor.setPwmImmediate(0);
    emitEvent("FLUSH_STARTED");
    return;
  }

  if (millis() - lastSampleMs < kSampleMs)
    return;
  const uint32_t nowMs = millis();
  lastSampleMs = nowMs;
  ++captureSamples;
  const uint32_t dtMs = nowMs - lastIntegratedMs;
  const double representativeFlow = hasIntegratedFlow
      ? (static_cast<double>(lastIntegratedFlowLpm) + sensors.flow.flow) * 0.5
      : static_cast<double>(sensors.flow.flow);
  integratedVolumeL += representativeFlow *
                       static_cast<double>(dtMs) / 60000.0;
  lastIntegratedMs = nowMs;
  lastIntegratedFlowLpm = sensors.flow.flow;
  hasIntegratedFlow = true;

  const bool inBand = sensors.flow.flow >= kBandLowLpm && sensors.flow.flow <= kBandHighLpm;
  if (!reachedBand && inBand) {
    reachedBand = true;
    reachedBandMs = nowMs - captureStartedMs;
    emitEvent("FLOW_BAND_REACHED");
  }
  if (!reachedBand && nowMs - captureStartedMs > 120000UL) {
    latchFailure("FLOW_BAND_NOT_REACHED_WITHIN_120S");
    return;
  }
  if (reachedBand) {
    ++samplesAfterBand;
    if (inBand) {
      ++samplesInBand;
      consecutiveOutsideBand = 0;
    } else {
      ++consecutiveOutsideBand;
      if (consecutiveOutsideBand > maxConsecutiveOutsideBand)
        maxConsecutiveOutsideBand = consecutiveOutsideBand;
      if (consecutiveOutsideBand > 10) {
        latchFailure("FLOW_OUTSIDE_BAND_OVER_10S");
        return;
      }
    }
  }
  emitSample(sensors);
}

void updateFlush() {
  context.components.motor.setPwmImmediate(0);
  if (!context.logsIdle()) {
    if (millis() - stateStartedMs >= kFlushTimeoutMs)
      latchFailure("SD_FLUSH_TIMEOUT");
    return;
  }
  if (!context.isSdReady()) {
    latchFailure("SD_FAILED_DURING_FINALIZE");
    return;
  }
  if (!verifyClosedCapture(finalCsvRows, finalCsvFinalized,
                           finalMasterIndexUpdated, finalCsvReopenable,
                           finalCsvCadenceOk)) {
    latchFailure(!finalCsvFinalized ? "CSV_MISSING_FINALIZADO" :
                 (!finalMasterIndexUpdated ? "MASTER_INDEX_NOT_UPDATED" :
                  (!finalCsvReopenable ? "CSV_NOT_REOPENABLE" : "CSV_10S_CADENCE_INVALID")));
    return;
  }
  const double inBandRatio = samplesAfterBand == 0 ? 0.0 :
      static_cast<double>(samplesInBand) / static_cast<double>(samplesAfterBand);
  if (!reachedBand || reachedBandMs > 120000UL || inBandRatio < 0.95 ||
      maxConsecutiveOutsideBand > 10) {
    latchFailure("LONG_FLOW_ACCEPTANCE_FAILED");
    return;
  }
  if (fabs(integratedVolumeL - context.session.capturedVolume) > 0.1) {
    latchFailure("VOLUME_INTEGRATION_MISMATCH");
    return;
  }
  if (!motorsExactlyZero()) {
    latchFailure("PWM_FINAL_NOT_ZERO");
    return;
  }
  state = QaState::Ready;
  stateStartedMs = millis();
  emitEvent("READY_FOR_LONG_MEASUREMENTS");
  emitResult("READY_FOR_LONG_MEASUREMENTS", "all_acceptance_gates_passed");
}

}  // namespace

void setup() {
#if PPH_PWR_PIN >= 0
  pinMode(PPH_PWR_PIN, OUTPUT);
  digitalWrite(PPH_PWR_PIN, HIGH);
#endif
  I2CBus::getInstance().setWarmupFromNow(I2C_WARMUP_MS);
  context.components.motor.begin();
  context.components.motor.setPwmImmediate(0);
  Serial.begin(115200);
  Serial.setTimeout(25);
  Serial.println("DEMO!");
  // La telemetría QA usa la UART como protocolo; se silencian los logs
  // productivos para que ninguna tarea intercale bytes dentro del JSON.
  SerialOutputState::setTerminalActive(true);
  SystemDiagnostics::instance().begin();
  context.begin();
  stateStartedMs = millis();
  emitEvent("BOOT");
}

void loop() {
  const uint32_t frameStarted = millis();
  SystemDiagnostics::instance().feedWatchdog();
  pollSerial();
  context.update();

  if (state == QaState::Boot && context.bootInitComplete.load()) {
    context.components.motor.setPwmImmediate(0);
    state = QaState::Idle;
    stateStartedMs = millis();
    emitEvent("BOOT_READY");
    startQualification();
  } else if (state == QaState::Preflight) {
    updatePreflight();
  } else if (state == QaState::Stabilizing || state == QaState::Soak) {
    updateMotorOffWindow();
  } else if (state == QaState::Capture) {
    updateCapture();
  } else if (state == QaState::Flushing) {
    updateFlush();
  }

  SystemDiagnostics::instance().loopBeat(millis() - frameStarted);
  delay(2);
}
