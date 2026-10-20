#ifndef INPUT_H
#define INPUT_H

#include "../Variants/Legacy.h"

#include <Arduino.h>
#include "../Common/Board/EncoderBus.h"
#include "Profiler.h"
#include <atomic>

// Comandos del ATtiny del encoder.
namespace InputCommand
{
constexpr uint8_t ResetCounter = 0x01;
constexpr uint8_t ResetButton = 0x02;
}

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
  // Movimiento acumulado desde la última lectura. El ATmega entrega un
  // contador absoluto de 8 bits; el worker suma cada diferencia y la UI la
  // consume una sola vez, así ningún clic se pierde aunque se omita un frame.
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

  // Consume el movimiento acumulado (la segunda llamada devuelve 0 hasta que
  // llegue movimiento nuevo). Cada escena debe leerlo una sola vez por update.
  int getEncoderDelta(int exponent = 2)
  {
#if PROFILE_ENABLED && PROFILE_VERBOSE
    PROFILE_SCOPE("input.delta");
#endif
    int rawDelta = encoderDelta.exchange(0);
    int delta = intPow(rawDelta, exponent);
    if (rawDelta < 0 && exponent % 2 == 0)
      delta = -delta;
    return delta;
  }

  // Descarta movimiento pendiente; una escena nueva no debe heredar giros
  // hechos mientras otra pantalla ignoraba el encoder.
  void clearEncoderDelta()
  {
    encoderDelta = 0;
  }

  void begin()
  {
    if (isReady) {
      LOG_LN("Input ya inicializado, skipping...");
      return;
    }

    // El primer acceso inicializa el bus del encoder (principal o Wire1).
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

  const bool FLIP_ENCODER = EoloConfig::encoderInverted;
  const int BUTTON_DEBOUNCE_MS = 150;
  unsigned long lastButtonMs = 0;

  bool resetCounterHardware()
  {
    bool ok = EncoderBus::writeCommand(EoloConfig::attinyAddress, InputCommand::ResetCounter);
    if (ok) {
      rawCounter = 0;
      prevRawCounter = 0;
    }
    return ok;
  }

  bool resetButtonHardware()
  {
    bool ok = EncoderBus::writeCommand(EoloConfig::attinyAddress, InputCommand::ResetButton);
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
      // Contador de 8 bits con desbordamiento: la diferencia módulo 256 con
      // signo es el movimiento real entre dos lecturas.
      int diff = (int8_t)(uint8_t)(rawCounter - prevRawCounter);
      prevRawCounter = rawCounter;
      if (FLIP_ENCODER)
        diff = -diff;
      encoderDelta += diff;
      // La bandera se levanta después de acumular: la UI nunca ve el aviso
      // antes de que exista el movimiento.
      hasChanged = true;
      if (EoloDebug::verboseLogsEnabled())
        LOG_F("Encoder acumulado: %d\n", encoderDelta.load());
    }
  }

  bool readEncoderData()
  {
    uint8_t buffer[3];
    bool readOk = EncoderBus::readBytes(EoloConfig::attinyAddress, buffer, sizeof(buffer));
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
    }
    if (rawDirection != previousDirection) {
      if (EoloDebug::verboseLogsEnabled())
        LOG_F("Encoder: Dirección cambio a %d\n", rawDirection);
    }
    if (rawButton != previousButton) {
      if (EoloDebug::verboseLogsEnabled())
        LOG_F("Encoder: Boton pulsado? %d\n", rawButton);
      hasChanged = true;
    }
    return true;
  }
};


#endif
