#include <Arduino.h>
#include <ModbusMaster.h>
#include <Eolo/Core/Flow/SmartFlowController.h>

#if !defined(EOLO_MODEL_DRON)
#define EOLO_MODEL_DRON
#endif
#include "../EoloDemoPinout.h"

/*
  Demo de calibracion dinamica de motor para EOLO Dron.

  Hace control closed-loop temporal con motor + AFM07, sin guardar calibracion
  en flash y sin usar el ciclo de captura productivo.

  Comandos Serial:
    t 5.0              cambia flujo objetivo en L/min
    b 1660             cambia PWM inicial 0..2047
    p 640              fija PWM actual/manual antes de arrancar
    tune slow          perfil slow|normal|fast
    model              imprime modelo RAM
    resetmodel         borra modelo RAM
    verbose on|off     logs compactos/detallados
    run                habilita control dinamico
    stop               apaga motor
    status             imprime estado
*/

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t RS485_BAUD = 4800;
static const uint8_t AFM07_SLAVE_ID = 2;
static const uint16_t AFM07_FLOW_REG = 0x0000;

static const int MOTOR_PWM_CHANNEL = 0;
static const int MOTOR_PWM_FREQ = 20000;
static const int MOTOR_PWM_RESOLUTION = 11;
static const int MOTOR_PWM_MAX = (1 << MOTOR_PWM_RESOLUTION) - 1;

static const uint32_t CONTROL_INTERVAL_MS = 200;

HardwareSerial rs485(2);
ModbusMaster afm07;

bool controlEnabled = false;
float targetFlowLpm = 5.0f;
int basePwm = 1660;
int currentPwm = 0;
uint32_t lastControlMs = 0;
uint8_t consecutiveAfm07Errors = 0;
bool verboseLogs = false;
SmartFlowController flowController;
SmartFlowStatus lastControlStatus;

int constrainPwm(int pwm) {
  if (pwm < 0) return 0;
  if (pwm > MOTOR_PWM_MAX) return MOTOR_PWM_MAX;
  return pwm;
}

void writeMotorPwm(int pwm) {
  currentPwm = constrainPwm(pwm);
  ledcWrite(MOTOR_PWM_CHANNEL, currentPwm);
}

void preTransmission() {
  digitalWrite(RS485_DE_RE_PIN, HIGH);
  delayMicroseconds(200);
}

void postTransmission() {
  delayMicroseconds(200);
  digitalWrite(RS485_DE_RE_PIN, LOW);
}

bool readAfm07(float &flowLpm, uint8_t &errorCode) {
  errorCode = afm07.readHoldingRegisters(AFM07_FLOW_REG, 1);
  if (errorCode != ModbusMaster::ku8MBSuccess) return false;
  flowLpm = afm07.getResponseBuffer(0) / 100.0f;
  return true;
}

void resetControllerState(bool preserveModel) {
  flowController.resetController(preserveModel);
  lastControlStatus = SmartFlowStatus();
}

const char *modeName(SmartFlowMode mode) {
  switch (mode) {
    case SMART_FLOW_INTERPOLATE: return "interp";
    case SMART_FLOW_GAIN_PREDICT: return "gain";
    case SMART_FLOW_MIN_ACTIVE_BOOST: return "boost";
    case SMART_FLOW_PID_ONLY:
    default: return "pid";
  }
}

SmartFlowTune tunePreset(const String &name) {
  SmartFlowTune tune;
  if (name == "slow" || name == "suave") {
    tune.kp = 12.0f;
    tune.ki = 0.9f;
    tune.kd = 8.0f;
    tune.integralLimit = 12.0f;
    tune.deadband = 0.12f;
    tune.fastAlpha = 0.35f;
    tune.slowAlpha = 0.12f;
    tune.feedForwardAlpha = 0.12f;
    tune.minStep = 1;
    tune.maxStep = 8;
    tune.maxFeedForwardStep = 5;
  } else if (name == "fast" || name == "agresivo") {
    tune.kp = 26.0f;
    tune.ki = 2.0f;
    tune.kd = 4.0f;
    tune.integralLimit = 26.0f;
    tune.deadband = 0.08f;
    tune.fastAlpha = 0.55f;
    tune.slowAlpha = 0.22f;
    tune.feedForwardAlpha = 0.24f;
    tune.minStep = 2;
    tune.maxStep = 20;
    tune.maxFeedForwardStep = 14;
  }
  return tune;
}

void printModel() {
  Serial.printf("model valid=%s mode=%s gain=%.5f confidence=%.2f minFlowPwm=%d maxUsefulPwm=%d pwmFF=%d pid=%d step=%d fast=%.2f slow=%.2f dFlow=%.2f saturation=%s clamp=%s\n",
                lastControlStatus.modelValid ? "yes" : "no",
                modeName(lastControlStatus.mode),
                lastControlStatus.estimatedGain,
                lastControlStatus.confidence,
                lastControlStatus.minFlowPwm,
                lastControlStatus.maxUsefulPwm,
                lastControlStatus.pwmFF,
                lastControlStatus.pidCorrection,
                lastControlStatus.step,
                lastControlStatus.fastFlow,
                lastControlStatus.slowFlow,
                lastControlStatus.derivativeFlow,
                lastControlStatus.upperSaturation ? "yes" : "no",
                lastControlStatus.lowerClamp ? "yes" : "no");
}

void printStatus() {
  Serial.printf("enabled=%s target=%.2f L/min base=%d current=%d integral=%.3f gain=%.5f confidence=%.2f\n",
                controlEnabled ? "yes" : "no",
                targetFlowLpm,
                basePwm,
                currentPwm,
                lastControlStatus.integral,
                lastControlStatus.estimatedGain,
                lastControlStatus.confidence);
}

void printHelp() {
  Serial.println();
  Serial.println("Demo calibracion dinamica motor EOLO Dron");
  Serial.println("Comandos: t 5.0 | b 1660 | p 640 | tune slow|normal|fast | model | resetmodel | verbose on|off | run | stop | status");
  printStatus();
}

void handleSerialCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line == "run") {
    controlEnabled = true;
    resetControllerState(true);
    if (currentPwm == 0) writeMotorPwm(basePwm);
    lastControlMs = 0;
    consecutiveAfm07Errors = 0;
    Serial.println("Control dinamico ON");
  } else if (line == "stop") {
    controlEnabled = false;
    resetControllerState(false);
    writeMotorPwm(0);
    Serial.println("Motor OFF");
  } else if (line == "status") {
    printStatus();
  } else if (line == "model") {
    printModel();
  } else if (line == "resetmodel") {
    resetControllerState(false);
    Serial.println("Modelo RAM reiniciado");
  } else if (line.startsWith("verbose ")) {
    String value = line.substring(8);
    value.trim();
    verboseLogs = value == "on" || value == "1" || value == "yes";
    Serial.printf("verbose=%s\n", verboseLogs ? "on" : "off");
  } else if (line.startsWith("tune ")) {
    String name = line.substring(5);
    name.trim();
    if (name == "normal" || name == "slow" || name == "fast" || name == "suave" || name == "agresivo") {
      flowController.setTune(tunePreset(name));
      resetControllerState(true);
      Serial.printf("tune=%s\n", name.c_str());
    } else {
      Serial.println("tune invalido: slow|normal|fast");
    }
  } else if (line.startsWith("t ")) {
    targetFlowLpm = line.substring(2).toFloat();
    resetControllerState(true);
    Serial.printf("target=%.2f L/min\n", targetFlowLpm);
  } else if (line.startsWith("b ")) {
    basePwm = constrainPwm(line.substring(2).toInt());
    resetControllerState(true);
    writeMotorPwm(basePwm);
    Serial.printf("base PWM=%d\n", basePwm);
  } else if (line.startsWith("p ")) {
    writeMotorPwm(line.substring(2).toInt());
    resetControllerState(true);
    Serial.printf("PWM actual=%d\n", currentPwm);
  } else {
    printHelp();
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  enableDemoPeripheralPower();

  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);
  rs485.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  afm07.begin(AFM07_SLAVE_ID, rs485);
  afm07.preTransmission(preTransmission);
  afm07.postTransmission(postTransmission);

  ledcSetup(MOTOR_PWM_CHANNEL, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
  ledcAttachPin(MOTOR_PWM_PIN, MOTOR_PWM_CHANNEL);
  writeMotorPwm(0);

  printHelp();
}

void loop() {
  if (Serial.available()) {
    handleSerialCommand(Serial.readStringUntil('\n'));
  }

  if (!controlEnabled) return;

  uint32_t nowMs = millis();
  if (lastControlMs != 0 && nowMs - lastControlMs < CONTROL_INTERVAL_MS) return;

  float dtSeconds = lastControlMs == 0 ? 1.0f : (nowMs - lastControlMs) / 1000.0f;
  lastControlMs = nowMs;

  float measuredFlow = 0.0f;
  uint8_t errorCode = 0;
  if (!readAfm07(measuredFlow, errorCode)) {
    consecutiveAfm07Errors++;
    if (consecutiveAfm07Errors >= 5) resetControllerState(false);
    Serial.printf("AFM07 error=0x%02X; mantiene PWM=%d errors=%u\n", errorCode, currentPwm, consecutiveAfm07Errors);
    return;
  }
  consecutiveAfm07Errors = 0;

  int previousPwm = currentPwm;
  lastControlStatus = flowController.update(nowMs, currentPwm, targetFlowLpm, measuredFlow, dtSeconds, MOTOR_PWM_MAX);
  writeMotorPwm(lastControlStatus.pwm);

  if (verboseLogs) {
    Serial.printf("target=%.2f raw=%.2f fast=%.2f slow=%.2f error=%.2f base=%d pwm=%d->%d ff=%d pid=%d step=%d mode=%s gain=%.5f conf=%.2f integral=%.3f sat=%s clamp=%s\n",
                  targetFlowLpm,
                  measuredFlow,
                  lastControlStatus.fastFlow,
                  lastControlStatus.slowFlow,
                  lastControlStatus.errorFast,
                  basePwm,
                  previousPwm,
                  currentPwm,
                  lastControlStatus.pwmFF,
                  lastControlStatus.pidCorrection,
                  lastControlStatus.step,
                  modeName(lastControlStatus.mode),
                  lastControlStatus.estimatedGain,
                  lastControlStatus.confidence,
                  lastControlStatus.integral,
                  lastControlStatus.upperSaturation ? "yes" : "no",
                  lastControlStatus.lowerClamp ? "yes" : "no");
  } else {
    Serial.printf("target=%.2f raw=%.2f fast=%.2f error=%.2f pwm=%d->%d mode=%s ff=%d pid=%d step=%d conf=%.2f\n",
                  targetFlowLpm,
                  measuredFlow,
                  lastControlStatus.fastFlow,
                  lastControlStatus.errorFast,
                  previousPwm,
                  currentPwm,
                  modeName(lastControlStatus.mode),
                  lastControlStatus.pwmFF,
                  lastControlStatus.pidCorrection,
                  lastControlStatus.step,
                  lastControlStatus.confidence);
  }
}
