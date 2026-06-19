// Robot Web Server — REST API
#pragma once
#include <Arduino.h>

void webServerInit();
bool webServerRunning();

// Circular log buffer for /api/log
void webLog(const char* fmt, ...);
String webLogGet();
