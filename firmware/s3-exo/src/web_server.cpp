// Robot Web Server — stripped deskpet version + robot status API
#include "web_server.h"
#include "hw/audio.h"
#include "hw/display.h"
#include "hw/power.h"
#include "stats.h"
#include "wifi_manager.h"
#include "robot/fairino_udp.h"
#include "robot/cnde_client.h"
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>

static AsyncWebServer server(80);
static bool s_webRunning = false;

// Log buffer
static const size_t LOG_CAP = 100;
static char logBuf[LOG_CAP][128];
static size_t logHead = 0, logCount = 0;

void webLog(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(logBuf[logHead], sizeof(logBuf[0]), fmt, args);
  va_end(args);
  logHead = (logHead + 1) % LOG_CAP;
  if (logCount < LOG_CAP) logCount++;
}

String webLogGet() {
  String json = "[";
  size_t start = (logCount < LOG_CAP) ? 0 : logHead;
  for (size_t i = 0; i < logCount; i++) {
    size_t idx = (start + i) % LOG_CAP;
    if (i > 0) json += ',';
    json += '"';
    for (char* p = logBuf[idx]; *p; p++) {
      if (*p == 0x22) { json += (char)0x5C; }
      if (*p == '\n') { json += "\\n"; continue; }
      if (*p == '\r') continue;
      if (*p == '\t') { json += "\\t"; continue; }
      json += *p;
    }
    json += '"';
  }
  json += ']';
  return json;
}

static void addCors(AsyncWebServerResponse* resp) {
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void handleStatus(AsyncWebServerRequest* req) {
  AsyncResponseStream* resp = req->beginResponseStream("application/json");
  JsonDocument doc;

  HwBattery bat = hwBattery();
  JsonObject batObj = doc["battery"].to<JsonObject>();
  batObj["pct"] = bat.pct;
  batObj["mV"]  = bat.mV;
  batObj["charging"] = bat.usbPresent;

  JsonObject wifiObj = doc["wifi"].to<JsonObject>();
  wifiObj["connected"] = wifiMgrConnected();
  wifiObj["rssi"]      = wifiMgrConnected() ? wifiMgrRssi() : 0;
  wifiObj["ip"]        = wifiMgrConnected() ? wifiMgrLocalIP() : "";
  wifiObj["ssid"]      = wifiMgrConnected() ? WiFi.SSID() : "";
  wifiObj["static"]    = wifiMgrIsStatic();

  JsonObject sysObj = doc["sys"].to<JsonObject>();
  sysObj["uptime_sec"]  = millis() / 1000;
  sysObj["free_heap"]   = ESP.getFreeHeap();
  sysObj["free_psram"]  = ESP.getFreePsram();
  sysObj["fs_used"]     = LittleFS.usedBytes();
  sysObj["fs_total"]    = LittleFS.totalBytes();

  const RobotStateData& rs = g_cnde.getState();
  JsonObject robotObj = doc["robot"].to<JsonObject>();
  robotObj["cnde_connected"] = g_cnde.isConnected();
  robotObj["valid"]   = rs.valid;
  if (rs.valid) {
    JsonArray joints = robotObj["joints"].to<JsonArray>();
    for (int i = 0; i < 6; i++) joints.add(rs.jointPos[i]);
    robotObj["robot_state"]   = rs.robotState;
    robotObj["program_state"] = rs.programState;
    robotObj["main_code"]     = rs.mainCode;
    robotObj["sub_code"]      = rs.subCode;
  }

  doc["fairino_ok"] = true;
  Settings& s = settings();
  doc["sound"]      = s.sound;
  doc["brightness"] = s.brightness;

  serializeJson(doc, *resp);
  addCors(resp);
  req->send(resp);
}

static void handleBrightness(AsyncWebServerRequest* req, JsonVariant& json) {
  uint8_t lvl = json["level"] | 99;
  if (lvl > 4) { req->send(400, "application/json", "{\"ok\":false}"); return; }
  hwDisplayBrightness(lvl);
  req->send(200, "application/json", "{\"ok\":true}");
}

static void handleSound(AsyncWebServerRequest* req, JsonVariant& json) {
  if (!json["sound"].is<bool>()) { req->send(400, "application/json", "{\"ok\":false}"); return; }
  _settings.sound = json["sound"].as<bool>();
  settingsSave();
  req->send(200, "application/json", "{\"ok\":true}");
}

static void handleWifi(AsyncWebServerRequest* req, JsonVariant& json) {
  const char* action = json["action"];
  if (!action) { req->send(400, "application/json", "{\"ok\":false}"); return; }
  if (strcmp(action, "scan") == 0) {
    wifiMgrScan([req](String result) {
      AsyncWebServerResponse* r = req->beginResponse(200, "application/json", result);
      r->addHeader("Access-Control-Allow-Origin", "*");
      req->send(r);
    });
    return;
  }
  if (strcmp(action, "connect") == 0) {
    const char* ssid = json["ssid"];
    const char* pass = json["pass"];
    if (!ssid) { req->send(400, "application/json", "{\"ok\":false}"); return; }
    wifiMgrConnect(ssid, pass ? pass : "");
    req->send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (strcmp(action, "disconnect") == 0) {
    wifiMgrDisconnect();
    req->send(200, "application/json", "{\"ok\":true}");
    return;
  }
  req->send(400, "application/json", "{\"error\":\"unknown action\"}");
}

static void handleBeep(AsyncWebServerRequest* req, JsonVariant& json) {
  uint16_t freq = json["freq"] | 440;
  uint16_t dur  = json["dur"] | 200;
  if (dur > 5000) dur = 5000;
  hwBeep(freq, dur);
  req->send(200, "application/json", "{\"ok\":true}");
}

static void handleMelody(AsyncWebServerRequest* req, JsonVariant& json) {
  JsonArray notes = json["notes"];
  if (!notes) { req->send(400, "application/json", "{\"error\":\"missing notes\"}"); return; }
  for (JsonArray n : notes) {
    uint16_t freq = n[0] | 0;
    uint16_t dur  = n[1] | 0;
    uint16_t gap  = n[2] | 0;
    if (freq > 10000) freq = 10000;
    uint16_t total = dur + gap;
    if (total > 5000) total = 5000;
    hwBeep(freq, total);
  }
  req->send(200, "application/json", "{\"ok\":true}");
}

static void handleMelodyPage(AsyncWebServerRequest* req) {
  String html = R"rawliteral(<!DOCTYPE html>
<html lang="zh"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Melody Player</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#1a1a2e;color:#eee;font-family:system-ui,sans-serif;max-width:480px;margin:0 auto;padding:12px}
h1{text-align:center;font-size:20px;margin:8px 0 16px;color:#f0c040}
h2{font-size:14px;margin:8px 0 6px;color:#aaa}
.card{background:#16213e;border-radius:12px;padding:12px;margin-bottom:12px}
.btn{display:inline-block;padding:10px 12px;margin:4px;background:#0f3460;color:#eee;border:none;border-radius:8px;font-size:13px;cursor:pointer;min-width:80px}
.btn:active{background:#e94560}
.btn.full{width:calc(100% - 8px)}
.key{display:inline-block;padding:8px 10px;margin:2px;background:#0a9f;color:#fff;border:none;border-radius:6px;font-size:12px;cursor:pointer;min-width:36px;text-align:center}
.key:active{background:#ff6b35}
.status{margin-top:10px;padding:6px 10px;border-radius:6px;font-size:13px;text-align:center}
.status.ok{background:#1b5e20;color:#a5d6a7}
.status.err{background:#4a1c1c;color:#ef9a9a}
</style></head><body>
<h1>Melody Player</h1>
<div class="card"><h2>Presets</h2>
<button class="btn" onclick="playMelody(SHUI_CHE)">Sprinkler</button>
<button class="btn" onclick="playMelody(LAN_HUA_CAO)">Orchid</button>
<button class="btn" onclick="playMelody(HAPPY_BDAY)">Happy BDay</button>
<button class="btn" onclick="playMelody(TWINKLE)">Twinkle</button>
<button class="btn" onclick="playMelody(ODE_JOY)">Ode Joy</button>
<button class="btn full" onclick="playMelody(MARIO)">Mario</button>
</div>
<div class="card"><h2>Piano</h2>
<button class="key" onclick="beep(262)">C</button><button class="key" onclick="beep(294)">D</button>
<button class="key" onclick="beep(330)">E</button><button class="key" onclick="beep(349)">F</button>
<button class="key" onclick="beep(392)">G</button><button class="key" onclick="beep(440)">A</button>
<button class="key" onclick="beep(494)">B</button><button class="key" onclick="beep(523)">c</button>
</div>
<div id="st" class="status" style="display:none"></div>
<script>
const SHUI_CHE=[[440,440,60],[330,440,60],[392,440,60],[440,440,60],[440,440,60],[392,440,60],[330,440,60],[294,440,60],[330,230,20],[294,230,20],[262,230,20],[294,230,20],[330,230,20],[392,230,20],[330,1900,100]];
const HAPPY_BDAY=[[262,150,30],[262,150,30],[294,300,60],[262,300,60],[349,300,60],[330,600,60],[262,150,30],[262,150,30],[294,300,60],[262,300,60],[392,300,60],[349,600,60],[262,150,30],[262,150,30],[523,300,60],[440,300,60],[349,300,60],[330,300,60],[294,900,60]];
const TWINKLE=[[262,200,40],[262,200,40],[392,200,40],[392,200,40],[440,200,40],[440,200,40],[392,400,60],[349,200,40],[349,200,40],[330,200,40],[330,200,40],[294,200,40],[294,200,40],[262,400,60]];
const ODE_JOY=[[330,200,40],[330,200,40],[349,200,40],[392,200,40],[392,200,40],[349,200,40],[330,200,40],[294,200,40],[262,200,40],[262,200,40],[294,200,40],[330,200,40],[330,300,60],[294,200,40],[294,200,40],[330,200,40],[262,200,40],[294,200,40],[330,200,40],[349,200,40],[262,300,60],[330,200,40],[262,200,40],[330,200,40],[294,300,60]];
const MARIO=[[659,100,30],[659,100,30],[0,100,0],[659,100,30],[0,100,0],[523,100,30],[659,100,30],[0,150,0],[784,100,30],[0,300,0],[392,100,30],[0,300,0],[523,200,40],[0,200,0],[392,200,40],[0,200,0],[330,200,40],[0,200,0],[440,150,30],[0,100,0],[494,100,30],[0,100,0],[466,100,30],[440,200,40]];
const LAN_HUA_CAO=[[330,530,60],[330,530,60],[330,530,60],[294,530,60],[262,530,60],[294,530,60],[330,530,60],[392,530,60],[440,530,60],[392,530,60],[330,530,60],[392,530,60],[440,1920,80],[262,530,60],[262,530,60],[262,530,60],[440,530,60],[392,530,60],[440,530,60],[392,530,60],[330,530,60],[294,530,60],[330,530,60],[294,530,60],[262,530,60],[294,1920,80]];
function show(s,t,c){var e=document.getElementById('st');e.style.display='block';e.textContent=s;e.className='status '+c}
async function playMelody(notes){show('Playing...','ok');try{var r=await fetch('/api/melody',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({notes:notes})});if(r.ok)show('Done','ok');else show('Error','err')}catch(e){show(''+e.message,'err')}}
async function beep(f){try{await fetch('/api/beep',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({freq:f,dur:200})})}catch(e){}}
</script></body></html>)rawliteral";
  AsyncWebServerResponse* resp = req->beginResponse(200, "text/html; charset=utf-8", html);
  addCors(resp);
  req->send(resp);
}

void webServerInit() {
  if (!LittleFS.begin(true)) {
    Serial.println("[web] LittleFS mount FAILED");
  }
  if (MDNS.begin("fairino")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[web] mDNS: fairino.local");
  }
  server.on("/api/*", HTTP_OPTIONS, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse(204);
    addCors(r);
    req->send(r);
  });
  server.on("/api/status",     HTTP_GET,  handleStatus);
  server.on("/api/brightness", HTTP_POST, [](AsyncWebServerRequest* req, JsonVariant& json) { handleBrightness(req, json); });
  server.on("/api/sound",      HTTP_POST, [](AsyncWebServerRequest* req, JsonVariant& json) { handleSound(req, json); });
  server.on("/api/wifi",       HTTP_POST, [](AsyncWebServerRequest* req, JsonVariant& json) { handleWifi(req, json); });
  server.on("/api/beep",       HTTP_POST, [](AsyncWebServerRequest* req, JsonVariant& json) { handleBeep(req, json); });
  server.on("/api/melody",     HTTP_POST, [](AsyncWebServerRequest* req, JsonVariant& json) { handleMelody(req, json); });
  server.on("/melody", HTTP_GET, handleMelodyPage);
  server.on("/api/log", HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse(200, "application/json", webLogGet());
    addCors(r);
    req->send(r);
  });
  server.serveStatic("/", LittleFS, "/web/").setDefaultFile("index.html");
  server.onNotFound([](AsyncWebServerRequest* req) {
    if (req->url().startsWith("/api/")) {
      AsyncWebServerResponse* r = req->beginResponse(404, "application/json", "{\"error\":\"not found\"}");
      addCors(r);
      req->send(r);
      return;
    }
    if (LittleFS.exists("/web/index.html")) {
      AsyncWebServerResponse* r = req->beginResponse(LittleFS, "/web/index.html", "text/html");
      addCors(r);
      req->send(r);
    } else {
      req->send(404, "text/plain", "404 Not Found");
    }
  });
  server.begin();
  s_webRunning = true;
  Serial.println("[web] HTTP server started on port 80");
}

bool webServerRunning() { return s_webRunning && wifiMgrConnected(); }
