// Robot WiFi Manager — deskpet-based state machine + static IP support
#include "wifi_manager.h"
#include "stats.h"
#include <WiFi.h>

static WifiMgrState s_state = WM_IDLE;
static int8_t s_curProfile = -1;
static uint32_t s_connectStart = 0;
static const uint32_t CONNECT_TIMEOUT_MS = 15000;
static const uint32_t PROFILE_COOLDOWN_MS = 5000;

static String s_localIP;
static int s_rssi = 0;
static bool s_connected = false;

// Static IP state
static bool s_staticIP = false;
static IPAddress s_staticAddr, s_staticGw, s_staticMask;

static void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.printf("[WiFi] STA connected to %s\n", (const char*)info.wifi_sta_connected.ssid);
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      s_localIP = IPAddress(info.got_ip.ip_info.ip.addr).toString();
      s_rssi = WiFi.RSSI();
      s_connected = true;
      s_state = WM_OK;
      if (s_curProfile > 0) {
        String curSsid = WiFi.SSID();
        wifiCredAddTop(curSsid.c_str(), wifiCredPass(s_curProfile));
        s_curProfile = 0;
      }
      Serial.printf("[WiFi] OK  IP: %s  RSSI: %d  Static:%s\n",
                    s_localIP.c_str(), s_rssi, s_staticIP ? "yes" : "no");
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      s_connected = false;
      s_localIP = "";
      if (s_state == WM_OK) {
        s_state = WM_AUTO_CONNECT;
        s_connectStart = millis();
        Serial.println("[WiFi] Disconnected  will auto-reconnect");
      }
      break;
    default: break;
  }
}

void wifiMgrInit() {
  wifiCredLoad();
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(onWifiEvent);
  if (wifiCredHas()) {
    s_curProfile = 0;
    s_state = WM_AUTO_CONNECT;
    s_connectStart = millis();
    Serial.printf("[WiFi] Auto-connect profile 1/%d: '%s'\n", wifiCredCount(), wifiCredSsid(0));
    if (s_staticIP) WiFi.config(s_staticAddr, s_staticGw, s_staticMask);
    WiFi.begin(wifiCredSsid(0), wifiCredPass(0));
  } else {
    Serial.println("[WiFi] No saved credentials — idle");
    s_state = WM_IDLE;
  }
}

void wifiMgrTick() {
  uint32_t now = millis();
  switch (s_state) {
    case WM_AUTO_CONNECT:
    case WM_CONNECTING:
      if (s_connected) break;
      if (now - s_connectStart > CONNECT_TIMEOUT_MS) {
        s_state = WM_FAIL;
        s_connected = false;
        Serial.printf("[WiFi] Timeout connecting to '%s'\n",
                      s_curProfile >= 0 ? wifiCredSsid(s_curProfile) : "?");
        s_connectStart = now;
        WiFi.disconnect();
      }
      break;
    case WM_FAIL:
      if (now - s_connectStart > PROFILE_COOLDOWN_MS) {
        if (s_curProfile >= 0 && s_curProfile + 1 < wifiCredCount()) {
          s_curProfile++;
          s_state = WM_AUTO_CONNECT;
          s_connectStart = now;
          Serial.printf("[WiFi] Try profile %d/%d: '%s'\n", s_curProfile + 1, wifiCredCount(),
                        wifiCredSsid(s_curProfile));
          if (s_staticIP) WiFi.config(s_staticAddr, s_staticGw, s_staticMask);
          WiFi.begin(wifiCredSsid(s_curProfile), wifiCredPass(s_curProfile));
        } else {
          Serial.println("[WiFi] All profiles exhausted");
          s_state = WM_IDLE;
          s_curProfile = -1;
        }
      }
      break;
    default: break;
  }
}

void wifiMgrConnect(const char* ssid, const char* pass) {
  wifiCredAddTop(ssid, pass);
  s_staticIP = false;
  s_curProfile = 0;
  s_state = WM_CONNECTING;
  s_connectStart = millis();
  Serial.printf("[WiFi] Connecting (DHCP) to '%s'...\n", ssid);
  WiFi.begin(ssid, pass);
}

void wifiMgrConnectStatic(const char* ssid, const char* pass,
                          const char* ip, const char* gateway, const char* subnet) {
  wifiCredAddTop(ssid, pass);
  s_staticAddr.fromString(ip);
  s_staticGw.fromString(gateway);
  s_staticMask.fromString(subnet);
  s_staticIP = true;
  s_curProfile = 0;
  s_state = WM_CONNECTING;
  s_connectStart = millis();
  Serial.printf("[WiFi] Connecting (STATIC %s) to '%s'...\n", ip, ssid);
  WiFi.config(s_staticAddr, s_staticGw, s_staticMask);
  WiFi.begin(ssid, pass);
}

void wifiMgrReconnectStatic(const char* ip, const char* gateway, const char* subnet) {
  if (!wifiCredHas()) {
    Serial.println("[WiFi] ReconnectStatic: no saved credentials");
    return;
  }
  s_staticAddr.fromString(ip);
  s_staticGw.fromString(gateway);
  s_staticMask.fromString(subnet);
  s_staticIP = true;
  s_state = WM_CONNECTING;
  s_connectStart = millis();
  Serial.printf("[WiFi] Reconnecting (STATIC %s) to '%s'...\n", ip, wifiCredSsid(0));
  WiFi.config(s_staticAddr, s_staticGw, s_staticMask);
  WiFi.begin(wifiCredSsid(0), wifiCredPass(0));
}

void wifiMgrDisconnect() {
  WiFi.disconnect();
  s_state = WM_IDLE;
  s_connected = false;
  s_localIP = "";
  s_curProfile = -1;
  s_staticIP = false;
  Serial.println("[WiFi] Disconnected by user");
}

// Async WiFi scan in FreeRTOS task
struct ScanCtx { std::function<void(String)> cb; };

static void scanTask(void* param) {
  ScanCtx* ctx = (ScanCtx*)param;
  int n = WiFi.scanNetworks(false, true);
  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ',';
    json += "{\"ssid\":\"";
    String ssid = WiFi.SSID(i);
    for (size_t j = 0; j < ssid.length(); j++) {
      char c = ssid[j];
      if (c == 0x22) { json += (char)0x5C; }
      json += c;
    }
    json += "\",\"rssi\":";
    json += WiFi.RSSI(i);
    json += ",\"secure\":";
    json += (WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
    json += ",\"channel\":";
    json += WiFi.channel(i);
    json += '}';
  }
  json += ']';
  WiFi.scanDelete();
  if (ctx->cb) ctx->cb(json);
  delete ctx;
  vTaskDelete(NULL);
}

void wifiMgrScan(std::function<void(String json)> callback) {
  ScanCtx* ctx = new ScanCtx{callback};
  xTaskCreatePinnedToCore(scanTask, "wifiscan", 4096, ctx, 1, NULL, 0);
}

WifiMgrState wifiMgrState()      { return s_state; }
String       wifiMgrLocalIP()    { return s_connected ? s_localIP : ""; }
int          wifiMgrRssi()       { return s_rssi; }
bool         wifiMgrConnected()  { return s_connected; }
bool         wifiMgrIsStatic()   { return s_staticIP; }
