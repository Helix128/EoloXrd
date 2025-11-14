// -- EOLO --
// Centro de Investigación en Tecnologías para la Sociedad (C+)
// Universidad del Desarrollo

// Librerías para display y comunicación I2C
#include <U8g2lib.h>
#include <Wire.h>

// Módulos principales
#include "Data/Context.h"
#include "Drawing/SceneManager.h"
#include "Drawing/SceneRegistry.h"
#include "Config.h" 
#include "Utility/I2CUtil.h"

// Instancias globales
DisplayModel u8g2(U8G2_R0, SCL_PIN, SDA_PIN);
Context ctx(u8g2); // Aquí se procesa toda la lógica

void setup()
{ 
  Serial.begin(115200);
  while(!Serial){
    delay(50);
  }
  
  I2CUtility::begin();
  I2CUtility::scan();
  
  // Inicialización del contexto de la app
  ctx.begin();
  
  // Registrar todas las escenas (SceneRegistry)
  registerAllScenes();

  // Carga la escena inicial (splash)
  SceneManager::setScene("splash", ctx);
}

const int targetMs = 50;
unsigned long int lastFrameMs = 0;
void loop()
{
  if(millis() - lastFrameMs < targetMs){
    return;
  }
  lastFrameMs = millis();
  // Actualizar el contexto de la app y la escena actual
  ctx.update();
  SceneManager::update(ctx);  
}
