#ifndef INPUT_H
#define INPUT_H

#include "../Config.h"

#if SERIAL_INPUT == false

#include <Arduino.h>
#include <Wire.h>

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
      Serial.println("Input ya inicializado, skipping...");
      return;
    }

    Wire.begin(SDA_PIN, SCL_PIN); // Inicializa Wire para comunicación I2C
    Wire.setClock(100000);        // 100kHz
    Serial.println("Encoder inicializado");
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
    readEncoderData();
    debounce();  
  }

  // Reiniciar valor del encoder via driver
  bool resetCounter()
  {
    Wire.beginTransmission(ATTINY_ADDRESS);
    Wire.write(CMD_RESET_COUNTER);
    bool ok = (Wire.endTransmission() == 0);
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
    Wire.beginTransmission(ATTINY_ADDRESS);
    Wire.write(CMD_RESET_BUTTON);
    bool ok = (Wire.endTransmission() == 0);
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

  const bool FLIP_ENCODER = false; // Poner a true si el encoder va invertido
  const int BUTTON_DEBOUNCE_MS = 100; 
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
        Serial.print("Botón cambiado a: ");
        Serial.println(buttonPressed);
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
        Serial.print("Encoder delta cambiado a: ");
        Serial.println(encoderDelta);
      }
    }
  }
    
  // Función interna para leer datos del encoder desde el ATTiny
  void readEncoderData()
  {
    if (Wire.requestFrom(ATTINY_ADDRESS, 3) == 3)
    {
      short int prevCounter = rawCounter;
      bool prevButton = rawButton;
      short int prevDirection = rawDirection;
      
      rawDirection = Wire.read();
      rawCounter = Wire.read();
      rawButton = (Wire.read() == 1);
      
      if (rawCounter != prevCounter)
      {
        Serial.print("Encoder: Contador cambio a ");
        Serial.println(rawCounter);
        hasChanged = true;
      }
      if(rawDirection != prevDirection)
      { 
        
        Serial.print("Encoder: Dirección cambio a ");
        Serial.println(rawDirection);
        hasChanged = true;
      }
      if (rawButton != prevButton)
      {
        Serial.print("Encoder: Boton pulsado? ");
        Serial.println(rawButton);
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
            Serial.println("Input simulado ya inicializado, skipping...");
            return;
        }
        Serial.println("Input simulado iniciado");
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
                Serial.println("Encoder derecha");
            }
            else if (command == 'a') {
                encoderDelta--;
                hasChanged = true;
                Serial.println("Encoder izquierda");
            } 
            else if (command == 's') {
                buttonPressed = true;
                hasChanged = true;
                Serial.println("Botón pulsado");
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