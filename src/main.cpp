// -- EOLO MP --
// Centro de Investigación en Tecnologías para la Sociedad (C+)
// Universidad del Desarrollo
// Diego Muñoz | Helix128
// https://github.com/Helix128/EoloXrd

#if defined(EOLO_TARGET_DRON)
  #pragma message("Compilando para EOLO Dron headless.")
#elif defined(EOLO_TARGET_STANDARD)
  #pragma message("Compilando para EOLO Standard.")
  #include "Board/Modem.h"
#elif defined(EOLO_TARGET_EXPRESS_LEGACY)
  #pragma message("Compilando para EOLO Express Legacy.")
#elif defined(EOLO_TARGET_EXPRESS)
  #pragma message("Compilando para EOLO Express.")
#else
  #error "Define un target EOLO_TARGET_* en platformio.ini"
#endif

#ifdef FEATURE_FLOW_AFM07
  #pragma message("Sensor de flujo: AFM07")
#elif defined(FEATURE_FLOW_FS3000)
  #pragma message("Sensor de flujo legacy: FS3000")
#endif

#include "Config.h"

#ifdef FEATURE_HEADLESS
#include <esp_sleep.h>
#include "Board/CaptureSwitches.h"
#if defined(EOLO_TARGET_DRON)
#include "Board/HeadlessSetupServer.h"
#include "Board/HeadlessSetupTypes.h"
#endif
#else
// Librerías para display
#include <U8g2lib.h>
#endif

// Módulos principales
#include "Data/Context.h"
#ifndef FEATURE_HEADLESS
#include "Drawing/SceneManager.h"
#include "Drawing/SceneRegistry.h"
#endif
#include "Utility/RS485Monitor.h"
#include "Utility/RS485PatternValidator.h"
#include "Utility/DebugConsole.h"
#include "Profiler.h"

// Instancias globales
#ifndef FEATURE_HEADLESS
DisplayModel u8g2(U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN);
Context ctx(u8g2); // Aquí se procesa toda la lógica
#else
Context ctx;
CaptureSwitches captureSwitches;
#if defined(EOLO_TARGET_DRON)
HeadlessSetupServer headlessSetupServer(ctx, captureSwitches);
#endif
#endif
DebugConsole debugConsole;

#ifndef FEATURE_HEADLESS
static void reinitDisplay() {
    u8g2.begin();
    u8g2.setBusClock(I2C_CLOCK);
}
#endif

#ifdef FEATURE_HEADLESS
enum class DroneBootState : uint8_t
{
  Idle,
  Setup,
  Waiting,
  Capturing,
  Finished
};

static DroneBootState droneState = DroneBootState::Idle;
static bool droneFinishHandled = false;

static void setDroneLed(StatusLedPattern pattern)
{
#ifdef FEATURE_NEOPIXEL
  ctx.components.statusLed.setPattern(pattern);
#else
  (void)pattern;
#endif
}

static void updateDroneStatusLed()
{
  if (droneState == DroneBootState::Capturing &&
      (ctx.sdStatus == SD_ERROR || ctx.sdStatus == SD_MISSING))
  {
    setDroneLed(StatusLedPattern::Error);
    return;
  }

  if (droneState == DroneBootState::Capturing && ctx.motorOverheatActive)
  {
    setDroneLed(StatusLedPattern::MotorOverheat);
    return;
  }

  if (droneState == DroneBootState::Capturing &&
      (ctx.logActive || ctx.uploadPending || ctx.uploadActive))
  {
    setDroneLed(StatusLedPattern::Busy);
    return;
  }

  switch (droneState)
  {
  case DroneBootState::Setup:
    setDroneLed(StatusLedPattern::Setup);
    break;
  case DroneBootState::Waiting:
    setDroneLed(StatusLedPattern::Waiting);
    break;
  case DroneBootState::Capturing:
    setDroneLed(StatusLedPattern::Capturing);
    break;
  case DroneBootState::Finished:
    setDroneLed(StatusLedPattern::Finished);
    break;
  case DroneBootState::Idle:
  default:
    setDroneLed(StatusLedPattern::Boot);
    break;
  }
}

static void startDroneConfiguredCapture(const HeadlessSetupConfig &config)
{
  DateTime now = ctx.components.rtc.now();
  HeadlessSetup::applyToSession(config, ctx.session, now);
  ctx.clearSession();
  ctx.saveSession();

  if (config.waitSeconds == 0)
  {
    LOG_LN("Drone: captura instantanea desde setup web.");
    ctx.beginCapture();
    droneState = DroneBootState::Capturing;
  }
  else
  {
    LOG_OUT("Drone: setup web confirmado; esperando ");
    LOG_OUT(config.waitSeconds);
    LOG_OUT_LN(" segundos antes de capturar.");
    droneState = DroneBootState::Waiting;
  }
  updateDroneStatusLed();
}

static void configureDroneCapture()
{
  captureSwitches.begin();
  delay(100);
  CaptureSwitchSnapshot switchSnapshot = captureSwitches.snapshot();
  CaptureSwitches::printSnapshot(stdOut, switchSnapshot);
  CaptureSwitchSelection selection = switchSnapshot.selection;

  if (HeadlessSetup::shouldEnterWebSetup(selection.waitCode))
  {
    LOG_LN("Switches de espera en Off; entrando a setup web de EOLO Dron.");
    ctx.components.motor.setPowerPct(0);
    headlessSetupServer.begin();
    droneState = DroneBootState::Setup;
    updateDroneStatusLed();
    return;
  }

  ctx.session.usePlantower = false;
  ctx.session.targetFlow = DRONE_TARGET_FLOW_LPM;

  if (!selection.shouldStart)
  {
    LOG_LN("Configuracion de switches indica idle; captura no iniciada.");
    ctx.components.motor.setPowerPct(0);
    droneState = DroneBootState::Idle;
    updateDroneStatusLed();
    return;
  }

  DateTime now = ctx.components.rtc.now();
  ctx.session.startDate = DateTime(now.unixtime() + selection.waitSeconds);
  ctx.session.duration = selection.durationSeconds;
  ctx.session.elapsedTime = 0;
  ctx.session.lastLog = 0;
  ctx.session.capturedVolume = 0.0f;
  ctx.clearSession();
  ctx.saveSession();

  if (selection.instantStart)
  {
    LOG_LN("Drone: captura instantanea.");
    ctx.beginCapture();
    droneState = DroneBootState::Capturing;
  }
  else
  {
    LOG_OUT("Drone: esperando ");
    LOG_OUT(selection.waitSeconds);
    LOG_OUT_LN(" segundos antes de capturar.");
    droneState = DroneBootState::Waiting;
  }
  updateDroneStatusLed();
}

static void updateDroneController()
{
  if (droneState == DroneBootState::Setup)
  {
    ctx.components.motor.setPowerPct(0);
    headlessSetupServer.handleClient();
    if (headlessSetupServer.confirmed())
    {
      HeadlessSetupConfig config = headlessSetupServer.confirmedConfig();
      headlessSetupServer.stop();
      startDroneConfiguredCapture(config);
    }
    updateDroneStatusLed();
    return;
  }

  if (droneState == DroneBootState::Waiting)
  {
    if (ctx.isHeadlessCalibrationRunning())
    {
      updateDroneStatusLed();
      return;
    }
    uint32_t now = ctx.getUnixTime();
    if (now >= ctx.session.startDate.unixtime())
    {
      LOG_LN("Drone: espera cumplida, iniciando captura.");
      ctx.beginCapture();
      droneState = DroneBootState::Capturing;
    }
    updateDroneStatusLed();
    return;
  }

  if (droneState == DroneBootState::Capturing && ctx.isEnd)
  {
    droneState = DroneBootState::Finished;
  }

  updateDroneStatusLed();

  if (droneState == DroneBootState::Finished && !droneFinishHandled)
  {
#ifdef FEATURE_MODEM
    if (!ctx.logsIdle() ||
        ctx.uploadPending || ctx.uploadActive ||
        ctx.components.modemService.pendingCount() > 0)
    {
      ctx.components.modemService.shutdownWhenIdle();
      return;
    }
    if (ctx.components.modemService.state() != ModemServiceState::Off)
    {
      ctx.components.modemService.shutdownNow();
      return;
    }
#endif
    droneFinishHandled = true;
    ctx.clearSession();
    ctx.components.motor.setPwmImmediate(0);
    setDroneLed(StatusLedPattern::Finished);
    ctx.components.statusLed.poll(true);
#ifdef STATUS_LED_LOW_POWER
    delay(140);
    setDroneLed(StatusLedPattern::Off);
    ctx.components.statusLed.poll(true);
#endif
    LOG_LN("Drone: captura finalizada; entrando en deep sleep hasta reset/power-cycle.");
    Serial.flush();
    esp_deep_sleep_start();
  }
}
#endif

void setup()
{   
#if PPH_PWR_PIN >= 0
  pinMode(PPH_PWR_PIN,OUTPUT); // perifericos
  digitalWrite(PPH_PWR_PIN, HIGH); // Encender perifericos (I2C, display) lo antes posible
#endif
#ifdef FEATURE_MODEM
#if MODEM_POWER_MODE == MODEM_POWER_ALWAYS_ON
  Modem::configurePowerPinOn();
#else
  Modem::configurePowerPinOff();
#endif
#else
#if MODEM_PWR_PIN >= 0
  pinMode(MODEM_PWR_PIN,OUTPUT); // modem
  digitalWrite(MODEM_PWR_PIN, LOW);
#endif
#endif
  
  ctx.components.motor.begin(); // apagar motores

  Serial.begin(115200);

#ifdef FEATURE_MODEM
  debugConsole.attachModemService(&ctx.components.modemService);
#endif
  debugConsole.attachRTC(&ctx.components.rtc);
#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)
  debugConsole.attachCaptureSwitches(&captureSwitches);
  debugConsole.attachDroneContext(&ctx);
#endif
#ifndef FEATURE_HEADLESS
  debugConsole.attachDisplayReinit(reinitDisplay);
#endif
  RS485Monitor::getInstance(); // Inicializar monitor RS485
  LOG_LN("RS485 Monitor inicializado");

  /*
  I2CBus::getInstance().begin();
  I2CBus::getInstance().scan();
  */

#ifndef FEATURE_HEADLESS
  // Registrar todas las escenas (SceneRegistry) 
  registerAllScenes();
#endif
  // Inicialización del contexto de la app
  ctx.begin();

#ifdef FEATURE_HEADLESS
  setDroneLed(StatusLedPattern::Boot);
  configureDroneCapture();
#else
  // Carga la escena inicial (splash)
  SceneManager::setScene("splash", ctx);  
#endif
}

const int targetMs = 8;
unsigned long int lastFrameMs = 0;
unsigned long int frameStartMs = 0;

void loop()
{ 
  debugConsole.poll();

#ifdef FEATURE_HEADLESS
  if (millis() - lastFrameMs < targetMs){
    return;
  }

  frameStartMs = millis();
  lastFrameMs += targetMs;

  ctx.update();
  updateDroneController();

  unsigned long frameExecutionMs = millis() - frameStartMs;
  PROFILE_MARK("loop.frame", frameExecutionMs * 1000UL);
  RS485Monitor::getInstance().recordLoopFrameTime(frameExecutionMs);
  RS485Monitor::getInstance().checkAndReportViolations();
  RS485PatternValidator::getInstance().reportViolations();
  return;
#else
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
#endif
}
