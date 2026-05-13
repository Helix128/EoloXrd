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

// Librerías para display
#include <U8g2lib.h>

// Módulos principales
#include "Data/Context.h"
#include "Drawing/SceneManager.h"
#include "Drawing/SceneRegistry.h"
#include "Config.h" 
#include "Utility/RS485Monitor.h"
#include "Utility/RS485PatternValidator.h"
#include "Utility/DebugConsole.h"
#include "Profiler.h"

// Instancias globales
DisplayModel u8g2(U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN);
Context ctx(u8g2); // Aquí se procesa toda la lógica
DebugConsole debugConsole;

void setup()
{   
  pinMode(PPH_PWR_PIN,OUTPUT); // perifericos
  digitalWrite(PPH_PWR_PIN, HIGH); // Encender perifericos (I2C, display) lo antes posible
  pinMode(MODEM_PWR_PIN,OUTPUT); // modem
  
  ctx.components.motor.begin(); // apagar motores

  Serial.begin(115200);

#ifdef FEATURE_MODEM
  debugConsole.attachModem(&ctx.components.modem);
#endif

  RS485Monitor::getInstance(); // Inicializar monitor RS485
  LOG_LN("RS485 Monitor inicializado");

  /*
  I2CBus::getInstance().begin();
  I2CBus::getInstance().scan();
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
  debugConsole.poll();

  if(SceneManager::getSceneIndex()<=1){
    ctx.components.motor.setPowerPct(0);
  }

  if(millis() - lastFrameMs < targetMs){
    return;
  }

  frameStartMs = millis();
  lastFrameMs += targetMs;

  // Actualizar el contexto de la app y la escena actual
  bool externalDirty = ctx.update();
  SceneManager::update(ctx, externalDirty);

  unsigned long frameExecutionMs = millis() - frameStartMs;
  PROFILE_MARK("loop.frame", frameExecutionMs * 1000UL);
  RS485Monitor::getInstance().recordLoopFrameTime(frameExecutionMs);
  RS485Monitor::getInstance().checkAndReportViolations();

  // Validar patrones de uso
  RS485PatternValidator::getInstance().reportViolations();
}
