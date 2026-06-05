#ifndef HEADLESS_SETUP_SERVER_H
#define HEADLESS_SETUP_SERVER_H

#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)

#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <SD.h>
#include <WebServer.h>
#include <WiFi.h>
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

  static String jsonEscape(const String &value)
  {
    String out;
    out.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); i++)
    {
      char c = value.charAt(i);
      if (c == '"' || c == '\\')
      {
        out += '\\';
        out += c;
      }
      else if (c == '\n')
      {
        out += "\\n";
      }
      else if (c == '\r')
      {
        out += "\\r";
      }
      else
      {
        out += c;
      }
    }
    return out;
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

    String durationMode = _server.arg("durationMode");
    if (durationMode == "infinite")
    {
      config.durationSeconds = DRONE_DURATION_INFINITE;
    }
    else
    {
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
    String body = "{";
    body += "\"sdReady\":";
    body += _ctx.isSdReady ? "true" : "false";
    body += ",\"sdStatus\":\"";
    body += sdStatusText(_ctx.sdStatus);
    body += "\",\"rtc\":\"";
    body += jsonEscape(_ctx.components.rtc.now().timestamp());
    body += "\",\"defaults\":{\"waitSeconds\":";
    body += String(_defaults.waitSeconds);
    body += ",\"durationSeconds\":";
    body += String(_defaults.durationSeconds);
    body += ",\"targetFlow\":";
    body += String(_defaults.targetFlow, 1);
    body += "},\"calibration\":{\"loaded\":";
    body += _ctx.calibration.isLoaded ? "true" : "false";
    body += ",\"minFlow\":";
    body += _ctx.calibration.isLoaded && _ctx.calibration.numPoints > 0 ? String(_ctx.calibration.flows[0], 2) : "0";
    body += ",\"maxFlow\":";
    body += _ctx.calibration.isLoaded && _ctx.calibration.numPoints > 0 ? String(_ctx.calibration.flows[_ctx.calibration.numPoints - 1], 2) : "0";
    body += "},\"switches\":{\"waitCode\":";
    body += String(snap.waitCode);
    body += ",\"durationCode\":";
    body += String(snap.durationCode);
    body += ",\"wait\":\"";
    body += CaptureSwitches::waitDescription(snap.waitCode);
    body += "\",\"duration\":\"";
    body += CaptureSwitches::durationDescription(snap.durationCode);
    body += "\"}}";
    _server.send(200, "application/json", body);
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

    String body = "{\"available\":true,\"files\":[";
    bool first = true;
    File file = dir.openNextFile();
    while (file)
    {
      String name = String(file.name());
      int slash = name.lastIndexOf('/');
      if (slash >= 0)
        name = name.substring(slash + 1);

      if (!file.isDirectory() && HeadlessSetup::isSafeLogBasename(name))
      {
        if (!first)
          body += ",";
        first = false;
        body += "{\"name\":\"";
        body += jsonEscape(name);
        body += "\",\"size\":";
        body += String((uint32_t)file.size());
        body += "}";
      }
      file.close();
      file = dir.openNextFile();
    }
    dir.close();
    body += "]}";
    _server.send(200, "application/json", body);
  }

  String safeLogPathFromRequest()
  {
    String name = _server.arg("file");
    if (!HeadlessSetup::isSafeLogBasename(name))
      return "";
    return String(_ctx.logsDir) + "/" + name;
  }

  void handlePreview()
  {
    if (!_ctx.isSdReady)
    {
      _server.send(503, "application/json", "{\"error\":\"sd_unavailable\"}");
      return;
    }

    String path = safeLogPathFromRequest();
    if (path.isEmpty() || !SD.exists(path.c_str()))
    {
      _server.send(404, "application/json", "{\"error\":\"file_not_found\"}");
      return;
    }

    File file = SD.open(path.c_str(), FILE_READ);
    if (!file)
    {
      _server.send(500, "application/json", "{\"error\":\"open_failed\"}");
      return;
    }

    String header = file.readStringUntil('\n');
    header.trim();

    const uint32_t size = file.size();
    if (size > 4096)
    {
      file.seek(size - 4096);
      file.readStringUntil('\n');
    }

    String rows[40];
    size_t count = 0;
    while (file.available())
    {
      String row = file.readStringUntil('\n');
      row.trim();
      if (row.length() == 0 || row == header)
        continue;
      rows[count % 40] = row;
      count++;
    }
    file.close();

    size_t start = count > 40 ? count % 40 : 0;
    size_t total = count > 40 ? 40 : count;
    String body = "{\"header\":\"";
    body += jsonEscape(header);
    body += "\",\"rows\":[";
    for (size_t i = 0; i < total; i++)
    {
      if (i > 0)
        body += ",";
      body += "\"";
      body += jsonEscape(rows[(start + i) % 40]);
      body += "\"";
    }
    body += "]}";
    _server.send(200, "application/json", body);
  }

  void handleDownload()
  {
    if (!_ctx.isSdReady)
    {
      _server.send(503, "text/plain", "SD no disponible");
      return;
    }

    String name = _server.arg("file");
    String path = safeLogPathFromRequest();
    if (path.isEmpty() || !SD.exists(path.c_str()))
    {
      _server.send(404, "text/plain", "Archivo no encontrado");
      return;
    }

    File file = SD.open(path.c_str(), FILE_READ);
    if (!file)
    {
      _server.send(500, "text/plain", "No se pudo abrir el archivo");
      return;
    }

    _server.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
    _server.streamFile(file, "text/csv");
    file.close();
  }

  void handleConfirm()
  {
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
