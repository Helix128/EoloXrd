#include <Arduino.h>
#include <ModbusMaster.h>

#if !defined(EOLO_MODEL_DRON)
#define EOLO_MODEL_DRON
#endif
#include "../EoloDemoPinout.h"

/*
  Demo de calibracion dinamica de motor para EOLO Dron.

  Hace control closed-loop temporal con motor + AFM07, sin guardar calibracion
  en flash y sin usar el ciclo de captura productivo.

  Comandos Serial:
    t 5.0   cambia flujo objetivo en L/min
    b 960   cambia PWM base 0..2047
    p 640   fija PWM actual/manual antes de arrancar
    run     habilita control dinamico
    stop    apaga motor
    status  imprime estado
*/

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t RS485_BAUD = 4800;
static const uint8_t AFM07_SLAVE_ID = 2;
static const uint16_t AFM07_FLOW_REG = 0x0000;

static const int MOTOR_PWM_CHANNEL = 0;
static const int MOTOR_PWM_FREQ = 20000;
static const int MOTOR_PWM_RESOLUTION = 11;
static const int MOTOR_PWM_MAX = (1 << MOTOR_PWM_RESOLUTION) - 1;

static const uint32_t CONTROL_INTERVAL_MS = 1500;
static const float FLOW_DEADBAND_LPM = 0.15f;
static const float FLOW_KP = 48.0f;
static const float FLOW_KI = 4.0f;
static const float FLOW_INTEGRAL_LIMIT = 40.0f;
static const float FLOW_FILTER_ALPHA = 0.30f;
static const float FLOW_MIN_ACTIVE_LPM = 0.20f;
static const int FLOW_MAX_STEP_PWM = 32;

HardwareSerial rs485(2);
ModbusMaster afm07;

bool controlEnabled = false;
float targetFlowLpm = 5.0f;
int basePwm = 960;
int currentPwm = 0;
float integral = 0.0f;
float filteredFlowLpm = 0.0f;
bool filteredFlowReady = false;
uint32_t lastControlMs = 0;

int constrainPwm(int pwm) {
  if (pwm < 0) return 0;
  if (pwm > MOTOR_PWM_MAX) return MOTOR_PWM_MAX;
  return pwm;
}

int limitPwmStep(int current, int target, int maxStep) {
  current = constrainPwm(current);
  target = constrainPwm(target);
  if (maxStep <= 0) return target;
  if (target > current + maxStep) return current + maxStep;
  if (target < current - maxStep) return current - maxStep;
  return target;
}

int nextClosedLoopPwm(int base, int current, float targetFlow, float measuredFlow, float dtSeconds) {
  float error = targetFlow - measuredFlow;
  if (fabsf(error) < FLOW_DEADBAND_LPM) error = 0.0f;

  if (measuredFlow < FLOW_MIN_ACTIVE_LPM && targetFlow > FLOW_MIN_ACTIVE_LPM) {
    integral = 0.0f;
    return limitPwmStep(current, MOTOR_PWM_MAX, FLOW_MAX_STEP_PWM);
  }

  integral += error * dtSeconds;
  if (integral > FLOW_INTEGRAL_LIMIT) integral = FLOW_INTEGRAL_LIMIT;
  if (integral < -FLOW_INTEGRAL_LIMIT) integral = -FLOW_INTEGRAL_LIMIT;

  float correction = FLOW_KP * error + FLOW_KI * integral;
  int desired = constrainPwm(base + static_cast<int>(correction));
  return limitPwmStep(current, desired, FLOW_MAX_STEP_PWM);
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
  flowLpm = afm07.getResponseBuffer(0) / 10.0f;
  return true;
}

void resetControllerState() {
  integral = 0.0f;
  filteredFlowLpm = 0.0f;
  filteredFlowReady = false;
}

void printStatus() {
  Serial.printf("enabled=%s target=%.2f L/min base=%d current=%d integral=%.3f\n",
                controlEnabled ? "yes" : "no",
                targetFlowLpm,
                basePwm,
                currentPwm,
                integral);
}

void printHelp() {
  Serial.println();
  Serial.println("Demo calibracion dinamica motor EOLO Dron");
  Serial.println("Comandos: t 5.0 | b 960 | p 640 | run | stop | status");
  printStatus();
}

void handleSerialCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line == "run") {
    controlEnabled = true;
    resetControllerState();
    if (currentPwm == 0) writeMotorPwm(basePwm);
    lastControlMs = 0;
    Serial.println("Control dinamico ON");
  } else if (line == "stop") {
    controlEnabled = false;
    resetControllerState();
    writeMotorPwm(0);
    Serial.println("Motor OFF");
  } else if (line == "status") {
    printStatus();
  } else if (line.startsWith("t ")) {
    targetFlowLpm = line.substring(2).toFloat();
    resetControllerState();
    Serial.printf("target=%.2f L/min\n", targetFlowLpm);
  } else if (line.startsWith("b ")) {
    basePwm = constrainPwm(line.substring(2).toInt());
    resetControllerState();
    Serial.printf("base PWM=%d\n", basePwm);
  } else if (line.startsWith("p ")) {
    writeMotorPwm(line.substring(2).toInt());
    resetControllerState();
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
    Serial.printf("AFM07 error=0x%02X; mantiene PWM=%d\n", errorCode, currentPwm);
    return;
  }

  if (!filteredFlowReady) {
    filteredFlowLpm = measuredFlow;
    filteredFlowReady = true;
  } else {
    filteredFlowLpm = FLOW_FILTER_ALPHA * measuredFlow + (1.0f - FLOW_FILTER_ALPHA) * filteredFlowLpm;
  }

  int previousPwm = currentPwm;
  int nextPwm = nextClosedLoopPwm(basePwm, currentPwm, targetFlowLpm, filteredFlowLpm, dtSeconds);
  writeMotorPwm(nextPwm);

  Serial.printf("target=%.2f raw=%.2f filtered=%.2f error=%.2f base=%d pwm=%d->%d integral=%.3f\n",
                targetFlowLpm,
                measuredFlow,
                filteredFlowLpm,
                targetFlowLpm - filteredFlowLpm,
                basePwm,
                previousPwm,
                currentPwm,
                integral);
}
