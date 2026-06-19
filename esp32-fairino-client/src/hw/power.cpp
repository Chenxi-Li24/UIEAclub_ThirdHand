// src/hw/power.cpp — 18650 电池分压 ADC 检测

#include "hw/power.h"

#include "hw/pins.h"

#include <Arduino.h>

static const float R1 = 100.0f;
static const float R2 = 100.0f;
static const float DIV = (R1 + R2) / R2;  // = 2.0

bool hwPowerInit() {
  analogReadResolution(12);
  pinMode(PIN_VBAT_ADC, INPUT);
  pinMode(PIN_PWR_KEY, INPUT_PULLUP);
  Serial.println("hwPower: 18650 ADC OK");
  return true;
}

HwBattery hwBattery() {
  HwBattery b = {};
  int raw = analogRead(PIN_VBAT_ADC);

  // Detect floating ADC (no battery connected): readings oscillate wildly
  static int lastRaw = -1;
  static bool noBat = false;
  if (lastRaw >= 0 && abs(raw - lastRaw) > 800)
    noBat = true;
  if (raw < 200)
    noBat = true;  // near-zero = definitely no battery
  lastRaw = raw;

  if (noBat) {
    b.mV = 0;
    b.pct = -1;  // -1 = no battery / USB powered
    b.usbPresent = true;
    return b;
  }

  float vPin = (raw / 4095.0f) * 3.1f;  // ESP32-S3 ADC ref ≈ 3.1V
  float vBat = vPin * DIV;
  b.mV = (int)(vBat * 1000);
  b.pct = constrain((b.mV - 3200) / 10, 0, 100);
  b.usbPresent = (vBat > 4.5f);
  return b;
}

void hwPowerOff() {
  Serial.println("hwPower: shutdown...");
  esp_deep_sleep_start();
}
