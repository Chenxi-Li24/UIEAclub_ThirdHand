// WiFi Manager — state machine, auto-connect, static IP
#include "wifi_manager.h"
#include <WiFi.h>
#include <esp_wifi.h>

// ── Credential storage (in-memory, no NVS) ──────────────────────────
static char _wifiSsid[64]  = {0};
static char _wifiPass[128] = {0};

static void wifiCredSave(const char* ssid, const char* pass) {
    strncpy(_wifiSsid, ssid, sizeof(_wifiSsid)-1);
    strncpy(_wifiPass, pass, sizeof(_wifiPass)-1);
}
static bool wifiCredHas()           { return _wifiSsid[0] != 0; }
static const char* wifiCredSsid()   { return _wifiSsid; }

static WifiMgrState  s_state        = WM_IDLE;
static uint32_t      s_connectStart = 0;
static uint8_t       s_retryCount   = 0;
static const uint8_t MAX_RETRIES    = 3;
static const uint32_t CONNECT_TIMEOUT_MS = 15000;
static const uint32_t FAIL_COOLDOWN_MS   = 30000;

static String s_localIP;
static int    s_rssi = 0;
static bool   s_connected = false;
static bool   s_staticIP = false;
static IPAddress s_staticAddr, s_staticGw, s_staticMask;

static void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.printf("[WiFi] STA connected to %s\n",
        (const char*)info.wifi_sta_connected.ssid);
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      s_localIP  = IPAddress(info.got_ip.ip_info.ip.addr).toString();
      s_rssi     = WiFi.RSSI();
      s_connected = true;
      s_state    = WM_OK;
      s_retryCount = 0;
      Serial.printf("[WiFi] OK  IP: %s  RSSI: %d\n", s_localIP.c_str(), s_rssi);
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      s_connected = false;
      s_localIP   = "";
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
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(onWifiEvent);
}

void wifiMgrTick() {
  uint32_t now = millis();
  switch (s_state) {
    case WM_AUTO_CONNECT:
    case WM_CONNECTING:
      if (s_connected) break;
      if (now - s_connectStart > CONNECT_TIMEOUT_MS) {
        s_state  = WM_FAIL;
        s_connected = false;
        s_retryCount++;
        Serial.printf("[WiFi] Connect timeout (%d/%d)\n", s_retryCount, MAX_RETRIES);
        s_connectStart = now;
        WiFi.disconnect();
      }
      break;
    case WM_FAIL:
      if (now - s_connectStart > FAIL_COOLDOWN_MS) {
        if (s_retryCount < MAX_RETRIES && wifiCredHas()) {
          Serial.printf("[WiFi] Retry %d/%d (%s)...\n", s_retryCount + 1, MAX_RETRIES,
                        s_staticIP ? "static" : "DHCP");
          s_state = WM_AUTO_CONNECT;
          s_connectStart = now;
          if (s_staticIP) {
            WiFi.config(s_staticAddr, s_staticGw, s_staticMask);
          }
          WiFi.begin(_wifiSsid, _wifiPass);
        } else {
          Serial.println("[WiFi] All attempts exhausted");
          s_state = WM_IDLE;
          s_retryCount = 0;
        }
      }
      break;
    default: break;
  }
}

void wifiMgrConnect(const char* ssid, const char* pass) {
  wifiCredSave(ssid, pass);
  s_staticIP = false;
  s_state = WM_CONNECTING;
  s_connectStart = millis();
  s_retryCount = 0;
  Serial.printf("[WiFi] Connecting (DHCP) to '%s'...\n", ssid);
  WiFi.begin(ssid, pass);
}

void wifiMgrConnectStatic(const char* ssid, const char* pass,
                          const char* ip, const char* gateway, const char* subnet) {
  wifiCredSave(ssid, pass);
  s_staticAddr.fromString(ip);
  s_staticGw.fromString(gateway);
  s_staticMask.fromString(subnet);
  s_staticIP = true;
  s_state = WM_CONNECTING;
  s_connectStart = millis();
  s_retryCount = 0;
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
  s_retryCount = 0;
  Serial.printf("[WiFi] Reconnecting (STATIC %s) to '%s'...\n", ip, wifiCredSsid());
  WiFi.config(s_staticAddr, s_staticGw, s_staticMask);
  WiFi.begin(_wifiSsid, _wifiPass);
}

void wifiMgrDisconnect() {
  WiFi.disconnect();
  s_state      = WM_IDLE;
  s_connected   = false;
  s_localIP     = "";
  s_retryCount  = 0;
  s_staticIP    = false;
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
      if (c == 0x22) { json += (char)0x5C; } // escape double-quote
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

WifiMgrState wifiMgrState()    { return s_state; }
String       wifiMgrLocalIP()  { return s_localIP; }
int          wifiMgrRssi()     { return s_rssi; }
bool         wifiMgrConnected() { return s_connected; }
bool         wifiMgrIsStatic()  { return s_staticIP; }
