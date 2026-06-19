#pragma once
// Robot WiFi Manager — state machine, auto-connect, scan, static IP
#include <Arduino.h>
#include <functional>

enum WifiMgrState {
  WM_IDLE,           // no saved creds or all attempts exhausted
  WM_AUTO_CONNECT,   // trying saved creds (auto-retry up to 3x)
  WM_CONNECTING,     // explicit connect via API
  WM_OK,             // connected
  WM_FAIL            // last attempt failed, will retry
};

void wifiMgrInit();
void wifiMgrTick();
void wifiMgrConnect(const char* ssid, const char* pass);
void wifiMgrConnectStatic(const char* ssid, const char* pass,
                          const char* ip, const char* gateway, const char* subnet);
void wifiMgrReconnectStatic(const char* ip, const char* gateway, const char* subnet);
void wifiMgrDisconnect();
void wifiMgrScan(std::function<void(String json)> callback);

WifiMgrState wifiMgrState();
String       wifiMgrLocalIP();
int          wifiMgrRssi();
bool         wifiMgrConnected();
bool         wifiMgrIsStatic();
