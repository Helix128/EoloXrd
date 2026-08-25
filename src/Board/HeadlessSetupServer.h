#ifndef HEADLESS_SETUP_SERVER_H
#define HEADLESS_SETUP_SERVER_H

#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)

#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <SD.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "CaptureSwitches.h"
#include "HeadlessSetupTypes.h"
#include "HeadlessSetupWebPage.h"
#include "../Data/Context.h"
#include "../Utility/SystemDiagnostics.h"

#ifndef HEADLESS_SETUP_AP_SSID
#define HEADLESS_SETUP_AP_SSID "eolo-dron"
#endif

#ifndef HEADLESS_SETUP_AP_PASSWORD
#define HEADLESS_SETUP_AP_PASSWORD "eolo-dron"
#endif

#ifndef HEADLESS_SETUP_STA_SSID
#define HEADLESS_SETUP_STA_SSID "udd-recicla"
#endif

#ifndef HEADLESS_SETUP_STA_PASSWORD
#define HEADLESS_SETUP_STA_PASSWORD "R3c1claj3#udd-2019"
#endif

enum class StaConnectionState : uint8_t
{
  Disabled,
  Connecting,
  Connected,
  Abandoned
};

class DiagnosticWebServer : public WebServer
{
public:
  explicit DiagnosticWebServer(int port) : WebServer(port) {}
  void beginRequest() { _lastResponseCode = 200; }
  int lastResponseCode() const { return _lastResponseCode; }
  void send(int code, const char *contentType = nullptr, const String &content = String(""))
  {
    _lastResponseCode = code;
    WebServer::send(code, contentType, content);
  }

private:
  int _lastResponseCode = 200;
};

class HeadlessSetupServer
{
public:
  static constexpr const char *ApSsid = HEADLESS_SETUP_AP_SSID;
  static constexpr const char *ApPassword = HEADLESS_SETUP_AP_PASSWORD;
  static constexpr const char *PortalHost = "eolo.setup";
  static constexpr const char *PortalUrl = "http://eolo.setup/";
  static constexpr const char *PortalIpUrl = "http://192.168.4.1/";
  static constexpr const char *MdnsHost = "eolo-dron";
  static constexpr uint16_t DnsPort = 53;

  explicit HeadlessSetupServer(Context &context, CaptureSwitches &switches)
      : _ctx(context), _switches(switches), _server(80)
  {
  }

  void begin()
  {
    if (_running)
      return;

    _confirmed = false;
    loadDefaults();

    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_AP_STA);
    IPAddress apIP(192, 168, 4, 1);
    const bool apConfigured = WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    const bool apStarted = apConfigured && WiFi.softAP(ApSsid, ApPassword);
    if (!apStarted) {
      LOG_LN("ERROR: no se pudo iniciar el AP eolo-dron");
      WiFi.mode(WIFI_AP);
      return;
    }
    _dnsServer.start(DnsPort, "*", WiFi.softAPIP());

    _wifiEventId = WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
      if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        _staAttempts = 0;
        _lastDisconnectReason = 0;
      } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        _lastDisconnectReason = info.wifi_sta_disconnected.reason;
        LOG_F("Wi-Fi LAN desconectada: motivo=%u; AP activo con %u cliente(s)\n",
              _lastDisconnectReason, WiFi.softAPgetStationNum());
        if (_mdnsRunning) { MDNS.end(); _mdnsRunning = false; }
        if (_staState != StaConnectionState::Disabled) {
          _staState = StaConnectionState::Connecting;
          _staStartMs = millis();
          scheduleStaRetry();
        }
      }
    });

    // Cargar credenciales de red local (Preferences o macro en compilacion)
    char staSsid[33] = "";
    char staPass[65] = "";
    Preferences wifiPrefs;
    if (wifiPrefs.begin("eolo_wifi", false))
    {
      if (wifiPrefs.isKey("ssid"))
      {
        wifiPrefs.getString("ssid", staSsid, sizeof(staSsid));
        wifiPrefs.getString("pass", staPass, sizeof(staPass));
      }
      wifiPrefs.end();
    }

    if (staSsid[0] == '\0' && strlen(HEADLESS_SETUP_STA_SSID) > 0)
    {
      strlcpy(staSsid, HEADLESS_SETUP_STA_SSID, sizeof(staSsid));
      strlcpy(staPass, HEADLESS_SETUP_STA_PASSWORD, sizeof(staPass));
    }

    if (staSsid[0] != '\0')
    {
      strlcpy(_staSsid, staSsid, sizeof(_staSsid));
      strlcpy(_staPass, staPass, sizeof(_staPass));
      LOG_OUT("Intentando conexion a Wi-Fi local: ");
      LOG_OUT_LN(staSsid);
      WiFi.begin(staSsid, staPass);
      _staState = StaConnectionState::Connecting;
      _staStartMs = millis();
      _staAttempts = 1;
    }
    else
    {
      WiFi.mode(WIFI_AP);
      _staState = StaConnectionState::Disabled;
    }

    route("/", HTTP_GET, [this]() { handleRoot(); });
    route("/healthz", HTTP_GET, [this]() { handleHealthz(); });
    route("/api/diagnostics", HTTP_GET, [this]() { handleDiagnostics(); });
    route("/api/status", HTTP_GET, [this]() { handleStatus(); });
    route("/api/logs", HTTP_GET, [this]() { handleLogs(); });
    route("/api/logs/preview", HTTP_GET, [this]() { handlePreview(); });
    route("/download", HTTP_GET, [this]() { handleDownload(); });
    route("/api/logs/delete", HTTP_POST, [this]() { handleLogDelete(); });
    route("/api/confirm", HTTP_POST, [this]() { handleConfirm(); });
    route("/api/presets", HTTP_GET, [this]() { handlePresets(); });
    route("/api/presets/load", HTTP_GET, [this]() { handlePresetLoad(); });
    route("/api/presets/save", HTTP_POST, [this]() { handlePresetSave(); });
    route("/api/presets/delete", HTTP_POST, [this]() { handlePresetDelete(); });
    route("/api/motor/ignite", HTTP_POST, [this]() { handleIgnite(); });
    route("/api/debug/enter", HTTP_POST, [this]() { handleDebugEnter(); });
    route("/api/debug/pwm", HTTP_POST, [this]() { handleDebugPwm(); });
    route("/api/debug/status", HTTP_GET, [this]() { handleDebugStatus(); });
    route("/favicon.ico", HTTP_GET, [this]() {
      _server.send(204, "image/x-icon", "");
    });
    registerCaptivePortalEndpoints();
    _server.onNotFound([this]() {
      _server.beginRequest();
      SystemDiagnostics::instance().httpBegin(_server.uri());
      handleNotFound();
      SystemDiagnostics::instance().httpEnd(_server.lastResponseCode());
    });
    _server.begin();
    _running = true;

    LOG_OUT("Setup Wi-Fi headless activo: ");
    LOG_OUT(ApSsid);
    LOG_OUT(" / ");
    LOG_OUT_LN(ApPassword);
    LOG_OUT("URL setup: ");
    LOG_OUT_LN(PortalUrl);
    LOG_OUT("IP setup: ");
    LOG_OUT_LN(WiFi.softAPIP());
    LOG_LN("LAN (si conecta): IP asignada o http://eolo-dron.local/");
  }

  void pollStaConnection()
  {
    if (_staState != StaConnectionState::Connecting)
      return;

    if (WiFi.status() == WL_CONNECTED)
    {
      _staState = StaConnectionState::Connected;
      LOG_OUT("Wi-Fi local conectada! IP asignada: ");
      LOG_OUT_LN(WiFi.localIP());
      if (!_mdnsRunning && MDNS.begin(MdnsHost))
      {
        _mdnsRunning = true;
        MDNS.addService("http", "tcp", 80);
        LOG_LN("mDNS activo: http://eolo-dron.local/");
      }
    }
    else if (millis() - _staStartMs >= kStaTimeoutMs &&
             (int32_t)(millis() - _nextStaRetryMs) >= 0)
    {
      if (WiFi.softAPgetStationNum() > 0) {
        _nextStaRetryMs = millis() + 10000UL;
        return;
      }
      if (_staAttempts >= kMaxStaAttempts) {
        _staState = StaConnectionState::Abandoned;
        WiFi.disconnect(false, false);
        LOG_LN("Wi-Fi LAN no disponible; AP sigue activo. Reintentos agotados.");
        return;
      }
      ++_staAttempts;
      LOG_F("Reintento Wi-Fi LAN %u/%u; AP permanece activo\n", _staAttempts, kMaxStaAttempts);
      WiFi.disconnect(false, false);
      WiFi.begin(_staSsid, _staPass);
      _staStartMs = millis();
      scheduleStaRetry();
    }
  }

  void handleClient()
  {
    if (!_running)
      return;

    pollStaConnection();
    _dnsServer.processNextRequest();
    _server.handleClient();
  }

  void stop()
  {
    if (!_running)
      return;

    _server.stop();
    _dnsServer.stop();
    if (_staState == StaConnectionState::Connected)
    {
      MDNS.end();
    }
    WiFi.disconnect(true, true);
    WiFi.removeEvent(_wifiEventId);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    _running = false;
    _staState = StaConnectionState::Disabled;
  }

  bool confirmed() const { return _confirmed; }
  const HeadlessSetupConfig &confirmedConfig() const { return _confirmedConfig; }
  bool debugModeActive() const { return _debugMode; }

private:
  Context &_ctx;
  CaptureSwitches &_switches;
  DiagnosticWebServer _server;
  DNSServer _dnsServer;
  HeadlessSetupConfig _defaults;
  HeadlessSetupConfig _confirmedConfig;
  StaConnectionState _staState = StaConnectionState::Disabled;
  unsigned long _staStartMs = 0;
  static constexpr unsigned long kStaTimeoutMs = 3500UL;
  bool _running = false;
  bool _confirmed = false;
  bool _debugMode = false;
  bool _mdnsRunning = false;
  wifi_event_id_t _wifiEventId = 0;
  char _staSsid[33] = {};
  char _staPass[65] = {};
  uint8_t _staAttempts = 0;
  uint8_t _lastDisconnectReason = 0;
  unsigned long _nextStaRetryMs = 0;
  static constexpr uint8_t kMaxStaAttempts = 5;

  void scheduleStaRetry()
  {
    uint8_t shift = _staAttempts > 4 ? 4 : _staAttempts;
    _nextStaRetryMs = millis() + (1000UL << shift);
  }

  template <typename Handler>
  void route(const char *uri, HTTPMethod method, Handler handler)
  {
    _server.on(uri, method, [this, handler]() {
      _server.beginRequest();
      SystemDiagnostics::instance().httpBegin(_server.uri());
      handler();
      SystemDiagnostics::instance().httpEnd(_server.lastResponseCode());
    });
  }

  static const char *sdStatusText(SDStatus status)
  {
    switch (status)
    {
    case SD_OK:
      return "ok";
    case SD_WRITING:
      return "writing";
    case SD_MISSING:
      return "missing";
    case SD_ERROR:
    default:
      return "error";
    }
  }

  void loadDefaults()
  {
    Preferences prefs;
    prefs.begin("eolo_headless", false);
    _defaults.waitSeconds = prefs.getUInt("wait", 0);
    _defaults.durationSeconds = prefs.getUInt("duration", 5UL * MINUTE);
    _defaults.targetFlow = prefs.isKey("flow") ? prefs.getFloat("flow") : DRONE_TARGET_FLOW_LPM;
    _defaults.flowSectionCount = prefs.getUChar("flowSecCount", 0);
    if (_defaults.flowSectionCount > MaxFlowSections)
      _defaults.flowSectionCount = 0;
    for (uint8_t i = 0; i < MaxFlowSections; i++)
    {
      char durKey[12];
      char flowKey[12];
      snprintf(durKey, sizeof(durKey), "secDur%u", (unsigned int)i);
      snprintf(flowKey, sizeof(flowKey), "secFlow%u", (unsigned int)i);
      _defaults.flowSections[i].durationSeconds = prefs.getUInt(durKey, 0);
      _defaults.flowSections[i].targetFlow = prefs.isKey(flowKey) ? prefs.getFloat(flowKey) : DRONE_TARGET_FLOW_LPM;
    }
    prefs.end();

    if (!HeadlessSetup::validateConfig(_defaults))
      _defaults = HeadlessSetupConfig();
  }

  void saveDefaults(const HeadlessSetupConfig &config)
  {
    Preferences prefs;
    prefs.begin("eolo_headless", false);
    prefs.putUInt("wait", config.waitSeconds);
    prefs.putUInt("duration", config.durationSeconds);
    prefs.putFloat("flow", config.targetFlow);
    prefs.putUChar("flowSecCount", config.flowSectionCount);
    for (uint8_t i = 0; i < MaxFlowSections; i++)
    {
      char durKey[12];
      char flowKey[12];
      snprintf(durKey, sizeof(durKey), "secDur%u", (unsigned int)i);
      snprintf(flowKey, sizeof(flowKey), "secFlow%u", (unsigned int)i);
      prefs.putUInt(durKey, i < config.flowSectionCount ? config.flowSections[i].durationSeconds : 0);
      prefs.putFloat(flowKey, i < config.flowSectionCount ? config.flowSections[i].targetFlow : DRONE_TARGET_FLOW_LPM);
    }
    prefs.end();
  }

  bool parseFlowSchedule(HeadlessSetupConfig &config)
  {
    config.flowSectionCount = 0;
    for (uint8_t i = 0; i < MaxFlowSections; i++)
      config.flowSections[i] = FlowSection();

    if (!_server.hasArg("flowSchedule"))
      return true;

    String raw = _server.arg("flowSchedule");
    if (raw.length() == 0 || raw == "[]")
      return true;

    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, raw);
    if (err || !doc.is<JsonArray>())
      return false;

    JsonArray arr = doc.as<JsonArray>();
    if (arr.size() > MaxFlowSections)
      return false;

    uint8_t index = 0;
    for (JsonVariant item : arr)
    {
      config.flowSections[index].durationSeconds = item["durationSeconds"] | 0;
      config.flowSections[index].targetFlow = item["targetFlow"] | NAN;
      index++;
    }
    config.flowSectionCount = index;
    return true;
  }

  bool parseConfigFromRequest(HeadlessSetupConfig &config)
  {
    if (!_server.hasArg("waitSeconds") || !_server.hasArg("targetFlow"))
      return false;

    config = HeadlessSetupConfig();
    config.waitSeconds = _server.arg("waitSeconds").toInt();
    config.targetFlow = _server.arg("targetFlow").toFloat();

    if (_server.hasArg("durationMode") && _server.arg("durationMode") == "infinite") {
      config.durationSeconds = DRONE_DURATION_INFINITE;
    } else {
      if (!_server.hasArg("durationSeconds"))
        return false;
      config.durationSeconds = _server.arg("durationSeconds").toInt();
    }

    if (!parseFlowSchedule(config))
      return false;

    return HeadlessSetup::validateConfig(config);
  }

  void redirectToPortal()
  {
    _server.sendHeader("Location", PortalUrl, true);
    _server.send(302, "text/plain", "Redirecting...");
  }

  void registerCaptivePortalEndpoints()
  {
    route("/generate_204", HTTP_GET, [this]() { redirectToPortal(); });
    route("/gen_204", HTTP_GET, [this]() { redirectToPortal(); });
    route("/mobile/status.php", HTTP_GET, [this]() { redirectToPortal(); });
    route("/hotspot-detect.html", HTTP_GET, [this]() { redirectToPortal(); });
    route("/library/test/success.html", HTTP_GET, [this]() { redirectToPortal(); });
    route("/success.txt", HTTP_GET, [this]() { redirectToPortal(); });
    route("/connecttest.txt", HTTP_GET, [this]() { redirectToPortal(); });
    route("/ncsi.txt", HTTP_GET, [this]() { redirectToPortal(); });
    route("/fwlink", HTTP_GET, [this]() { redirectToPortal(); });
  }

  void handleNotFound()
  {
    if (_server.uri().startsWith("/api/"))
    {
      _server.send(404, "application/json", "{\"error\":\"not_found\"}");
      return;
    }

    // Suppress browser background request noise and redirect loops
    if (_server.uri() == "/favicon.ico" || _server.uri().endsWith(".png") || _server.uri().endsWith(".jpg"))
    {
      _server.send(404, "text/plain", "Not Found");
      return;
    }

    String host = _server.hostHeader();
    if (host.length() > 0)
    {
      if (host.equals("192.168.4.1") || host.equals("eolo.setup") ||
          host.equals("eolo-dron.local") || host.startsWith("eolo-dron.local:") ||
          (_staState == StaConnectionState::Connected && (host.equals(WiFi.localIP().toString()) || host.startsWith(WiFi.localIP().toString() + ":"))))
      {
        _server.send(404, "text/plain", "Not Found");
        return;
      }
      redirectToPortal();
      return;
    }

    _server.send(404, "text/plain", "Not Found");
  }

  void handleRoot()
  {
    _server.sendHeader("Cache-Control", "no-store");
    _server.sendHeader("Content-Encoding", "gzip");
    _server.send_P(200, "text/html; charset=utf-8", reinterpret_cast<PGM_P>(kHeadlessSetupHtmlGzip), kHeadlessSetupHtmlGzipSize);
  }

  void handleHealthz()
  {
    SystemDiagnostics::Snapshot snap = SystemDiagnostics::instance().snapshot();
    char response[192];
    snprintf(response, sizeof(response),
             "{\"ok\":true,\"uptimeMs\":%lu,\"loopHeartbeat\":%lu,\"heap\":%lu}",
             (unsigned long)snap.uptimeMs, (unsigned long)snap.loopHeartbeat,
             (unsigned long)snap.freeHeap);
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", response);
  }

  void handleDiagnostics()
  {
    SystemDiagnostics::Snapshot snap = SystemDiagnostics::instance().snapshot();
    I2CBus &bus = I2CBus::getInstance();
    I2CBus::Stats i2c = bus.getStats();
    LogIndexService::ReconcileSummary index = _ctx.logIndexSummary();
    DynamicJsonDocument doc(4096);
    doc["uptimeMs"] = snap.uptimeMs;
    doc["core"] = snap.core;
    doc["phase"] = snap.phase;
    JsonObject loop = doc.createNestedObject("loop");
    loop["heartbeat"] = snap.loopHeartbeat;
    loop["lastDurationMs"] = snap.loopLastDurationMs;
    loop["maxPauseMs"] = snap.loopMaxPauseMs;
    JsonObject tasks = doc.createNestedObject("tasks");
    tasks["i2cHeartbeat"] = snap.i2cHeartbeat;
    tasks["i2cStackFreeWords"] = snap.i2cStackWords;
    tasks["rs485Heartbeat"] = snap.rs485Heartbeat;
    tasks["rs485StackFreeWords"] = snap.rs485StackWords;
    JsonObject heap = doc.createNestedObject("heap");
    heap["free"] = snap.freeHeap;
    heap["minimum"] = snap.minFreeHeap;
    JsonObject http = doc.createNestedObject("http");
    http["lastRequest"] = snap.lastRequest;
    http["lastDurationMs"] = snap.httpLastDurationMs;
    http["lastCode"] = snap.httpLastCode;
    http["slow"] = snap.httpSlow;
    http["failed"] = snap.httpFailed;
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["apActive"] = WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA;
    wifi["apIp"] = WiFi.softAPIP().toString();
    wifi["apClients"] = WiFi.softAPgetStationNum();
    wifi["channel"] = WiFi.channel();
    wifi["staConnected"] = WiFi.status() == WL_CONNECTED;
    wifi["staIp"] = WiFi.localIP().toString();
    wifi["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    wifi["lastDisconnectReason"] = _lastDisconnectReason;
    wifi["attempts"] = _staAttempts;
    JsonObject sd = doc.createNestedObject("sd");
    sd["ready"] = _ctx.isSdReady();
    sd["status"] = sdStatusText(_ctx.sdStatus());
    JsonObject reconciliation = sd.createNestedObject("reconciliation");
    reconciliation["examined"] = index.examined;
    reconciliation["current"] = index.current;
    reconciliation["recovered"] = index.recovered;
    reconciliation["incompatible"] = index.incompatible;
    reconciliation["errors"] = index.errors;
    JsonObject i2cJson = doc.createNestedObject("i2c");
    i2cJson["ready"] = bus.isReady();
    i2cJson["transactions"] = i2c.transactions;
    i2cJson["failures"] = i2c.failures;
    i2cJson["recoveries"] = i2c.recoveries;
    i2cJson["lastResult"] = I2CBus::resultName(i2c.lastResult);
    i2cJson["bmeFailure"] = _ctx.components.bme.lastInitFailureName();
    JsonArray addresses = i2cJson.createNestedArray("addresses");
    for (uint8_t address : {uint8_t(ATTINY_ADDRESS), uint8_t(0x0A), uint8_t(0x68), uint8_t(0x76), uint8_t(0x77)}) {
      I2CBus::AddressStats stats = bus.getAddressStats(address);
      JsonObject item = addresses.createNestedObject();
      item["address"] = address;
      item["observed"] = stats.observed;
      item["result"] = I2CBus::resultName(stats.lastResult);
      item["failures"] = stats.consecutiveFailures;
    }
    JsonObject app = doc.createNestedObject("application");
    app["bootComplete"] = _ctx.bootInitComplete.load();
    app["capturing"] = _ctx.isCaptureActive();
    app["logActive"] = _ctx.isLogActive();
    app["uploadActive"] = _ctx.isUploadActive();
    String response;
    response.reserve(measureJson(doc) + 1);
    serializeJson(doc, response);
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", response);
  }

  void handleDebugEnter()
  {
    _debugMode = true;
    LOG_LN("Debug mode activado via web.");
    _server.send(200, "application/json", "{\"ok\":true}");
  }

  void handleDebugPwm()
  {
    if (!_debugMode)
    {
      _server.send(403, "application/json", "{\"ok\":false,\"error\":\"not_in_debug\"}");
      return;
    }

    int pwm = 0;
    if (_server.hasArg("pct"))
    {
      float pct = _server.arg("pct").toFloat();
      if (pct < 0.0f) pct = 0.0f;
      if (pct > 100.0f) pct = 100.0f;
      pwm = static_cast<int>((MAX_PWM * pct) / 100.0f);
    }
    else if (_server.hasArg("pwm"))
    {
      pwm = _server.arg("pwm").toInt();
      if (pwm < 0) pwm = 0;
      if (pwm > MAX_PWM) pwm = MAX_PWM;
    }
    else
    {
      _server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing_param\"}");
      return;
    }

    _ctx.components.motor.setPwmImmediate(pwm);
    LOG_OUT("Debug PWM: ");
    LOG_OUT_LN(pwm);
    _server.send(200, "application/json", "{\"ok\":true}");
  }

  void handleDebugStatus()
  {
    StaticJsonDocument<512> doc;
    doc["debugMode"] = _debugMode;
    doc["maxPwm"] = MAX_PWM;

    int currentPwm = _ctx.components.motor.getMotorPwm(0);
    doc["pwm"] = currentPwm;
    doc["pct"] = (MAX_PWM > 0) ? (currentPwm * 100.0f / MAX_PWM) : 0.0f;

#if defined(FEATURE_FLOW_AFM07) || defined(FEATURE_FLOW_FS3000)
    FlowData flowData;
    bool flowValid = _ctx.components.flowSensor.getData(flowData) && flowData.valid;
    JsonObject flow = doc.createNestedObject("flow");
    flow["valid"] = flowValid;
    flow["lpm"] = flowValid ? flowData.flow : 0.0f;
    flow["ageMs"] = flowData.ageMs;
#endif

    doc["motorTempValid"] = _ctx.isMotorThermalSensorValid();
    doc["motorTemp"] = _ctx.motorThermalTemperatureC();
    doc["overheat"] = _ctx.isMotorOverheatActive();

    size_t needed = measureJson(doc) + 1;
    char *buf = (char *)malloc(needed);
    if (buf) {
      serializeJson(doc, buf, needed);
      _server.send(200, "application/json", buf);
      free(buf);
    } else {
      _server.send(503, "application/json", "{\"error\":\"memory\"}");
    }
  }

  void handleIgnite()
  {
    if (!_debugMode)
    {
      _server.send(403, "application/json", "{\"ok\":false,\"error\":\"not_in_debug\"}");
      return;
    }

    int kickPwm = FLOW_PID_KICK_PWM;
    if (kickPwm <= 0) kickPwm = static_cast<int>(MAX_PWM * 0.80f);
    if (kickPwm > MAX_PWM) kickPwm = MAX_PWM;

    unsigned long kickMs = FLOW_PID_KICK_MS;
    if (kickMs == 0) kickMs = 300;

    LOG_OUT("Ejecutando pulso de arranque diagnostico: PWM ");
    LOG_OUT(kickPwm);
    LOG_OUT(" durante ");
    LOG_OUT(kickMs);
    LOG_OUT_LN(" ms");

    _ctx.components.motor.setPwmImmediate(kickPwm);
    delay(kickMs);
    _ctx.components.motor.setPwmImmediate(0);

#if defined(FEATURE_FLOW_PID)
    _ctx.motorCapture.forceIgnition();
#endif

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"kickPwm\":%d,\"durationMs\":%lu}", kickPwm, kickMs);
    _server.send(200, "application/json", resp);
  }

  void handleStatus()
  {
    CaptureSwitchSnapshot snap = _switches.snapshot();
    StaticJsonDocument<2048> doc;
    doc["sdReady"] = _ctx.isSdReady();
    doc["sdStatus"] = sdStatusText(_ctx.sdStatus());
    doc["rtc"] = _ctx.components.rtc.now().timestamp();
    doc["staConnected"] = (_staState == StaConnectionState::Connected);
    if (_staState == StaConnectionState::Connected)
    {
      doc["staIp"] = WiFi.localIP().toString();
    }

    JsonObject defaults = doc.createNestedObject("defaults");
    defaults["waitSeconds"] = _defaults.waitSeconds;
    defaults["durationSeconds"] = _defaults.durationSeconds;
    defaults["targetFlow"] = _defaults.targetFlow;
    defaults["flowSectionCount"] = _defaults.flowSectionCount;
    JsonArray sections = defaults.createNestedArray("flowSections");
    for (uint8_t i = 0; i < _defaults.flowSectionCount; i++)
    {
      JsonObject section = sections.createNestedObject();
      section["durationSeconds"] = _defaults.flowSections[i].durationSeconds;
      section["targetFlow"] = _defaults.flowSections[i].targetFlow;
    }

    JsonObject switches = doc.createNestedObject("switches");
    switches["waitCode"] = snap.waitCode;
    switches["durationCode"] = snap.durationCode;
    switches["wait"] = CaptureSwitches::waitDescription(snap.waitCode);
    switches["duration"] = CaptureSwitches::durationDescription(snap.durationCode);

#if defined(FEATURE_FLOW_PID)
    FlowPidStatus pidSt = _ctx.motorCapture.getPidStatus();
    JsonObject motor = doc.createNestedObject("motor");
    motor["ignitionPhase"] = FlowMotorController::ignitionPhaseText(pidSt.ignitionPhase);
    motor["kickActive"] = pidSt.kickActive;
    motor["kickCount"] = pidSt.kickCount;
    motor["stallDetected"] = pidSt.stallDetected;
    motor["pwm"] = pidSt.pwm;
#endif

    size_t needed = measureJson(doc) + 1;
    char *buf = (char *)malloc(needed);
    if (buf) {
      serializeJson(doc, buf, needed);
      _server.send(200, "application/json", buf);
      free(buf);
    } else {
      _server.send(503, "application/json", "{\"error\":\"memory\"}");
    }
  }

  void handleLogs()
  {
    if (!_ctx.isSdReady())
    {
      _server.send(503, "application/json", "{\"available\":false,\"files\":[]}");
      return;
    }

    File dir = SD.open(_ctx.logsDirectory());
    if (!dir || !dir.isDirectory())
    {
      _server.send(200, "application/json", "{\"available\":true,\"files\":[]}");
      return;
    }

    _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    _server.send(200, "application/json", "");
    _server.sendContent("{\"available\":true,\"files\":[");

    bool first = true;
    File file = dir.openNextFile();
    while (file)
    {
      SystemDiagnostics::instance().feedWatchdog();
      delay(0);
      const char *rawName = file.name();
      const char *base = rawName;
      const char *slash = strrchr(rawName, '/');
      if (slash) base = slash + 1;

      if (!file.isDirectory() && HeadlessSetup::isSafeLogBasename(base))
      {
        if (!first)
        {
          _server.sendContent(",");
        }
        first = false;

        char fileJson[256];
        snprintf(fileJson, sizeof(fileJson), "{\"name\":\"%s\",\"size\":%u}", base, (uint32_t)file.size());
        _server.sendContent(fileJson);
      }
      file.close();
      file = dir.openNextFile();
    }
    dir.close();

    _server.sendContent("]}");
    _server.sendContent(""); // end chunked stream
  }

  // Rellena el buffer 'out' con la ruta segura del log. Retorna true si el nombre de archivo es válido.
  bool safeLogPathFromRequest(char *out, size_t outLen)
  {
    // Note: WebServer::arg returns a String; fetch minimally and use c_str().
    String name = _server.arg("file");
    if (!HeadlessSetup::isSafeLogBasename(name.c_str()))
      return false;
    // compose path: logs directory + '/' + name
    int ret = snprintf(out, outLen, "%s/%s", _ctx.logsDirectory(), name.c_str());
    return ret > 0 && (size_t)ret < outLen;
  }

  void handlePreview()
  {
    if (!_ctx.isSdReady())
    {
      _server.send(503, "application/json", "{\"error\":\"sd_unavailable\"}");
      return;
    }

    char pathBuf[256];
    if (!safeLogPathFromRequest(pathBuf, sizeof(pathBuf)) || !SD.exists(pathBuf))
    {
      _server.send(404, "application/json", "{\"error\":\"file_not_found\"}");
      return;
    }

    File file = SD.open(pathBuf, FILE_READ);
    if (!file)
    {
      _server.send(500, "application/json", "{\"error\":\"open_failed\"}");
      return;
    }

    char headerBuf[256];
    size_t hlen = file.readBytesUntil('\n', headerBuf, sizeof(headerBuf) - 1);
    headerBuf[hlen] = '\0';
    // trim CR
    if (hlen > 0 && headerBuf[hlen - 1] == '\r') headerBuf[hlen - 1] = '\0';

    const uint32_t size = file.size();
    if (size > 4096)
    {
      file.seek(size - 4096);
      // discard until next line, safely reading without overwriting headerBuf
      while (file.available() && file.read() != '\n')
      {
        // just consume
      }
    }

    // Keep only the file offsets for the last rows.  The previous static
    // 40 x 256-byte matrix consumed 10 KiB of BSS permanently even when the
    // setup portal was inactive.  A single reusable row buffer is enough to
    // stream the same response from SD.
    constexpr size_t kPreviewRows = 40;
    constexpr size_t kPreviewRowMax = 256;
    uint32_t rowOffsets[kPreviewRows];
    char rowBuf[kPreviewRowMax];
    size_t count = 0;
    while (file.available())
    {
      SystemDiagnostics::instance().feedWatchdog();
      delay(0);
      const uint32_t rowOffset = file.position();
      size_t rlen = file.readBytesUntil('\n', rowBuf, sizeof(rowBuf) - 1);
      rowBuf[rlen] = '\0';
      // trim CR
      if (rlen > 0 && rowBuf[rlen - 1] == '\r') rowBuf[rlen - 1] = '\0';
      if (rlen == 0 || strcmp(rowBuf, headerBuf) == 0)
      {
        // ignore
      }
      else
      {
        rowOffsets[count % kPreviewRows] = rowOffset;
        count++;
      }
    }

    const size_t start = count > kPreviewRows ? count % kPreviewRows : 0;
    const size_t total = count > kPreviewRows ? kPreviewRows : count;

    // Stream the preview output to save heap memory
    _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    _server.send(200, "application/json", "");
    _server.sendContent("{\"header\":\"");
    _server.sendContent(headerBuf);
    _server.sendContent("\",\"rows\":[");

    for (size_t i = 0; i < total; i++)
    {
      if (i > 0)
      {
        _server.sendContent(",");
      }
      _server.sendContent("\"");
      const uint32_t rowOffset = rowOffsets[(start + i) % kPreviewRows];
      if (file.seek(rowOffset))
      {
        size_t rlen = file.readBytesUntil('\n', rowBuf, sizeof(rowBuf) - 1);
        rowBuf[rlen] = '\0';
        if (rlen > 0 && rowBuf[rlen - 1] == '\r') rowBuf[rlen - 1] = '\0';
        _server.sendContent(rowBuf);
      }
      _server.sendContent("\"");
    }

    _server.sendContent("]}");
    _server.sendContent(""); // end chunked stream
    file.close();
  }

  void handleDownload()
  {
    if (!_ctx.isSdReady())
    {
      _server.send(503, "text/plain", "SD no disponible");
      return;
    }

    String name = _server.arg("file");
    char pathBuf[256];
    if (!safeLogPathFromRequest(pathBuf, sizeof(pathBuf)) || !SD.exists(pathBuf))
    {
      _server.send(404, "text/plain", "Archivo no encontrado");
      return;
    }

    File file = SD.open(pathBuf, FILE_READ);
    if (!file)
    {
      _server.send(500, "text/plain", "No se pudo abrir el archivo");
      return;
    }

    char disp[320];
    snprintf(disp, sizeof(disp), "attachment; filename=\"%s\"", name.c_str());
    _server.sendHeader("Content-Disposition", disp);
    _server.setContentLength(file.size());
    _server.send(200, "text/csv", "");
    WiFiClient client = _server.client();
    uint8_t buffer[1024];
    while (file.available() && client.connected()) {
      size_t count = file.read(buffer, sizeof(buffer));
      if (count == 0) break;
      size_t offset = 0;
      while (offset < count && client.connected()) {
        size_t written = client.write(buffer + offset, count - offset);
        if (written == 0) break;
        offset += written;
        SystemDiagnostics::instance().feedWatchdog();
        delay(0);
      }
      if (offset != count) break;
    }
    file.close();
  }

  void handleLogDelete()
  {
    if (!_ctx.isSdReady())
    {
      _server.send(503, "application/json", "{\"ok\":false,\"error\":\"sd_unavailable\"}");
      return;
    }

    char pathBuf[256];
    if (!safeLogPathFromRequest(pathBuf, sizeof(pathBuf)))
    {
      _server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_name\"}");
      return;
    }

    if (!SD.exists(pathBuf))
    {
      _server.send(404, "application/json", "{\"ok\":false,\"error\":\"file_not_found\"}");
      return;
    }

    String name = _server.arg("file");
    if (_ctx.removeLogAndIndex(name.c_str()))
    {
      _server.send(200, "application/json", "{\"ok\":true}");
    }
    else
    {
      _server.send(500, "application/json", "{\"ok\":false,\"error\":\"delete_failed\"}");
    }
  }

  void addConfigJson(JsonObject obj, const HeadlessSetupConfig &config)
  {
    obj["waitSeconds"] = config.waitSeconds;
    obj["durationSeconds"] = config.durationSeconds;
    obj["targetFlow"] = config.targetFlow;
    obj["flowSectionCount"] = config.flowSectionCount;
    JsonArray sections = obj.createNestedArray("flowSections");
    for (uint8_t i = 0; i < config.flowSectionCount; i++)
    {
      JsonObject section = sections.createNestedObject();
      section["durationSeconds"] = config.flowSections[i].durationSeconds;
      section["targetFlow"] = config.flowSections[i].targetFlow;
    }
  }

  String presetKey(const char *prefix, uint8_t index, int section = -1)
  {
    if (section >= 0)
      return String(prefix) + String(index) + "_" + String(section);
    return String(prefix) + String(index);
  }

  void loadPresetSlot(Preferences &prefs, uint8_t slot, HeadlessSetupPreset &preset)
  {
    String name = prefs.isKey(presetKey("name", slot).c_str()) ? prefs.getString(presetKey("name", slot).c_str()) : "";
    strlcpy(preset.name, name.c_str(), sizeof(preset.name));
    preset.config.waitSeconds = prefs.getUInt(presetKey("wait", slot).c_str(), 0);
    preset.config.durationSeconds = prefs.getUInt(presetKey("duration", slot).c_str(), 5UL * MINUTE);
    preset.config.targetFlow = prefs.isKey(presetKey("flow", slot).c_str()) ? prefs.getFloat(presetKey("flow", slot).c_str()) : DRONE_TARGET_FLOW_LPM;
    preset.config.flowSectionCount = prefs.getUChar(presetKey("secCount", slot).c_str(), 0);
    if (preset.config.flowSectionCount > MaxFlowSections)
      preset.config.flowSectionCount = 0;
    for (uint8_t i = 0; i < MaxFlowSections; i++)
    {
      preset.config.flowSections[i].durationSeconds = prefs.getUInt(presetKey("secDur", slot, i).c_str(), 0);
      preset.config.flowSections[i].targetFlow = prefs.isKey(presetKey("secFlow", slot, i).c_str()) ? prefs.getFloat(presetKey("secFlow", slot, i).c_str()) : DRONE_TARGET_FLOW_LPM;
    }
  }

  void savePresetSlot(Preferences &prefs, uint8_t slot, const HeadlessSetupPreset &preset)
  {
    prefs.putString(presetKey("name", slot).c_str(), preset.name);
    prefs.putUInt(presetKey("wait", slot).c_str(), preset.config.waitSeconds);
    prefs.putUInt(presetKey("duration", slot).c_str(), preset.config.durationSeconds);
    prefs.putFloat(presetKey("flow", slot).c_str(), preset.config.targetFlow);
    prefs.putUChar(presetKey("secCount", slot).c_str(), preset.config.flowSectionCount);
    for (uint8_t i = 0; i < MaxFlowSections; i++)
    {
      prefs.putUInt(presetKey("secDur", slot, i).c_str(), i < preset.config.flowSectionCount ? preset.config.flowSections[i].durationSeconds : 0);
      prefs.putFloat(presetKey("secFlow", slot, i).c_str(), i < preset.config.flowSectionCount ? preset.config.flowSections[i].targetFlow : DRONE_TARGET_FLOW_LPM);
    }
  }

  int findPresetSlot(Preferences &prefs, const char *name)
  {
    for (uint8_t i = 0; i < HeadlessSetup::kMaxPresets; i++)
    {
      String stored = prefs.isKey(presetKey("name", i).c_str()) ? prefs.getString(presetKey("name", i).c_str()) : "";
      if (stored == name)
        return i;
    }
    return -1;
  }

  int firstEmptyPresetSlot(Preferences &prefs)
  {
    for (uint8_t i = 0; i < HeadlessSetup::kMaxPresets; i++)
    {
      String stored = prefs.isKey(presetKey("name", i).c_str()) ? prefs.getString(presetKey("name", i).c_str()) : "";
      if (stored.length() == 0)
        return i;
    }
    return -1;
  }

  void clearPresetSlot(Preferences &prefs, uint8_t slot)
  {
    prefs.remove(presetKey("name", slot).c_str());
    prefs.remove(presetKey("wait", slot).c_str());
    prefs.remove(presetKey("duration", slot).c_str());
    prefs.remove(presetKey("flow", slot).c_str());
    prefs.remove(presetKey("secCount", slot).c_str());
    for (uint8_t i = 0; i < MaxFlowSections; i++)
    {
      prefs.remove(presetKey("secDur", slot, i).c_str());
      prefs.remove(presetKey("secFlow", slot, i).c_str());
    }
  }

  void handlePresets()
  {
    DynamicJsonDocument doc(2048);
    JsonArray presets = doc.createNestedArray("presets");
    Preferences prefs;
    prefs.begin("eolo_presets", true);
    for (uint8_t i = 0; i < HeadlessSetup::kMaxPresets; i++)
    {
      HeadlessSetupPreset preset;
      loadPresetSlot(prefs, i, preset);
      if (!HeadlessSetup::isSafePresetName(preset.name) || !HeadlessSetup::validateConfig(preset.config))
        continue;
      JsonObject obj = presets.createNestedObject();
      obj["name"] = preset.name;
      obj["durationSeconds"] = preset.config.durationSeconds;
      obj["targetFlow"] = preset.config.targetFlow;
      obj["flowSectionCount"] = preset.config.flowSectionCount;
    }
    prefs.end();

    size_t needed = measureJson(doc) + 1;
    char *buf = (char *)malloc(needed);
    if (buf) {
      serializeJson(doc, buf, needed);
      _server.send(200, "application/json", buf);
      free(buf);
    } else {
      _server.send(503, "application/json", "{\"error\":\"memory\"}");
    }
  }

  void handlePresetLoad()
  {
    String name = _server.arg("name");
    if (!HeadlessSetup::isSafePresetName(name.c_str()))
    {
      _server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_name\"}");
      return;
    }

    Preferences prefs;
    prefs.begin("eolo_presets", true);
    int slot = findPresetSlot(prefs, name.c_str());
    if (slot < 0)
    {
      prefs.end();
      _server.send(404, "application/json", "{\"ok\":false,\"error\":\"not_found\"}");
      return;
    }

    HeadlessSetupPreset preset;
    loadPresetSlot(prefs, slot, preset);
    prefs.end();

    if (!HeadlessSetup::validateConfig(preset.config))
    {
      _server.send(500, "application/json", "{\"ok\":false,\"error\":\"stored_invalid\"}");
      return;
    }

    DynamicJsonDocument doc(1536);
    doc["ok"] = true;
    doc["name"] = preset.name;
    JsonObject config = doc.createNestedObject("config");
    addConfigJson(config, preset.config);

    size_t needed = measureJson(doc) + 1;
    char *buf = (char *)malloc(needed);
    if (buf) {
      serializeJson(doc, buf, needed);
      _server.send(200, "application/json", buf);
      free(buf);
    } else {
      _server.send(503, "application/json", "{\"error\":\"memory\"}");
    }
  }

  void handlePresetSave()
  {
    String name = _server.arg("name");
    if (!HeadlessSetup::isSafePresetName(name.c_str()))
    {
      _server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_name\"}");
      return;
    }

    HeadlessSetupPreset preset;
    strlcpy(preset.name, name.c_str(), sizeof(preset.name));
    if (!parseConfigFromRequest(preset.config))
    {
      _server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_config\"}");
      return;
    }

    Preferences prefs;
    prefs.begin("eolo_presets", false);
    int slot = findPresetSlot(prefs, preset.name);
    if (slot < 0)
      slot = firstEmptyPresetSlot(prefs);
    if (slot < 0)
    {
      prefs.end();
      _server.send(507, "application/json", "{\"ok\":false,\"error\":\"preset_limit\"}");
      return;
    }

    savePresetSlot(prefs, slot, preset);
    prefs.end();
    _server.send(200, "application/json", "{\"ok\":true}");
  }

  void handlePresetDelete()
  {
    String name = _server.arg("name");
    if (!HeadlessSetup::isSafePresetName(name.c_str()))
    {
      _server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_name\"}");
      return;
    }

    Preferences prefs;
    prefs.begin("eolo_presets", false);
    int slot = findPresetSlot(prefs, name.c_str());
    if (slot < 0)
    {
      prefs.end();
      _server.send(404, "application/json", "{\"ok\":false,\"error\":\"not_found\"}");
      return;
    }
    clearPresetSlot(prefs, slot);
    prefs.end();
    _server.send(200, "application/json", "{\"ok\":true}");
  }

  void handleConfirm()
  {
    HeadlessSetupConfig config;
    if (!parseConfigFromRequest(config))
    {
      _server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_config\"}");
      return;
    }

    saveDefaults(config);
    _confirmedConfig = config;
    _confirmed = true;
    _server.send(200, "application/json", "{\"ok\":true}");
  }

};

#endif

#endif
