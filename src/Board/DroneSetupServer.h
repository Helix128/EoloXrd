#ifndef DRONE_SETUP_SERVER_H
#define DRONE_SETUP_SERVER_H

#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)

#include <Arduino.h>
#include <Preferences.h>
#include <SD.h>
#include <WebServer.h>
#include <WiFi.h>
#include "CaptureSwitches.h"
#include "DroneSetupTypes.h"
#include "../Data/Context.h"

class DroneSetupServer
{
public:
  static constexpr const char *ApSsid = "EOLO-Dron-Setup";
  static constexpr const char *ApPassword = "EoloSetup2026";

  explicit DroneSetupServer(Context &context, CaptureSwitches &switches)
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
    WiFi.softAP(ApSsid, ApPassword);

    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    _server.on("/api/logs", HTTP_GET, [this]() { handleLogs(); });
    _server.on("/api/logs/preview", HTTP_GET, [this]() { handlePreview(); });
    _server.on("/download", HTTP_GET, [this]() { handleDownload(); });
    _server.on("/api/confirm", HTTP_POST, [this]() { handleConfirm(); });
    _server.onNotFound([this]() { _server.send(404, "application/json", "{\"error\":\"not_found\"}"); });
    _server.begin();
    _running = true;

    LOG_OUT("Drone setup Wi-Fi activo: ");
    LOG_OUT(ApSsid);
    LOG_OUT(" / ");
    LOG_OUT_LN(ApPassword);
    LOG_OUT("IP setup: ");
    LOG_OUT_LN(WiFi.softAPIP());
  }

  void handleClient()
  {
    if (_running)
      _server.handleClient();
  }

  void stop()
  {
    if (!_running)
      return;

    _server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    _running = false;
  }

  bool confirmed() const { return _confirmed; }
  const DroneSetupConfig &confirmedConfig() const { return _confirmedConfig; }

private:
  Context &_ctx;
  CaptureSwitches &_switches;
  WebServer _server;
  DroneSetupConfig _defaults;
  DroneSetupConfig _confirmedConfig;
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
    prefs.begin("eolo_drone_setup", true);
    _defaults.waitSeconds = prefs.getUInt("wait", 0);
    _defaults.durationSeconds = prefs.getUInt("duration", 5UL * MINUTE);
    _defaults.targetFlow = prefs.getFloat("flow", DRONE_TARGET_FLOW_LPM);
    prefs.end();

    if (!DroneSetup::validateConfig(_defaults))
      _defaults = DroneSetupConfig();
  }

  void saveDefaults(const DroneSetupConfig &config)
  {
    Preferences prefs;
    prefs.begin("eolo_drone_setup", false);
    prefs.putUInt("wait", config.waitSeconds);
    prefs.putUInt("duration", config.durationSeconds);
    prefs.putFloat("flow", config.targetFlow);
    prefs.end();
  }

  bool parseConfirm(DroneSetupConfig &config)
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

    return DroneSetup::validateConfig(config);
  }

  void handleRoot()
  {
    _server.send_P(200, "text/html; charset=utf-8", kHtml);
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

      if (!file.isDirectory() && DroneSetup::isSafeLogBasename(name))
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
    if (!DroneSetup::isSafeLogBasename(name))
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
    DroneSetupConfig config;
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

  static constexpr const char kHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EOLO Dron Setup</title>
<style>
:root{color-scheme:light;--bg:#f6f8fb;--fg:#17202a;--muted:#5f6b7a;--line:#d8dee8;--primary:#146c94;--danger:#b42318;--ok:#067647}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font-family:system-ui,-apple-system,Segoe UI,sans-serif}header{padding:18px 16px;background:#fff;border-bottom:1px solid var(--line)}main{max-width:980px;margin:0 auto;padding:16px;display:grid;gap:16px}.bar{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap}h1{font-size:24px;margin:0}h2{font-size:18px;margin:0 0 12px}.panel{background:#fff;border:1px solid var(--line);border-radius:8px;padding:16px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:12px}.field{display:grid;gap:6px}label,.label{font-size:13px;color:var(--muted)}input,select,button{font:inherit}input,select{width:100%;padding:11px;border:1px solid var(--line);border-radius:6px;background:#fff;color:var(--fg)}button{border:0;border-radius:6px;padding:11px 14px;background:var(--primary);color:#fff;font-weight:650}button.secondary{background:#eef3f7;color:var(--fg)}button.link{background:transparent;color:var(--primary);padding:4px 0}.chips{display:flex;gap:8px;flex-wrap:wrap}.chip{background:#eef3f7;color:#1f2937}.status{display:flex;gap:8px;flex-wrap:wrap}.pill{padding:5px 9px;border-radius:999px;background:#eef3f7;font-size:13px}.ok{color:var(--ok)}.bad{color:var(--danger)}.warn{color:#93370d}table{width:100%;border-collapse:collapse;font-size:14px}th,td{text-align:left;border-bottom:1px solid var(--line);padding:8px}pre{margin:0;white-space:pre;overflow:auto;max-height:320px;font-size:12px}.actions{display:flex;gap:10px;align-items:center;flex-wrap:wrap}.hidden{display:none}@media(max-width:620px){main{padding:12px}.panel{padding:14px}h1{font-size:21px}table{font-size:12px}}
</style>
</head>
<body>
<header><div class="bar"><h1>EOLO Dron</h1><div class="status" id="statusPills"></div></div></header>
<main>
<section class="panel">
<h2>Configuracion de captura</h2>
<form id="setupForm" class="grid">
<div class="field"><label>Espera antes de capturar</label><input id="waitSeconds" name="waitSeconds" type="number" min="0" max="86400" step="1"></div>
<div class="field"><label>Duracion</label><select id="durationMode" name="durationMode"><option value="finite">Finita</option><option value="infinite">Infinita</option></select></div>
<div class="field" id="durationField"><label>Segundos de captura</label><input id="durationSeconds" name="durationSeconds" type="number" min="1" max="86400" step="1"></div>
<div class="field"><label>Flujo objetivo L/min</label><input id="targetFlow" name="targetFlow" type="number" min="0" max="8" step="0.1"></div>
<div class="field"><span class="label">Presets espera</span><div class="chips"><button type="button" class="chip" data-wait="0">Instantanea</button><button type="button" class="chip" data-wait="300">5 min</button><button type="button" class="chip" data-wait="900">15 min</button></div></div>
<div class="field"><span class="label">Presets duracion</span><div class="chips"><button type="button" class="chip" data-duration="300">5 min</button><button type="button" class="chip" data-duration="900">15 min</button><button type="button" class="chip" data-duration="infinite">Infinita</button></div></div>
<div class="field"><span class="label">Estado</span><div id="calibrationText">Cargando...</div><div id="flowWarning" class="warn"></div></div>
<div class="field"><span class="label">&nbsp;</span><button type="submit">Confirmar e iniciar</button></div>
</form>
</section>
<section class="panel">
<div class="bar"><h2>Registros CSV</h2><div class="actions"><button type="button" class="secondary" id="refreshLogs">Actualizar</button><button type="button" class="secondary" id="downloadAll">Descargar todos</button></div></div>
<table><thead><tr><th>Archivo</th><th>Tamano</th><th></th></tr></thead><tbody id="logsBody"><tr><td colspan="3">Cargando...</td></tr></tbody></table>
</section>
<section class="panel">
<h2>Preview</h2>
<pre id="preview">Seleccione un CSV.</pre>
</section>
</main>
<script>
const $=id=>document.getElementById(id);let logFiles=[],cal=null;
function fmtSize(n){if(n>1048576)return(n/1048576).toFixed(1)+' MB';if(n>1024)return(n/1024).toFixed(1)+' KB';return n+' B'}
function setDuration(v){if(v==='infinite'){$('durationMode').value='infinite';$('durationField').classList.add('hidden')}else{$('durationMode').value='finite';$('durationSeconds').value=v;$('durationField').classList.remove('hidden')}}
function updateFlowWarning(){const f=parseFloat($('targetFlow').value);let msg='';if(cal&&cal.loaded&&(f<cal.minFlow||f>cal.maxFlow))msg='Fuera del rango calibrado: '+cal.minFlow.toFixed(2)+' a '+cal.maxFlow.toFixed(2)+' L/min';$('flowWarning').textContent=msg}
async function loadStatus(){const r=await fetch('/api/status');const s=await r.json();cal=s.calibration;$('statusPills').innerHTML='<span class="pill '+(s.sdReady?'ok':'bad')+'">SD '+s.sdStatus+'</span><span class="pill">RTC '+s.rtc+'</span><span class="pill">Switch espera '+s.switches.wait+'</span>';$('waitSeconds').value=s.defaults.waitSeconds;$('targetFlow').value=Number(s.defaults.targetFlow).toFixed(1);setDuration(s.defaults.durationSeconds===4294967295?'infinite':s.defaults.durationSeconds);$('calibrationText').textContent=s.calibration.loaded?'Calibracion '+s.calibration.minFlow.toFixed(2)+' a '+s.calibration.maxFlow.toFixed(2)+' L/min':'Sin calibracion cargada';updateFlowWarning()}
async function loadLogs(){const body=$('logsBody');body.innerHTML='<tr><td colspan="3">Cargando...</td></tr>';const r=await fetch('/api/logs');if(!r.ok){body.innerHTML='<tr><td colspan="3">SD no disponible</td></tr>';return}const j=await r.json();logFiles=j.files||[];if(!logFiles.length){body.innerHTML='<tr><td colspan="3">Sin CSV disponibles</td></tr>';return}body.innerHTML=logFiles.map(f=>'<tr><td>'+f.name+'</td><td>'+fmtSize(f.size)+'</td><td><button class="link" data-preview="'+f.name+'">Ver</button> <a href="/download?file='+encodeURIComponent(f.name)+'">Descargar</a></td></tr>').join('')}
async function preview(name){const r=await fetch('/api/logs/preview?file='+encodeURIComponent(name));const j=await r.json();$('preview').textContent=[j.header].concat(j.rows||[]).join('\n')}
document.addEventListener('click',e=>{const t=e.target;if(t.dataset.wait)$('waitSeconds').value=t.dataset.wait;if(t.dataset.duration)setDuration(t.dataset.duration);if(t.dataset.preview)preview(t.dataset.preview);updateFlowWarning()});
$('durationMode').addEventListener('change',e=>setDuration(e.target.value==='infinite'?'infinite':$('durationSeconds').value||300));
$('targetFlow').addEventListener('input',updateFlowWarning);
$('refreshLogs').addEventListener('click',loadLogs);
$('downloadAll').addEventListener('click',()=>logFiles.forEach((f,i)=>setTimeout(()=>{location.href='/download?file='+encodeURIComponent(f.name)},i*450)));
$('setupForm').addEventListener('submit',async e=>{e.preventDefault();const fd=new URLSearchParams(new FormData(e.target));if($('durationMode').value==='infinite')fd.set('durationMode','infinite');const r=await fetch('/api/confirm',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:fd});$('preview').textContent=r.ok?'Configuracion confirmada. El Wi-Fi se apagara y comenzara la sesion.':'Configuracion invalida.'});
loadStatus().then(loadLogs);
</script>
</body>
</html>
)HTML";
};

#endif

#endif
