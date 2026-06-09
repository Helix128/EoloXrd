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
    _server.on("/api/calibration/status", HTTP_GET, [this]() { handleCalibrationStatus(); });
    _server.on("/api/calibration/start", HTTP_POST, [this]() { handleCalibrationStart(); });
    _server.on("/api/calibration/abort", HTTP_POST, [this]() { handleCalibrationAbort(); });
    _server.on("/api/calibration/clear", HTTP_POST, [this]() { handleCalibrationClear(); });
    _server.on("/download", HTTP_GET, [this]() { handleDownload(); });
    _server.on("/api/confirm", HTTP_POST, [this]() { handleConfirm(); });
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

private:
  Context &_ctx;
  CaptureSwitches &_switches;
  WebServer _server;
  DNSServer _dnsServer;
  HeadlessSetupConfig _defaults;
  HeadlessSetupConfig _confirmedConfig;
  bool _running = false;
  bool _confirmed = false;

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
    prefs.begin("eolo_headless", true);
    _defaults.waitSeconds = prefs.getUInt("wait", 0);
    _defaults.durationSeconds = prefs.getUInt("duration", 5UL * MINUTE);
    _defaults.targetFlow = prefs.getFloat("flow", DRONE_TARGET_FLOW_LPM);
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
    prefs.end();
  }

  bool parseConfirm(HeadlessSetupConfig &config)
  {
    if (!_server.hasArg("waitSeconds") || !_server.hasArg("targetFlow"))
      return false;

    config.waitSeconds = _server.arg("waitSeconds").toInt();
    config.targetFlow = _server.arg("targetFlow").toFloat();

    if (_server.hasArg("durationMode") && _server.arg("durationMode") == "infinite") {
      config.durationSeconds = DRONE_DURATION_INFINITE;
    } else {
      if (!_server.hasArg("durationSeconds"))
        return false;
      config.durationSeconds = _server.arg("durationSeconds").toInt();
    }

    return HeadlessSetup::validateConfig(config);
  }

  void redirectToPortal()
  {
    _server.sendHeader("Location", PortalUrl, true);
    _server.send(302, "text/plain", "");
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

    redirectToPortal();
  }

  void handleRoot()
  {
    _server.sendHeader("Cache-Control", "no-store");
    _server.sendHeader("Content-Encoding", "gzip");
    _server.send_P(200, "text/html; charset=utf-8", reinterpret_cast<PGM_P>(kHeadlessSetupHtmlGzip), kHeadlessSetupHtmlGzipSize);
  }

  void handleStatus()
  {
    CaptureSwitchSnapshot snap = _switches.snapshot();
    StaticJsonDocument<1024> doc;
    doc["sdReady"] = _ctx.isSdReady;
    doc["sdStatus"] = sdStatusText(_ctx.sdStatus);
    doc["rtc"] = _ctx.components.rtc.now().timestamp();

    JsonObject defaults = doc.createNestedObject("defaults");
    defaults["waitSeconds"] = _defaults.waitSeconds;
    defaults["durationSeconds"] = _defaults.durationSeconds;
    defaults["targetFlow"] = _defaults.targetFlow;

    JsonObject calibration = doc.createNestedObject("calibration");
    calibration["loaded"] = _ctx.calibration.isLoaded;
    if (_ctx.calibration.isLoaded && _ctx.calibration.numPoints > 0) {
      calibration["minFlow"] = _ctx.calibration.flows[0];
      calibration["maxFlow"] = _ctx.calibration.flows[_ctx.calibration.numPoints - 1];
    } else {
      calibration["minFlow"] = 0.0;
      calibration["maxFlow"] = 0.0;
    }

    JsonObject switches = doc.createNestedObject("switches");
    switches["waitCode"] = snap.waitCode;
    switches["durationCode"] = snap.durationCode;
    switches["wait"] = CaptureSwitches::waitDescription(snap.waitCode);
    switches["duration"] = CaptureSwitches::durationDescription(snap.durationCode);

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

    DynamicJsonDocument doc(4096);
    doc["available"] = true;
    JsonArray files = doc.createNestedArray("files");

    File file = dir.openNextFile();
    while (file)
    {
      const char *rawName = file.name();
      const char *base = rawName;
      const char *slash = strrchr(rawName, '/');
      if (slash) base = slash + 1;

      if (!file.isDirectory() && HeadlessSetup::isSafeLogBasename(base))
      {
        JsonObject obj = files.createNestedObject();
        obj["name"] = base;
        obj["size"] = (uint32_t)file.size();
      }
      file.close();
      file = dir.openNextFile();
    }
    dir.close();

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

  // Fill provided buffer 'out' with the safe log path. Returns true on success.
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
      // discard until next line
      file.readBytesUntil('\n', headerBuf, sizeof(headerBuf) - 1);
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

    DynamicJsonDocument doc(4096);
    doc["header"] = headerBuf;
    JsonArray arr = doc.createNestedArray("rows");
    for (size_t i = 0; i < total; i++)
    {
      arr.add(rows[(start + i) % ROWS]);
    }

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

  bool parseCalibrationConfig(HeadlessMotorCalibrationConfig &config)
  {
    if (_server.hasArg("pwmStart")) config.pwmStart = _server.arg("pwmStart").toInt();
    if (_server.hasArg("pwmEnd")) config.pwmEnd = _server.arg("pwmEnd").toInt();
    if (_server.hasArg("pwmStep")) config.pwmStep = _server.arg("pwmStep").toInt();
    if (_server.hasArg("settleMs")) config.settleMs = _server.arg("settleMs").toInt();
    if (_server.hasArg("sampleIntervalMs")) config.sampleIntervalMs = _server.arg("sampleIntervalMs").toInt();
    if (_server.hasArg("samplesPerPoint")) config.samplesPerPoint = _server.arg("samplesPerPoint").toInt();
    if (_server.hasArg("maxTargetFlow")) config.maxTargetFlow = _server.arg("maxTargetFlow").toFloat();
    if (_server.hasArg("minValidFlow")) config.minValidFlow = _server.arg("minValidFlow").toFloat();
    if (_server.hasArg("minFlowDelta")) config.minFlowDelta = _server.arg("minFlowDelta").toFloat();
    if (_server.hasArg("maxFlowStddev")) config.maxFlowStddev = _server.arg("maxFlowStddev").toFloat();
    return HeadlessMotorCalibration::validateConfig(config);
  }

  void handleCalibrationStatus()
  {
    const HeadlessMotorCalibration &cal = _ctx.headlessCalibration;

    // allocate document dynamically based on number of points to avoid large stack usage
    size_t approx = 1024 + (size_t)cal.pointCount() * 96;
    DynamicJsonDocument doc(approx);
    doc["running"] = cal.isRunning();
    doc["state"] = cal.stateText();
    doc["currentPwm"] = cal.currentPwm();
    doc["points"] = cal.pointCount();
    doc["minFlow"] = cal.minFlow();
    doc["maxFlow"] = cal.maxFlow();
    doc["error"] = cal.errorText();

    const HeadlessMotorCalibrationConfig &c = cal.config();
    JsonObject cfg = doc.createNestedObject("config");
    cfg["pwmStart"] = c.pwmStart;
    cfg["pwmEnd"] = c.pwmEnd;
    cfg["pwmStep"] = c.pwmStep;
    cfg["settleMs"] = c.settleMs;
    cfg["sampleIntervalMs"] = c.sampleIntervalMs;
    cfg["samplesPerPoint"] = c.samplesPerPoint;
    cfg["maxTargetFlow"] = c.maxTargetFlow;
    cfg["minValidFlow"] = c.minValidFlow;
    cfg["minFlowDelta"] = c.minFlowDelta;
    cfg["maxFlowStddev"] = c.maxFlowStddev;

    JsonArray data = doc.createNestedArray("data");
    for (int i = 0; i < cal.pointCount(); i++)
    {
      const HeadlessMotorCalibrationPoint &p = cal.point(i);
      JsonObject obj = data.createNestedObject();
      obj["pwm"] = p.pwm;
      obj["flow"] = p.flow;
      obj["stddev"] = p.stddev;
    }

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

  void handleCalibrationStart()
  {
    if (_ctx.isCapturing)
    {
      _server.send(409, "application/json", "{\"ok\":false,\"error\":\"capturing\"}");
      return;
    }

    HeadlessMotorCalibrationConfig config;
    if (!parseCalibrationConfig(config))
    {
      _server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_config\"}");
      return;
    }

    if (!_ctx.headlessCalibration.start(config))
    {
      _server.send(409, "application/json", "{\"ok\":false,\"error\":\"busy\"}");
      return;
    }
    _server.send(200, "application/json", "{\"ok\":true}");
  }

  void handleCalibrationAbort()
  {
    _ctx.headlessCalibration.abort(_ctx);
    _server.send(200, "application/json", "{\"ok\":true}");
  }

  void handleCalibrationClear()
  {
    _ctx.headlessCalibration.clear(_ctx);
    _server.send(200, "application/json", "{\"ok\":true}");
  }

  void handleConfirm()
  {
    if (_ctx.headlessCalibration.isRunning())
    {
      _server.send(409, "application/json", "{\"ok\":false,\"error\":\"calibration_running\"}");
      return;
    }

    HeadlessSetupConfig config;
    if (!parseConfirm(config))
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
