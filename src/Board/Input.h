#ifndef INPUT_H
#define INPUT_H

#include "../Config/Legacy.h"

#if SERIAL_INPUT == false

#include <Arduino.h>
#include "I2CBus.h"
#include "Profiler.h"
#include <atomic>

// Clase para manejar el input del encoder con botón.
// Las lecturas físicas se ejecutan desde Components::i2cWorker mediante
// pollHardware(); la lógica de estado/debounce conserva el comportamiento
// histórico del firmware.
class Input
{
public:
  std::atomic_bool buttonPressed{false};
  std::atomic_bool prevButtonPressed{false};
  std::atomic_bool buttonClick{false};
  std::atomic_int encoderDelta{0};
  bool isReady = false;
  std::atomic_bool hasChanged{false};

  Input() {}

  int intPow(int base, int exp)
  {
    int result = 1;
    for (int i = 0; i < exp; i++)
      result *= base;
    return result;
  }

  bool isButtonPressed()
  {
    // Consume un único evento por flanco ascendente estable. Mantener el
    // botón pulsado no vuelve a generar eventos; la liberación sólo rearma
    // el siguiente clic.
    return buttonClick.exchange(false);
  }

  int getEncoderDelta(int exponent = 2)
  {
#if PROFILE_ENABLED && PROFILE_VERBOSE
    PROFILE_SCOPE("input.delta");
#endif
    int rawDelta = encoderDelta.load();
    int delta = intPow(rawDelta, exponent);
    if (rawDelta < 0 && exponent % 2 == 0)
      delta = -delta;
    return delta;
  }

  void begin()
  {
    if (isReady) {
      LOG_LN("Input ya inicializado, skipping...");
      return;
    }

    I2CBus::getInstance().begin();
    LOG_LN("Encoder inicializado");
    delay(100);
    resetButton();
    resetCounter();
    readEncoderData(); // Leer estado inicial

    // Inicializar estados previos para detección de cambios.
    prevRawCounter = rawCounter;
    prevButtonRaw = rawButton;
    buttonPressed = rawButton;
    prevButtonPressed = rawButton;
    buttonClick = false;
    lastButtonMs = millis();

    isReady = true;
  }

  // La tarea I2C mantiene el sondeo fuera del loop de la UI.
  void poll() {}

  bool pollHardware()
  {
    PROFILE_SCOPE("input.poll");
    bool ok = readEncoderData();
    if (!ok)
      return false;
    debounce();
    return true;
  }

  // Reiniciar valor del encoder via driver.
  bool resetCounter()
  {
    return resetCounterHardware();
  }

  // Reiniciar botón del encoder via driver.
  bool resetButton()
  {
    return resetButtonHardware();
  }

private:
  // Lecturas crudas mantenidas internamente.
  volatile short int rawCounter = 0;
  volatile short int prevRawCounter = 0;

  volatile short int rawDirection = 0;
  volatile short int prevRawDirection = 0;

  volatile bool rawButton = false;
  volatile bool prevButtonRaw = false;

  const bool FLIP_ENCODER = ENCODER_INVERTED;
  const int BUTTON_DEBOUNCE_MS = 150;
  const int ENCODER_DEBOUNCE_MS = 50;
  unsigned long lastEncoderMs = 0;
  unsigned long lastButtonMs = 0;

  bool resetCounterHardware()
  {
    bool ok = I2CBus::getInstance().writeCommand(ATTINY_ADDRESS, CMD_RESET_COUNTER, false);
    if (ok) {
      rawCounter = 0;
      prevRawCounter = 0;
    }
    return ok;
  }

  bool resetButtonHardware()
  {
    bool ok = I2CBus::getInstance().writeCommand(ATTINY_ADDRESS, CMD_RESET_BUTTON, false);
    if (ok) {
      rawButton = false;
      prevButtonRaw = false;
      buttonPressed = false;
      prevButtonPressed = false;
      buttonClick = false;
    }
    return ok;
  }

  void debounce()
  {
    encoderDelta = 0;

    if (rawButton != prevButtonRaw) {
      unsigned long currentMs = millis();
      if (currentMs - lastButtonMs > BUTTON_DEBOUNCE_MS) {
        bool wasPressed = buttonPressed.load();
        prevButtonRaw = rawButton;
        prevButtonPressed = wasPressed;
        buttonPressed = rawButton;
        if (rawButton && !wasPressed) {
          buttonClick = true;
          hasChanged = true;
        }
        lastButtonMs = currentMs;
        if (EoloDebug::verboseLogsEnabled())
          LOG_F("Botón cambiado a: %d\n", buttonPressed.load());
      }
    }

    if (rawCounter != prevRawCounter) {
      unsigned long currentMs = millis();
      if (currentMs - lastEncoderMs > ENCODER_DEBOUNCE_MS) {
        if (FLIP_ENCODER)
          encoderDelta -= (rawCounter - prevRawCounter);
        else
          encoderDelta += (rawCounter - prevRawCounter);

        if (encoderDelta > 127)
          encoderDelta -= 256;
        else if (encoderDelta < -127)
          encoderDelta += 256;

        prevRawCounter = rawCounter;
        lastEncoderMs = currentMs;
        if (EoloDebug::verboseLogsEnabled())
          LOG_F("Encoder cambiado a: %d\n", encoderDelta.load());
      }
    }
  }

  bool readEncoderData()
  {
    uint8_t buffer[3];
    bool readOk = I2CBus::getInstance().readBytes(ATTINY_ADDRESS, buffer, sizeof(buffer), false);
    if (!readOk)
      return false;

    short int previousCounter = rawCounter;
    bool previousButton = rawButton;
    short int previousDirection = rawDirection;

    rawDirection = buffer[0];
    rawCounter = buffer[1];
    rawButton = (buffer[2] == 1);

    if (rawCounter != previousCounter) {
      if (EoloDebug::verboseLogsEnabled())
        LOG_F("Encoder: Contador cambio a %d\n", rawCounter);
      hasChanged = true;
    }
    if (rawDirection != previousDirection) {
      if (EoloDebug::verboseLogsEnabled())
        LOG_F("Encoder: Dirección cambio a %d\n", rawDirection);
      hasChanged = true;
    }
    if (rawButton != previousButton) {
      if (EoloDebug::verboseLogsEnabled())
        LOG_F("Encoder: Boton pulsado? %d\n", rawButton);
      hasChanged = true;
    }
    return true;
  }
};

#else

class Input {
public:
  int encoderDelta = 0;
  bool buttonPressed = false;
  bool isReady = false;
  bool hasChanged = false;

  void begin() {
    if (isReady) {
      LOG_LN("Input simulado ya inicializado, skipping...");
      return;
    }
    LOG_LN("Input simulado iniciado");
    isReady = true;
  }

  void poll() {
    resetButton();
    resetCounter();
    if (Serial.available()) {
      char command = Serial.read();
      if (command == 'd') {
        encoderDelta++;
        hasChanged = true;
        if (EoloDebug::verboseLogsEnabled())
          LOG_LN("Encoder derecha");
      }
      else if (command == 'a') {
        encoderDelta--;
        hasChanged = true;
        if (EoloDebug::verboseLogsEnabled())
          LOG_LN("Encoder izquierda");
      }
      else if (command == 's') {
        buttonPressed = true;
        hasChanged = true;
        if (EoloDebug::verboseLogsEnabled())
          LOG_LN("Botón pulsado");
      }
    }
  }

  bool pollHardware() { poll(); return true; }
  bool isButtonPressed() { return buttonPressed; }
  int getEncoderDelta(int = 2) { return encoderDelta; }
  void resetButton() { buttonPressed = false; }
  void resetCounter() { encoderDelta = 0; }
};

#endif

#endif
