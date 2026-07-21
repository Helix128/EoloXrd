#include <Arduino.h>

#if !defined(EOLO_MODEL_DRON) && !defined(EOLO_MODEL_STANDARD) && \
    !defined(EOLO_MODEL_EXPRESS) && !defined(EOLO_MODEL_EXPRESS_LEGACY)
#define EOLO_MODEL_EXPRESS
#endif
#include "../EoloDemoPinout.h"

/*
  Diagnostico de DIRECCION de PWM del motor/bomba para EOLO Express.

  Polaridad DIRECTA (no invertida), igual que la demo secuencial:
    duty 0   -> gate LOW  -> bomba APAGADA (para MOSFET low-side directo)
    duty 255 -> gate HIGH -> bomba a MAXIMO

  Arranca SIEMPRE en 0 (seguro). Control por serial:
    escribir un numero 0..255  -> setea duty directo en ambos motores
    'x'                        -> parada de emergencia (0)

  Objetivo: confirmar si con polaridad DIRECTA la bomba se comporta
  correctamente (0=apagada, 255=full). El perfil Express debe conservar
  `motorPwmInverted = false`.
*/

static const uint32_t SERIAL_BAUD = 115200;
static const int PWM_FREQ = 20000;
static const int PWM_RES  = 8;      // 8-bit -> 0..255
static const int CH0 = 0;
static const int CH1 = 1;

// Pines de motor según EoloDemoPinout (Express: 32/33; Standard: 14/25).
static const int MOTOR_A = MOTOR_PWM_PIN_A;
static const int MOTOR_B = MOTOR_PWM_PIN_B;

void applyDuty(int duty) {
  duty = constrain(duty, 0, 255);
  ledcWrite(CH0, duty);
  ledcWrite(CH1, duty);
  Serial.printf(">> DUTY DIRECTO = %d  (0=apagado, 255=full)\n", duty);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(600);

  // Estado seguro ANTES de configurar PWM: pines LOW
  pinMode(MOTOR_A, OUTPUT); digitalWrite(MOTOR_A, LOW);
  pinMode(MOTOR_B, OUTPUT); digitalWrite(MOTOR_B, LOW);

  ledcSetup(CH0, PWM_FREQ, PWM_RES);
  ledcSetup(CH1, PWM_FREQ, PWM_RES);
  ledcWrite(CH0, 0);
  ledcWrite(CH1, 0);
  ledcAttachPin(MOTOR_A, CH0);
  ledcAttachPin(MOTOR_B, CH1);
  ledcWrite(CH0, 0);
  ledcWrite(CH1, 0);

  Serial.println();
  Serial.println("### DIAG DIRECCION PWM MOTOR (polaridad DIRECTA) ###");
  Serial.printf("Motores en GPIO %d y %d | freq=%dHz | 8-bit\n", MOTOR_A, MOTOR_B, PWM_FREQ);
  Serial.println("Arranca en DUTY 0. La bomba debe estar APAGADA ahora.");
  Serial.println("Comandos: <0-255> setea duty | 'x' parada");
}

void loop() {
  static String buf;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 'x' || c == 'X') { buf = ""; applyDuty(0); continue; }
    if (c == '\n' || c == '\r') {
      buf.trim();
      if (buf.length()) applyDuty(buf.toInt());
      buf = "";
    } else {
      buf += c;
    }
  }
}
