#include "hw/input.h"

#include "hw/pins.h"

#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>

#define CST816T_ADDR 0x15
#define REG_GESTURE_ID 0x01
#define REG_FINGER_NUM 0x02

static bool s_touchOk = false;
static uint8_t s_readFailures = 0;
static bool s_recoverPending = false;
static uint32_t s_lastRecovery = 0;
static bool s_rawPressed = false;
static bool s_rawDragging = false;
static uint16_t s_pressX = 0;
static uint16_t s_pressY = 0;
static uint32_t s_blockClicksUntil = 0;

static constexpr uint16_t DRAG_THRESHOLD_PX = 12;
static constexpr uint32_t DRAG_CLICK_GUARD_MS = 300;

static void noteTouchRelease() {
  if (!s_rawPressed) return;
  if (s_rawDragging) s_blockClicksUntil = millis() + DRAG_CLICK_GUARD_MS;
  s_rawPressed = false;
  s_rawDragging = false;
}

static void noteTouchPress(uint16_t x, uint16_t y) {
  if (!s_rawPressed) {
    s_rawPressed = true;
    s_rawDragging = false;
    s_pressX = x;
    s_pressY = y;
    return;
  }

  const uint16_t dx = x > s_pressX ? x - s_pressX : s_pressX - x;
  const uint16_t dy = y > s_pressY ? y - s_pressY : s_pressY - y;
  if (dx >= DRAG_THRESHOLD_PX || dy >= DRAG_THRESHOLD_PX) {
    s_rawDragging = true;
  }
}

static void markReadFailure() {
  if (s_readFailures < UINT8_MAX) s_readFailures++;
  if (s_readFailures >= 8) {
    s_touchOk = false;
    s_recoverPending = true;
  }
}
static bool probeTouchController(bool hardwareReset) {
  if (hardwareReset) {
    pinMode(PIN_TP_RST, OUTPUT);
    digitalWrite(PIN_TP_RST, LOW);
    delay(10);
    digitalWrite(PIN_TP_RST, HIGH);
    delay(50);
  }

  Wire.beginTransmission(CST816T_ADDR);
  Wire.write(REG_GESTURE_ID);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)CST816T_ADDR, (uint8_t)1) != 1) return false;
  const uint8_t gesture = Wire.read();
  Serial.printf("hwInput: CST816T OK (gesture=0x%02X)\n", gesture);
  return true;
}

static void touchRead(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  (void)drv;
  if (!s_touchOk) {
    noteTouchRelease();
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  // Read finger count and X/Y in a single I2C transaction (registers 0x02-0x06).
  Wire.beginTransmission(CST816T_ADDR);
  Wire.write(REG_FINGER_NUM);
  if (Wire.endTransmission(false) != 0) {
    markReadFailure();
    noteTouchRelease();
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  Wire.requestFrom((uint8_t)CST816T_ADDR, (uint8_t)5);
  if (Wire.available() < 5) {
    markReadFailure();
    noteTouchRelease();
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  const uint8_t fingers = Wire.read();
  const uint8_t xH = Wire.read();
  const uint8_t xL = Wire.read();
  const uint8_t yH = Wire.read();
  const uint8_t yL = Wire.read();
  s_readFailures = 0;

  if (fingers > 0) {
    const uint16_t tx = ((xH & 0x0F) << 8) | xL;
    const uint16_t ty = ((yH & 0x0F) << 8) | yL;
    if (tx < LCD_PHYS_W && ty < LCD_PHYS_H) {
      noteTouchPress(tx, ty);
      data->state = LV_INDEV_STATE_PR;
      data->point.x = tx;
      data->point.y = ty;
      return;
    }
  }
  noteTouchRelease();
  data->state = LV_INDEV_STATE_REL;
}

bool hwInputInit() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  s_touchOk = probeTouchController(true);
  s_readFailures = 0;
  s_recoverPending = false;
  if (!s_touchOk) Serial.println("hwInput: CST816T NOT FOUND; touch disabled");

  static lv_indev_drv_t indevDrv;
  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = touchRead;
  lv_indev_drv_register(&indevDrv);
  return s_touchOk;
}

void hwInputUpdate() {
  // Recover only after repeated failures. A single missed I2C sample is normal
  // and should simply be reported to LVGL as a release.
  if (!s_recoverPending || millis() - s_lastRecovery < 1000) return;

  s_recoverPending = false;
  s_lastRecovery = millis();
  Serial.println("hwInput: recovering CST816T/I2C after repeated read failures");
  Wire.end();
  delay(2);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);
  s_touchOk = probeTouchController(true);
  s_readFailures = 0;
  if (!s_touchOk) s_recoverPending = true;
}

bool hwInputClickAllowed() {
  return !s_rawDragging && (int32_t)(millis() - s_blockClicksUntil) >= 0;
}
