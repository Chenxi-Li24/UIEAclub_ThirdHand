#pragma once

#include <Arduino.h>

struct Settings {
  bool sound;
  bool led;
  uint8_t brightness;
};

void settingsLoad();
void settingsSave();
Settings& settings();

#define WIFI_MAX_PROFILES 5

void wifiCredLoad();
void wifiCredAddTop(const char* ssid, const char* pass);
void wifiCredClear();
int wifiCredCount();
const char* wifiCredSsid(int idx);
const char* wifiCredPass(int idx);
bool wifiCredHas();
