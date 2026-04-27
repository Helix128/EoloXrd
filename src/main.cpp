// -- EOLO MP --
// Centro de Investigación en Tecnologías para la Sociedad (C+)
// Universidad del Desarrollo
// Diego Muñoz | Helix128
// https://github.com/Helix128/EoloXrd

#ifdef FEATURE_MODEM
  #pragma message("Compilando para EOLO Standard (con Módem/Anemómetro).")
#else
  #pragma message("Compilando para EOLO Express.")
#endif

#ifdef FEATURE_FLOW_AFM07
  #pragma message("Sensor de flujo: AFM07")
#elif defined(FEATURE_FLOW_FS3000)
  #pragma message("Sensor de flujo legacy: FS3000")
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
#include "Utility/RS485Monitor.h"
#include "Utility/RS485PatternValidator.h"

// Instancia global del monitor (para que RS485 pueda acceder)
RS485Monitor* g_rs485Monitor = nullptr;

// Instancias globales
DisplayModel u8g2(U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN);
Context ctx(u8g2); // Aquí se procesa toda la lógica

void setup()
{   
  pinMode(PPH_PWR_PIN,OUTPUT); // perifericos
  digitalWrite(PPH_PWR_PIN, HIGH); // Encender perifericos (I2C, display) lo antes posible
  pinMode(MODEM_PWR_PIN,OUTPUT); // modem
  
  ctx.components.motor.begin(); // apagar motores

  Serial.begin(115200);

  // Inicializar monitor RS485
  g_rs485Monitor = &RS485Monitor::getInstance();
  LOG_LN("RS485 Monitor inicializado");

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
unsigned long int frameStartMs = 0;

void loop()
{ 
  if(SceneManager::getSceneIndex()<=1){
    ctx.components.motor.setPowerPct(0);
  }

  if(millis() - lastFrameMs < targetMs){
    return;
  }

  // Registrar inicio de frame para medir tiempo
  frameStartMs = millis();
  unsigned long frameMs = frameStartMs - lastFrameMs;
  
  lastFrameMs = frameStartMs;

  // Actualizar el contexto de la app y la escena actual
  ctx.update();
  SceneManager::update(ctx);

  // Registrar estadisticas de frame
  unsigned long frameExecutionMs = millis() - frameStartMs;
  if (g_rs485Monitor != nullptr) {
    g_rs485Monitor->recordLoopFrameTime(frameExecutionMs);
    g_rs485Monitor->checkAndReportViolations();
  }

  // Validar patrones de uso
  RS485PatternValidator::getInstance().reportViolations();
}
