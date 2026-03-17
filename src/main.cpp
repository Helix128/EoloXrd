// -- EOLO MP --
// Centro de Investigación en Tecnologías para la Sociedad (C+)
// Universidad del Desarrollo
// Diego Muñoz | Helix128
// https://github.com/Helix128/EoloXrd

#ifdef EOLO_GRANDE
  #pragma message("Compilando para EOLO grande.");
#else
  #pragma message("Compilando para EOLO pequeño.");
#endif

// Librerías para display y comunicación I2C
#include <U8g2lib.h>
#include "Wire.h"

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
  pinMode(PPH_PWR_PIN,OUTPUT); // perifericos
  pinMode(MODEM_PWR_PIN,OUTPUT); // modem
  
  Serial.begin(115200);

  /*
  I2CUtility::begin();
  I2CUtility::scan();
  */
  // Registrar todas las escenas (SceneRegistry)
  
  registerAllScenes();
  // Inicialización del contexto de la app
  ctx.begin();

  // Carga la escena inicial (splash)
  SceneManager::setScene("splash", ctx);  
}

const int targetMs = 16;
unsigned long int lastFrameMs = 0;
void loop()
{ 
  if(SceneManager::getSceneIndex()<=1){
    ctx.components.motor.setPowerPct(0);
  }

  if(millis() - lastFrameMs < targetMs){
    return;
  }

  lastFrameMs = millis();
  // Actualizar el contexto de la app y la escena actual
  ctx.update();
  SceneManager::update(ctx);  
}
