#pragma once
// DeskPet WiFi Manager — state machine, auto-connect, scan
// Must be initialized before esp_now and BLE (sets WiFi mode).
// Drive wifiMgrTick() from loop().

#include <Arduino.h>
#include <functional>

enum WifiMgrState {
  WM_IDLE,           // no saved creds or all attempts exhausted
  WM_AUTO_CONNECT,   // trying saved creds (auto-retry up to 3x)
  WM_CONNECTING,     // explicit connect via API/BLE
  WM_OK,             // connected
  WM_FAIL            // last attempt failed, will retry
};

void wifiMgrInit();                                          // load creds, set WIFI_STA, begin auto-connect if creds exist
void wifiMgrTick();                                          // drive state machine (call from loop)
void wifiMgrConnect(const char* ssid, const char* pass);     // save + connect (explicit trigger, DHCP)
void wifiMgrConnectStatic(const char* ssid, const char* pass,   // save + connect with static IP
                          const char* ip, const char* gateway, const char* subnet);
void wifiMgrReconnectStatic(const char* ip, const char* gateway, const char* subnet);
                          // reconnect using already-saved NVS credentials + static IP
void wifiMgrDisconnect();                                    // disconnect + clear retry counter
void wifiMgrScan(std::function<void(String json)> callback); // async WiFi scan → JSON

WifiMgrState wifiMgrState();
String       wifiMgrLocalIP();
int          wifiMgrRssi();
bool         wifiMgrConnected();
bool         wifiMgrIsStatic();                              // true if using static IP
