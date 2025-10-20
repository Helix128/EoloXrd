// Librerías para display y comunicación I2C
#include <U8g2lib.h>
#include <Wire.h>

// Módulos principales
#include "Data/Context.h"
#include "Drawing/SceneManager.h"
#include "Drawing/SceneRegistry.h"
#include "Config.h" 

#include "Testing/FlowMotor.h"

// Instancias globales
DisplayModel u8g2(U8G2_R0, SCL_PIN, SDA_PIN);
Context ctx(u8g2); // Aquí se procesa toda la lógica

// Crear instancia de Logger
Logger logger;

void setup()
{ 
  Serial.begin(115200);
  while(!Serial){
    delay(50);
  }
  // Inicialización del contexto de la app
  ctx.begin();
  
  // Registrar todas las escenas (SceneRegistry)
  registerAllScenes();
  
  // testFlowMotor(ctx);

  // Carga la escena inicial (splash)
  SceneManager::setScene("splash", ctx);
}

void loop()
{
  // Actualizar el contexto de la app y la escena actual
  ctx.update();
  SceneManager::update(ctx);
}
