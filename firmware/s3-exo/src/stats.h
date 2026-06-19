#pragma once
// Robot Controller NVS Settings — WiFi credentials + basic settings
// Header-only with file-static state: include from exactly one translation unit (main.cpp).
#include <Arduino.h>
#include <Preferences.h>

// ── Settings ────────────────────────────────────────────────────────────

struct Settings {
  bool sound;            // buzzer enabled
  bool led;              // WS2812 enabled
  uint8_t brightness;    // 0-4 screen brightness
};

static Settings _settings = {true, true, 2};
static Preferences _prefs;

inline void settingsLoad() {
  _prefs.begin("robot", true);
  _settings.sound      = _prefs.getBool("s_snd", true);
  _settings.led        = _prefs.getBool("s_led", true);
  _settings.brightness = _prefs.getUChar("s_bri", 2);
  if (_settings.brightness > 4) _settings.brightness = 2;
  _prefs.end();
}

inline void settingsSave() {
  _prefs.begin("robot", false);
  _prefs.putBool("s_snd", _settings.sound);
  _prefs.putBool("s_led", _settings.led);
  _prefs.putUChar("s_bri", _settings.brightness);
  _prefs.end();
}

inline Settings& settings() { return _settings; }

// ── WiFi Profiles (up to 5, index 0 = highest priority) ──────────────────
// NVS keys: w_n (count), w_s0..w_s4 (SSID), w_p0..w_p4 (password)
// Migrates old single-key format (w_ssid/w_pass) on first load.

#define WIFI_MAX_PROFILES 5

static char _wifiProfSsid[WIFI_MAX_PROFILES][33] = {{0}};
static char _wifiProfPass[WIFI_MAX_PROFILES][65] = {{0}};
static uint8_t _wifiProfCount = 0;

inline void wifiCredLoad() {
  memset(_wifiProfSsid, 0, sizeof(_wifiProfSsid));
  memset(_wifiProfPass, 0, sizeof(_wifiProfPass));
  _prefs.begin("robot", true);
  _wifiProfCount = _prefs.getUChar("w_n", 0);
  if (_wifiProfCount > WIFI_MAX_PROFILES) _wifiProfCount = WIFI_MAX_PROFILES;

  // Migration: old single-key format -> multi-profile
  String oldSsid = _prefs.getString("w_ssid", "");
  if (oldSsid.length() > 0 && _wifiProfCount == 0) {
    String oldPass = _prefs.getString("w_pass", "");
    strncpy(_wifiProfSsid[0], oldSsid.c_str(), 32);
    strncpy(_wifiProfPass[0], oldPass.c_str(), 64);
    _wifiProfCount = 1;
    _prefs.end();
    _prefs.begin("robot", false);
    _prefs.putUChar("w_n", 1);
    _prefs.putString("w_s0", _wifiProfSsid[0]);
    _prefs.putString("w_p0", _wifiProfPass[0]);
    _prefs.putString("w_ssid", "");
    _prefs.putString("w_pass", "");
    _prefs.end();
    Serial.printf("[WiFi] Migrated old credential '%s' to multi-profile\n", _wifiProfSsid[0]);
    return;
  }

  for (int i = 0; i < _wifiProfCount; i++) {
    char k[8];
    snprintf(k, sizeof(k), "w_s%d", i);
    _prefs.getString(k, _wifiProfSsid[i], 33);
    snprintf(k, sizeof(k), "w_p%d", i);
    _prefs.getString(k, _wifiProfPass[i], 65);
  }
  _prefs.end();
}

inline void _wifiCredFlush() {
  _prefs.begin("robot", false);
  _prefs.putUChar("w_n", _wifiProfCount);
  for (int i = 0; i < _wifiProfCount; i++) {
    char k[8];
    snprintf(k, sizeof(k), "w_s%d", i);
    _prefs.putString(k, _wifiProfSsid[i]);
    snprintf(k, sizeof(k), "w_p%d", i);
    _prefs.putString(k, _wifiProfPass[i]);
  }
  _prefs.end();
}

inline void wifiCredAddTop(const char* ssid, const char* pass) {
  if (!ssid || !ssid[0]) return;
  for (int i = 0; i < _wifiProfCount; i++) {
    if (strcmp(_wifiProfSsid[i], ssid) == 0) {
      char tS[33], tP[65];
      strncpy(tS, _wifiProfSsid[i], 32); tS[32] = 0;
      strncpy(tP, _wifiProfPass[i], 64); tP[64] = 0;
      for (int j = i; j > 0; j--) {
        strncpy(_wifiProfSsid[j], _wifiProfSsid[j - 1], 32);
        strncpy(_wifiProfPass[j], _wifiProfPass[j - 1], 64);
      }
      strncpy(_wifiProfSsid[0], tS, 32);
      strncpy(_wifiProfPass[0], tP, 64);
      _wifiCredFlush();
      return;
    }
  }
  int n = _wifiProfCount < WIFI_MAX_PROFILES ? _wifiProfCount : WIFI_MAX_PROFILES - 1;
  for (int j = n; j > 0; j--) {
    strncpy(_wifiProfSsid[j], _wifiProfSsid[j - 1], 32);
    strncpy(_wifiProfPass[j], _wifiProfPass[j - 1], 64);
  }
  strncpy(_wifiProfSsid[0], ssid, 32); _wifiProfSsid[0][32] = 0;
  strncpy(_wifiProfPass[0], pass ? pass : "", 64); _wifiProfPass[0][64] = 0;
  if (_wifiProfCount < WIFI_MAX_PROFILES) _wifiProfCount++;
  _wifiCredFlush();
}

inline void wifiCredClear() {
  memset(_wifiProfSsid, 0, sizeof(_wifiProfSsid));
  memset(_wifiProfPass, 0, sizeof(_wifiProfPass));
  _wifiProfCount = 0;
  _prefs.begin("robot", false);
  _prefs.putUChar("w_n", 0);
  for (int i = 0; i < WIFI_MAX_PROFILES; i++) {
    char k[8];
    snprintf(k, sizeof(k), "w_s%d", i);
    _prefs.putString(k, "");
    snprintf(k, sizeof(k), "w_p%d", i);
    _prefs.putString(k, "");
  }
  _prefs.end();
}

inline int wifiCredCount() { return _wifiProfCount; }
inline const char* wifiCredSsid(int idx) {
  return (idx >= 0 && idx < _wifiProfCount) ? _wifiProfSsid[idx] : "";
}
inline const char* wifiCredPass(int idx) {
  return (idx >= 0 && idx < _wifiProfCount) ? _wifiProfPass[idx] : "";
}
inline bool wifiCredHas() { return _wifiProfCount > 0 && _wifiProfSsid[0][0] != 0; }
