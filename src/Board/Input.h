#ifndef INPUT_H
#define INPUT_H

#include "../Config/Legacy.h"

#if SERIAL_INPUT == false

#include <Arduino.h>
#include "I2CBus.h"
#include "Profiler.h" 
#include <atomic>

// Clase para manejar el input del encoder con botón
class Input
{
public:

  std::atomic_bool buttonPressed{false};
  std::atomic_bool prevButtonPressed{false};
  std::atomic_int encoderDelta{0};
  bool isReady = false;
  std::atomic_bool hasChanged{false};
  
  // Constructor
  Input() {}

  int intPow(int base, int exp)
  {
    int result = 1;
    for (int i = 0; i < exp; i++)
    {
      result *= base;
    }
    return result;
  }

  bool isButtonPressed()
  {
    if (prevButtonPressed == buttonPressed)
    {
      return false;
    }
    return buttonPressed;
  }

  int getEncoderDelta(int exponent = 2)
  {
#if PROFILE_ENABLED && PROFILE_VERBOSE
    PROFILE_SCOPE("input.delta");
#endif
    int delta = intPow(encoderDelta, exponent);
    if (encoderDelta < 0 && exponent % 2 == 0)
    {
      delta = -delta;
    }
    return delta;
  }

  void begin()
  {
    if (isReady) {
      LOG_LN("Input ya inicializado, skipping...");
      return;
    }

    LOG_LN("Encoder inicializado");
    resetButtonHardware();
    resetCounterHardware();
    readEncoderData(); // Leer estado inicial

    // Inicializar estados previos para detección de cambios
    prevRawCounter = rawCounter;
    prevButtonRaw = rawButton;
    lastButtonMs = millis();
    
    isReady = true;
  }

  // El loop de UI conserva esta API por compatibilidad, pero ya no toca I2C.
  // Las lecturas las realiza Components::i2cWorker en core 0.
  void poll()
  {
    // No-op intencional.
  }

  bool pollHardware()
  {
    PROFILE_SCOPE("input.poll");
    serviceCommands();
    bool ok = readEncoderData();
    if (!ok)
      return false;
    debounce();
    return true;
  }

  // Reiniciar valor del encoder via driver
  bool resetCounter()
  {
    _pendingCommands.fetch_or(CMD_COUNTER_PENDING);
    return true;
  }

  // Reiniciar botón del encoder via driver
  bool resetButton()
  {
    _pendingCommands.fetch_or(CMD_BUTTON_PENDING);
    return true;
  }

private:
  // Lecturas crudas mantenidas internamente
  volatile short int rawCounter = 0;   // Contador raw (0..255)
  volatile short int prevRawCounter = 0;

  volatile short int rawDirection = 0; // Dirección de giro raw
  volatile short int prevRawDirection = 0;
  
  volatile bool rawButton = false; // Estado del botón (raw)
  volatile bool prevButtonRaw = false;

  static constexpr uint8_t CMD_COUNTER_PENDING = 0x01;
  static constexpr uint8_t CMD_BUTTON_PENDING = 0x02;
  std::atomic<uint8_t> _pendingCommands{0};

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
    }
    return ok;
  }

  void serviceCommands()
  {
    uint8_t pending = _pendingCommands.exchange(0);
    if (pending & CMD_BUTTON_PENDING)
      resetButtonHardware();
    if (pending & CMD_COUNTER_PENDING)
      resetCounterHardware();
  }

  void debounce(){
    encoderDelta = 0;
    buttonPressed = false;
    if(rawButton!=prevButtonRaw){
      unsigned long currentMs = millis();
      if(currentMs - lastButtonMs > BUTTON_DEBOUNCE_MS){
        prevButtonRaw = rawButton;
        prevButtonPressed = buttonPressed.load();
        buttonPressed = rawButton;
        lastButtonMs = currentMs;
        if (EoloDebug::verboseLogsEnabled()) {
          LOG_F("Botón cambiado a: %d\n", buttonPressed.load());
        }
      }
    }
    if(rawCounter!=prevRawCounter){
      unsigned long currentMs = millis();
      if(currentMs - lastEncoderMs > ENCODER_DEBOUNCE_MS){
       
        if(FLIP_ENCODER){
          encoderDelta -= (rawCounter - prevRawCounter);
        }
        else{
          encoderDelta += (rawCounter - prevRawCounter);
        }
        if(encoderDelta>127){
          encoderDelta -= 256;
        }
        else if(encoderDelta<-127){
          encoderDelta += 256;
        }
        prevRawCounter = rawCounter;
        
        lastEncoderMs = currentMs;
        if (EoloDebug::verboseLogsEnabled()) {
          LOG_F("Encoder cambiado a: %d\n", encoderDelta.load());
        }
      }
    }
  }
    
  // Función interna para leer datos del encoder desde el ATTiny
  bool readEncoderData()
  {     
    uint8_t buffer[3];
    bool readOk = I2CBus::getInstance().readBytes(ATTINY_ADDRESS, buffer, sizeof(buffer), false);
    if (readOk)
    {
      short int prevCounter = rawCounter;
      bool prevButton = rawButton;
      short int prevDirection = rawDirection;
      
      rawDirection = buffer[0];
      rawCounter = buffer[1];
      rawButton = (buffer[2] == 1);
      
      if (rawCounter != prevCounter)
      {
        if (EoloDebug::verboseLogsEnabled()) {
          LOG_F("Encoder: Contador cambio a %d\n", rawCounter);
        }
        hasChanged = true;
      }
      if(rawDirection != prevDirection)
      { 
        
        if (EoloDebug::verboseLogsEnabled()) {
          LOG_F("Encoder: Dirección cambio a %d\n", rawDirection);
        }
        hasChanged = true;
      }
      if (rawButton != prevButton)
      {
        if (EoloDebug::verboseLogsEnabled()) {
          LOG_F("Encoder: Boton pulsado? %d\n", rawButton);
        }
        hasChanged = true;
      }
      return true;
    }
    return false;
  }
};
#else

class Input{
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
                if (EoloDebug::verboseLogsEnabled()) {
                    LOG_LN("Encoder derecha");
                }
            }
            else if (command == 'a') {
                encoderDelta--;
                hasChanged = true;
                if (EoloDebug::verboseLogsEnabled()) {
                    LOG_LN("Encoder izquierda");
                }
            } 
            else if (command == 's') {
                buttonPressed = true;
                hasChanged = true;
                if (EoloDebug::verboseLogsEnabled()) {
                    LOG_LN("Botón pulsado");
                }
            }
        }
    }

    void resetButton() {
        buttonPressed = false;
    }
    void resetCounter() {
        encoderDelta = 0;
    }
};

#endif // INPUT_H
#endif
