#ifndef INPUT_H
#define INPUT_H

#include "../Config.h"

#if BAREBONES == false

#include <Arduino.h>
#include <Wire.h>

// Clase para manejar el input del encoder con botón
class Input
{
public:

  bool buttonPressed = false;
  int encoderDelta = 0;

  // Constructor
  Input() {}

  void begin()
  {
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

  const int BUTTON_DEBOUNCE_MS = 50; 
  const int ENCODER_DEBOUNCE_MS = 25;
  unsigned long lastEncoderMs = 0;
  unsigned long lastButtonMs = 0;

  void debounce(){

    encoderDelta = 0;

    if(rawButton!=prevButtonRaw){
      unsigned long currentMs = millis();
      if(currentMs - lastButtonMs > BUTTON_DEBOUNCE_MS){
        prevButtonRaw = rawButton;
        buttonPressed = rawButton;
        lastButtonMs = currentMs;
        Serial.print("Botón cambiado a: ");
        Serial.println(buttonPressed);
      }
    }
    if(rawCounter!=prevRawCounter){
      unsigned long currentMs = millis();
      if(currentMs - lastEncoderMs > ENCODER_DEBOUNCE_MS){
        encoderDelta += (rawCounter - prevRawCounter);
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
      }
      if(rawDirection != prevDirection)
      {
        Serial.print("Encoder: Dirección cambio a ");
        Serial.println(rawDirection);
      }
      if (rawButton != prevButton)
      {
        Serial.print("Encoder: Boton pulsado? ");
        Serial.println(rawButton);
      }
    }
  }
};
#else

class Input{
    public:
    int encoderDelta = 0;
    bool buttonPressed = false;

    void begin() {
        Serial.println("Input simulado iniciado");
    }

    void poll() {
        resetButton();
        resetCounter();
        if (Serial.available()) {
            char command = Serial.read();
            if (command == 'd') {
                encoderDelta++;
                Serial.println("Encoder derecha");
            }
            else if (command == 'a') {
                encoderDelta--;
                Serial.println("Encoder izquierda");
            } 
            else if (command == 's') {
                buttonPressed = true;
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