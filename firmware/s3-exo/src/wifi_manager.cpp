// Robot WiFi Manager — deskpet-based state machine + static IP support
#include "wifi_manager.h"
#include "stats.h"
#include <WiFi.h>
#include <freertos/semphr.h>
#include <utility>

static WifiMgrState s_state = WM_IDLE;
static int8_t s_curProfile = -1;
static uint32_t s_connectStart = 0;
static uint8_t s_retryCount = 0;
static const uint8_t MAX_RETRIES = 3;
static const uint32_t CONNECT_TIMEOUT_MS = 8000;
static const uint32_t PROFILE_COOLDOWN_MS = 2000;

static String s_localIP;
static int s_rssi = 0;
static volatile bool s_connected = false;

enum ScanState : uint8_t { SCAN_IDLE, SCAN_REQUESTED, SCAN_RUNNING };
static SemaphoreHandle_t s_scanMutex = nullptr;
static volatile ScanState s_scanState = SCAN_IDLE;
static std::function<void(String)> s_scanCallback;
static WifiScanEntry s_scanResults[WIFI_SCAN_MAX_RESULTS] = {};
static uint8_t s_scanCount = 0;
static uint32_t s_scanStarted = 0;

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
      s_retryCount = 0;
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
      Serial.printf("[WiFi] STA disconnected, reason=%d\n",
                    info.wifi_sta_disconnected.reason);
      if (s_state == WM_OK) {
        s_state = WM_AUTO_CONNECT;
        s_connectStart = millis();
        s_retryCount = 0;
        Serial.println("[WiFi] Disconnected  will auto-reconnect");
      }
      break;
    default: break;
  }
}

void wifiMgrInit(bool autoConnect) {
  if (!s_scanMutex) s_scanMutex = xSemaphoreCreateMutex();
  wifiCredLoad();
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(onWifiEvent);
  if (autoConnect && wifiCredHas()) {
    s_curProfile = 0;
    s_state = WM_AUTO_CONNECT;
    s_connectStart = millis();
    Serial.printf("[WiFi] Auto-connect profile 1/%d: '%s'\n", wifiCredCount(), wifiCredSsid(0));
    if (s_staticIP) WiFi.config(s_staticAddr, s_staticGw, s_staticMask);
    WiFi.begin(wifiCredSsid(0), wifiCredPass(0));
  } else if (!wifiCredHas()) {
    Serial.println("[WiFi] No saved credentials — idle");
    s_state = WM_IDLE;
  } else {
    Serial.printf("[WiFi] Loaded %d saved profile(s); explicit connect pending\n",
                  wifiCredCount());
    s_state = WM_IDLE;
  }
}

static bool scanResultExists(const char* ssid) {
  for (uint8_t i = 0; i < s_scanCount; i++) {
    if (strncmp(s_scanResults[i].ssid, ssid, sizeof(s_scanResults[i].ssid)) == 0)
      return true;
  }
  return false;
}

static String finishScan(int networkCount) {
  s_scanCount = 0;
  if (networkCount > 0) {
    for (int i = 0; i < networkCount && s_scanCount < WIFI_SCAN_MAX_RESULTS; i++) {
      const String ssid = WiFi.SSID(i);
      if (ssid.isEmpty() || scanResultExists(ssid.c_str())) continue;

      WifiScanEntry& entry = s_scanResults[s_scanCount++];
      strncpy(entry.ssid, ssid.c_str(), sizeof(entry.ssid) - 1);
      entry.ssid[sizeof(entry.ssid) - 1] = 0;
      entry.rssi = WiFi.RSSI(i);
      entry.channel = static_cast<uint8_t>(WiFi.channel(i));
      entry.secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
  }

  String json;
  json.reserve(2 + s_scanCount * 80);
  json += '[';
  for (uint8_t i = 0; i < s_scanCount; i++) {
    if (i) json += ',';
    json += F("{\"ssid\":\"");
    for (const char* p = s_scanResults[i].ssid; *p; p++) {
      if (*p == '\\' || *p == '"') json += '\\';
      json += *p;
    }
    json += F("\",\"rssi\":");
    json += s_scanResults[i].rssi;
    json += F(",\"secure\":");
    json += s_scanResults[i].secure ? F("true") : F("false");
    json += F(",\"channel\":");
    json += static_cast<unsigned int>(s_scanResults[i].channel);
    json += '}';
  }
  json += ']';
  return json;
}

static void wifiMgrScanTick() {
  bool startScan = false;
  if (s_scanMutex && xSemaphoreTake(s_scanMutex, 0) == pdTRUE) {
    if (s_scanState == SCAN_REQUESTED) {
      s_scanState = SCAN_RUNNING;
      s_scanStarted = millis();
      startScan = true;
    }
    xSemaphoreGive(s_scanMutex);
  }

  if (startScan) {
    WiFi.scanDelete();
    // Keep the Arduino core's default 300 ms/channel timeout. A shorter 120
    // ms value makes its global timeout exactly 2.4 s and can expire just as
    // an active 13-channel scan finishes.
    const int result = WiFi.scanNetworks(true, true, false, 300);
    Serial.printf("[WiFi] Async scan started, heap=%u, result=%d\n",
                  ESP.getFreeHeap(), result);
    if (result == WIFI_SCAN_FAILED) {
      WiFi.scanDelete();
    } else {
      return;
    }
  }

  if (s_scanState != SCAN_RUNNING) return;
  int result = WiFi.scanComplete();
  if (result == WIFI_SCAN_RUNNING && millis() - s_scanStarted <= 15000) return;
  if (result == WIFI_SCAN_RUNNING) result = WIFI_SCAN_FAILED;

  String json = finishScan(result > 0 ? result : 0);
  WiFi.scanDelete();

  std::function<void(String)> callback;
  if (s_scanMutex && xSemaphoreTake(s_scanMutex, portMAX_DELAY) == pdTRUE) {
    callback = std::move(s_scanCallback);
    s_scanState = SCAN_IDLE;
    xSemaphoreGive(s_scanMutex);
  }
  Serial.printf("[WiFi] Scan complete: raw=%d shown=%u heap=%u\n", result,
                s_scanCount, ESP.getFreeHeap());
  if (callback) callback(json);
}

void wifiMgrTick() {
  wifiMgrScanTick();
  uint32_t now = millis();
  switch (s_state) {
    case WM_AUTO_CONNECT:
    case WM_CONNECTING:
      if (s_connected) break;
      if (now - s_connectStart > CONNECT_TIMEOUT_MS) {
        s_state = WM_FAIL;
        s_connected = false;
        s_retryCount++;
        Serial.printf("[WiFi] Timeout connecting to '%s'\n",
                      s_curProfile >= 0 ? wifiCredSsid(s_curProfile) : "?");
        s_connectStart = now;
        WiFi.disconnect();
      }
      break;
    case WM_FAIL:
      if (now - s_connectStart > PROFILE_COOLDOWN_MS) {
        if (s_curProfile >= 0 && s_retryCount < MAX_RETRIES) {
          s_state = WM_AUTO_CONNECT;
          s_connectStart = now;
          Serial.printf("[WiFi] Retry profile %d/%d, attempt %d/%d: '%s'\n",
                        s_curProfile + 1, wifiCredCount(), s_retryCount + 1,
                        MAX_RETRIES, wifiCredSsid(s_curProfile));
          if (s_staticIP) WiFi.config(s_staticAddr, s_staticGw, s_staticMask);
          WiFi.begin(wifiCredSsid(s_curProfile), wifiCredPass(s_curProfile));
        } else if (s_curProfile >= 0 && s_curProfile + 1 < wifiCredCount()) {
          s_curProfile++;
          s_retryCount = 0;
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
  s_retryCount = 0;
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
  s_retryCount = 0;
  Serial.println("[WiFi] Disconnected by user");
}

bool wifiMgrScan(std::function<void(String json)> callback) {
  if (!s_scanMutex) return false;
  if (xSemaphoreTake(s_scanMutex, pdMS_TO_TICKS(20)) != pdTRUE) return false;
  if (s_scanState != SCAN_IDLE) {
    xSemaphoreGive(s_scanMutex);
    return false;
  }
  s_scanCallback = std::move(callback);
  s_scanState = SCAN_REQUESTED;
  xSemaphoreGive(s_scanMutex);
  return true;
}

bool wifiMgrScanBusy() {
  if (!s_scanMutex) return false;
  bool busy = true;
  if (xSemaphoreTake(s_scanMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    busy = s_scanState != SCAN_IDLE;
    xSemaphoreGive(s_scanMutex);
  }
  return busy;
}

uint8_t wifiMgrScanCount() { return s_scanCount; }

bool wifiMgrScanGet(uint8_t index, WifiScanEntry& entry) {
  if (index >= s_scanCount) return false;
  entry = s_scanResults[index];
  return true;
}

WifiMgrState wifiMgrState()      { return s_state; }
String       wifiMgrLocalIP()    { return s_connected ? s_localIP : ""; }
int          wifiMgrRssi()       { return s_rssi; }
bool         wifiMgrConnected()  { return s_connected; }
bool         wifiMgrIsStatic()   { return s_staticIP; }
