#include "stats.h"

#include <Preferences.h>
#include <cstring>

static Settings s_settings = {true, true, 2};
static Preferences s_prefs;

static char s_wifiProfSsid[WIFI_MAX_PROFILES][33] = {{0}};
static char s_wifiProfPass[WIFI_MAX_PROFILES][65] = {{0}};
static uint8_t s_wifiProfCount = 0;

void settingsLoad() {
  s_prefs.begin("robot", true);
  s_settings.sound = s_prefs.getBool("s_snd", true);
  s_settings.led = s_prefs.getBool("s_led", true);
  s_settings.brightness = s_prefs.getUChar("s_bri", 2);
  if (s_settings.brightness > 4) s_settings.brightness = 2;
  s_prefs.end();
}

void settingsSave() {
  s_prefs.begin("robot", false);
  s_prefs.putBool("s_snd", s_settings.sound);
  s_prefs.putBool("s_led", s_settings.led);
  s_prefs.putUChar("s_bri", s_settings.brightness);
  s_prefs.end();
}

Settings& settings() { return s_settings; }

void wifiCredLoad() {
  memset(s_wifiProfSsid, 0, sizeof(s_wifiProfSsid));
  memset(s_wifiProfPass, 0, sizeof(s_wifiProfPass));
  s_prefs.begin("robot", true);
  s_wifiProfCount = s_prefs.getUChar("w_n", 0);
  if (s_wifiProfCount > WIFI_MAX_PROFILES) s_wifiProfCount = WIFI_MAX_PROFILES;

  // Migrate the previous single-profile format when present.
  String oldSsid;
  if (s_prefs.isKey("w_ssid")) oldSsid = s_prefs.getString("w_ssid", "");
  if (oldSsid.length() > 0 && s_wifiProfCount == 0) {
    String oldPass;
    if (s_prefs.isKey("w_pass")) oldPass = s_prefs.getString("w_pass", "");
    strncpy(s_wifiProfSsid[0], oldSsid.c_str(), 32);
    strncpy(s_wifiProfPass[0], oldPass.c_str(), 64);
    s_wifiProfCount = 1;
    s_prefs.end();
    s_prefs.begin("robot", false);
    s_prefs.putUChar("w_n", 1);
    s_prefs.putString("w_s0", s_wifiProfSsid[0]);
    s_prefs.putString("w_p0", s_wifiProfPass[0]);
    s_prefs.remove("w_ssid");
    s_prefs.remove("w_pass");
    s_prefs.end();
    Serial.printf("[WiFi] Migrated old credential '%s' to multi-profile\n",
                  s_wifiProfSsid[0]);
    return;
  }

  for (int i = 0; i < s_wifiProfCount; i++) {
    char key[8];
    snprintf(key, sizeof(key), "w_s%d", i);
    if (s_prefs.isKey(key)) s_prefs.getString(key, s_wifiProfSsid[i], 33);
    snprintf(key, sizeof(key), "w_p%d", i);
    if (s_prefs.isKey(key)) s_prefs.getString(key, s_wifiProfPass[i], 65);
  }
  s_prefs.end();
}

static void wifiCredFlush() {
  s_prefs.begin("robot", false);
  s_prefs.putUChar("w_n", s_wifiProfCount);
  for (int i = 0; i < s_wifiProfCount; i++) {
    char key[8];
    snprintf(key, sizeof(key), "w_s%d", i);
    s_prefs.putString(key, s_wifiProfSsid[i]);
    snprintf(key, sizeof(key), "w_p%d", i);
    s_prefs.putString(key, s_wifiProfPass[i]);
  }
  s_prefs.end();
}

void wifiCredAddTop(const char* ssid, const char* pass) {
  if (!ssid || !ssid[0]) return;

  for (int i = 0; i < s_wifiProfCount; i++) {
    if (strcmp(s_wifiProfSsid[i], ssid) == 0) {
      char newSsid[33], newPass[65];
      // Copy first because the arguments may point into the profile arrays.
      strncpy(newSsid, ssid, 32); newSsid[32] = 0;
      strncpy(newPass, pass ? pass : "", 64); newPass[64] = 0;
      for (int j = i; j > 0; j--) {
        memcpy(s_wifiProfSsid[j], s_wifiProfSsid[j - 1], 33);
        memcpy(s_wifiProfPass[j], s_wifiProfPass[j - 1], 65);
      }
      memcpy(s_wifiProfSsid[0], newSsid, 33);
      memcpy(s_wifiProfPass[0], newPass, 65);
      wifiCredFlush();
      return;
    }
  }

  int last = s_wifiProfCount < WIFI_MAX_PROFILES
               ? s_wifiProfCount
               : WIFI_MAX_PROFILES - 1;
  for (int i = last; i > 0; i--) {
    memcpy(s_wifiProfSsid[i], s_wifiProfSsid[i - 1], 33);
    memcpy(s_wifiProfPass[i], s_wifiProfPass[i - 1], 65);
  }
  strncpy(s_wifiProfSsid[0], ssid, 32); s_wifiProfSsid[0][32] = 0;
  strncpy(s_wifiProfPass[0], pass ? pass : "", 64); s_wifiProfPass[0][64] = 0;
  if (s_wifiProfCount < WIFI_MAX_PROFILES) s_wifiProfCount++;
  wifiCredFlush();
}

void wifiCredClear() {
  memset(s_wifiProfSsid, 0, sizeof(s_wifiProfSsid));
  memset(s_wifiProfPass, 0, sizeof(s_wifiProfPass));
  s_wifiProfCount = 0;
  s_prefs.begin("robot", false);
  s_prefs.putUChar("w_n", 0);
  for (int i = 0; i < WIFI_MAX_PROFILES; i++) {
    char key[8];
    snprintf(key, sizeof(key), "w_s%d", i);
    s_prefs.remove(key);
    snprintf(key, sizeof(key), "w_p%d", i);
    s_prefs.remove(key);
  }
  s_prefs.end();
}

int wifiCredCount() { return s_wifiProfCount; }

const char* wifiCredSsid(int idx) {
  return (idx >= 0 && idx < s_wifiProfCount) ? s_wifiProfSsid[idx] : "";
}

const char* wifiCredPass(int idx) {
  return (idx >= 0 && idx < s_wifiProfCount) ? s_wifiProfPass[idx] : "";
}

bool wifiCredHas() {
  return s_wifiProfCount > 0 && s_wifiProfSsid[0][0] != 0;
}
