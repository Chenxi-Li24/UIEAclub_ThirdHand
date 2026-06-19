// src/hw/power.h — 18650 电池检测

#pragma once
#include <stdint.h>

struct HwBattery {
  int mV, pct;
  bool usbPresent;
};

bool hwPowerInit();
HwBattery hwBattery();
void hwPowerOff();
