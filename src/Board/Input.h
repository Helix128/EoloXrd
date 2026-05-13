#ifndef INPUT_H
#define INPUT_H

#include "../Config.h"

#if SERIAL_INPUT == false

#include <Arduino.h>
#include "I2CBus.h"
#include "Profiler.h" 

// Clase para manejar el input del encoder con botón
class Input
{
public:

  bool buttonPressed = false;
  bool prevButtonPressed = false;
  int encoderDelta = 0;
  bool isReady = false;
  bool hasChanged = false;
  
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
    Profiler p("Input getEncoderDelta");
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

    I2CBus::getInstance().begin();
    LOG_LN("Encoder inicializado");
    delay(100);
    resetButton();
    resetCounter();
    readEncoderData(); // Leer estado inicial

    // Inicializar estados previos para detección de cambios
    prevRawCounter = rawCounter;
    prevButtonRaw = rawButton;
    lastButtonMs = millis();
    
    isReady = true;
  }

  // Actualizar lecturas desde el driver
  void poll()
  {
    Profiler p("Input poll");
    readEncoderData();
    debounce();  
  }

  // Reiniciar valor del encoder via driver
  bool resetCounter()
  {
    bool ok = I2CBus::getInstance().writeCommand(ATTINY_ADDRESS, CMD_RESET_COUNTER);
    if (ok)
    {
      rawCounter = 0;
      prevRawCounter = 0;
    }
    return ok;
  }

  // Reiniciar botón del encoder via driver
  bool resetButton()
  {
    bool ok = I2CBus::getInstance().writeCommand(ATTINY_ADDRESS, CMD_RESET_BUTTON);
    if (ok)
    {
      rawButton = false;
      prevButtonRaw = false;
    }
    return ok;
  }

private:
  // Lecturas crudas mantenidas internamente
  volatile short int rawCounter = 0;   // Contador raw (0..255)
  volatile short int prevRawCounter = 0;

  volatile short int rawDirection = 0; // Dirección de giro raw
  volatile short int prevRawDirection = 0;
  
  volatile bool rawButton = false; // Estado del botón (raw)
  volatile bool prevButtonRaw = false;

  const bool FLIP_ENCODER = true; // Poner a true si el encoder va invertido
  const int BUTTON_DEBOUNCE_MS = 150; 
  const int ENCODER_DEBOUNCE_MS = 50;
  unsigned long lastEncoderMs = 0;
  unsigned long lastButtonMs = 0;

  void debounce(){
    encoderDelta = 0;
    buttonPressed = false;
    if(rawButton!=prevButtonRaw){
      unsigned long currentMs = millis();
      if(currentMs - lastButtonMs > BUTTON_DEBOUNCE_MS){
        prevButtonRaw = rawButton;
        prevButtonPressed = buttonPressed;
        buttonPressed = rawButton;
        lastButtonMs = currentMs;
        LOG_F("Botón cambiado a: %d\n", buttonPressed);
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
        LOG_F("Encoder cambiado a: %d\n", encoderDelta);
      }
    }
  }
    
  // Función interna para leer datos del encoder desde el ATTiny
  void readEncoderData()
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
        LOG_F("Encoder: Contador cambio a %d\n", rawCounter);
        hasChanged = true;
      }
      if(rawDirection != prevDirection)
      { 
        
        LOG_F("Encoder: Dirección cambio a %d\n", rawDirection);
        hasChanged = true;
      }
      if (rawButton != prevButton)
      {
        LOG_F("Encoder: Boton pulsado? %d\n", rawButton);
        hasChanged = true;
      }
    }
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
                LOG_LN("Encoder derecha");
            }
            else if (command == 'a') {
                encoderDelta--;
                hasChanged = true;
                LOG_LN("Encoder izquierda");
            } 
            else if (command == 's') {
                buttonPressed = true;
                hasChanged = true;
                LOG_LN("Botón pulsado");
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
