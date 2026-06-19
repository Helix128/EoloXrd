#ifndef HEADLESS_SETUP_SERVER_H
#define HEADLESS_SETUP_SERVER_H

#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)

#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <SD.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "CaptureSwitches.h"
#include "HeadlessSetupTypes.h"
#include "HeadlessSetupWebPage.h"
#include "../Data/Context.h"

#ifndef HEADLESS_SETUP_AP_SSID
#define HEADLESS_SETUP_AP_SSID "eolo-dron"
#endif

#ifndef HEADLESS_SETUP_AP_PASSWORD
#define HEADLESS_SETUP_AP_PASSWORD "eolo-dron"
#endif

class HeadlessSetupServer
{
public:
  static constexpr const char *ApSsid = HEADLESS_SETUP_AP_SSID;
  static constexpr const char *ApPassword = HEADLESS_SETUP_AP_PASSWORD;
  static constexpr const char *PortalHost = "eolo.setup";
  static constexpr const char *PortalUrl = "http://eolo.setup/";
  static constexpr const char *PortalIpUrl = "http://192.168.4.1/";
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
    WiFi.mode(WIFI_AP);
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(ApSsid, ApPassword);
    _dnsServer.start(DnsPort, "*", WiFi.softAPIP());

    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    _server.on("/api/logs", HTTP_GET, [this]() { handleLogs(); });
    _server.on("/api/logs/preview", HTTP_GET, [this]() { handlePreview(); });
    _server.on("/download", HTTP_GET, [this]() { handleDownload(); });
    _server.on("/api/logs/delete", HTTP_POST, [this]() { handleLogDelete(); });
    _server.on("/api/confirm", HTTP_POST, [this]() { handleConfirm(); });
    _server.on("/api/presets", HTTP_GET, [this]() { handlePresets(); });
    _server.on("/api/presets/load", HTTP_GET, [this]() { handlePresetLoad(); });
    _server.on("/api/presets/save", HTTP_POST, [this]() { handlePresetSave(); });
    _server.on("/api/presets/delete", HTTP_POST, [this]() { handlePresetDelete(); });
    _server.on("/api/motor/ignite", HTTP_POST, [this]() { handleIgnite(); });
    _server.on("/api/debug/enter", HTTP_POST, [this]() { handleDebugEnter(); });
    _server.on("/api/debug/pwm", HTTP_POST, [this]() { handleDebugPwm(); });
    _server.on("/api/debug/status", HTTP_GET, [this]() { handleDebugStatus(); });
    _server.on("/favicon.ico", HTTP_GET, [this]() {
      _server.send(204, "image/x-icon", "");
    });
    registerCaptivePortalEndpoints();
    _server.onNotFound([this]() { handleNotFound(); });
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
  }

  void handleClient()
  {
    if (!_running)
      return;

    _dnsServer.processNextRequest();
    _server.handleClient();
  }

  void stop()
  {
    if (!_running)
      return;

    _server.stop();
    _dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    _running = false;
  }

  bool confirmed() const { return _confirmed; }
  const HeadlessSetupConfig &confirmedConfig() const { return _confirmedConfig; }
  bool debugModeActive() const { return _debugMode; }

private:
  Context &_ctx;
  CaptureSwitches &_switches;
  WebServer _server;
  DNSServer _dnsServer;
  HeadlessSetupConfig _defaults;
  HeadlessSetupConfig _confirmedConfig;
  bool _running = false;
  bool _confirmed = false;
  bool _debugMode = false;

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
    _server.on("/generate_204", HTTP_GET, [this]() { redirectToPortal(); });
    _server.on("/gen_204", HTTP_GET, [this]() { redirectToPortal(); });
    _server.on("/mobile/status.php", HTTP_GET, [this]() { redirectToPortal(); });
    _server.on("/hotspot-detect.html", HTTP_GET, [this]() { redirectToPortal(); });
    _server.on("/library/test/success.html", HTTP_GET, [this]() { redirectToPortal(); });
    _server.on("/success.txt", HTTP_GET, [this]() { redirectToPortal(); });
    _server.on("/connecttest.txt", HTTP_GET, [this]() { redirectToPortal(); });
    _server.on("/ncsi.txt", HTTP_GET, [this]() { redirectToPortal(); });
    _server.on("/fwlink", HTTP_GET, [this]() { redirectToPortal(); });
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
    if (host.length() > 0 && !host.equals("192.168.4.1") && !host.equals("eolo.setup"))
    {
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

    doc["motorTempValid"] = _ctx.motorCapture.motorThermalSensorValid;
    doc["motorTemp"] = _ctx.motorCapture.motorThermalTemperature;
    doc["overheat"] = _ctx.motorCapture.motorOverheatActive;

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
#if defined(FEATURE_FLOW_PID)
    _ctx.motorCapture.forceIgnition();
    _server.send(200, "application/json", "{\"ok\":true}");
#else
    _server.send(400, "application/json", "{\"ok\":false,\"error\":\"no PID\"}");
#endif
  }

  void handleStatus()
  {
    CaptureSwitchSnapshot snap = _switches.snapshot();
    StaticJsonDocument<2048> doc;
    doc["sdReady"] = _ctx.isSdReady;
    doc["sdStatus"] = sdStatusText(_ctx.sdStatus);
    doc["rtc"] = _ctx.components.rtc.now().timestamp();

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
    if (!_ctx.isSdReady)
    {
      _server.send(503, "application/json", "{\"available\":false,\"files\":[]}");
      return;
    }

    File dir = SD.open(_ctx.logsDir);
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
    // compose path: logsDir + '/' + name
    int ret = snprintf(out, outLen, "%s/%s", _ctx.logsDir, name.c_str());
    return ret > 0 && (size_t)ret < outLen;
  }

  void handlePreview()
  {
    if (!_ctx.isSdReady)
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

    // collect last up to 40 rows
    const size_t ROWS = 40;
    const size_t ROW_MAX = 256;
    static char rows[ROWS][ROW_MAX];
    size_t count = 0;
    while (file.available())
    {
      size_t rlen = file.readBytesUntil('\n', rows[count % ROWS], ROW_MAX - 1);
      rows[count % ROWS][rlen] = '\0';
      // trim CR
      if (rlen > 0 && rows[count % ROWS][rlen - 1] == '\r') rows[count % ROWS][rlen - 1] = '\0';
      if (rlen == 0 || strcmp(rows[count % ROWS], headerBuf) == 0)
      {
        // ignore
      }
      else
      {
        count++;
      }
    }
    file.close();

    size_t start = count > ROWS ? count % ROWS : 0;
    size_t total = count > ROWS ? ROWS : count;

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
      _server.sendContent(rows[(start + i) % ROWS]);
      _server.sendContent("\"");
    }

    _server.sendContent("]}");
    _server.sendContent(""); // end chunked stream
  }

  void handleDownload()
  {
    if (!_ctx.isSdReady)
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
    _server.streamFile(file, "text/csv");
    file.close();
  }

  void handleLogDelete()
  {
    if (!_ctx.isSdReady)
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

    if (SD.remove(pathBuf))
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
