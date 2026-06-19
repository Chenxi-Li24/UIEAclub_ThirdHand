// src/hw/input.cpp — CST816T 触摸驱动 → LVGL 输入设备 (直接 I2C 读取)

#include "hw/input.h"

#include "hw/pins.h"

#include <Arduino.h>
#include <Wire.h>

#include <lvgl.h>

// ── CST816T I2C registers ──────────────────────────────────────────────
#define CST816T_ADDR 0x15
#define REG_GESTURE_ID 0x01
#define REG_FINGER_NUM 0x02
#define REG_XPOS_H 0x03
#define REG_XPOS_L 0x04
#define REG_YPOS_H 0x05
#define REG_YPOS_L 0x06

static bool s_touchOk = false;

// ── LVGL touch read callback ────────────────────────────────────────────
static void touchRead(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  if (!s_touchOk) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  Wire.beginTransmission(CST816T_ADDR);
  Wire.write(REG_FINGER_NUM);
  if (Wire.endTransmission(false) != 0) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  Wire.requestFrom((uint8_t)CST816T_ADDR,
                   (uint8_t)5);  // fingers(1) + x_h(1) + x_l(1) + y_h(1) + y_l(1)
  if (Wire.available() < 5) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  uint8_t fingers = Wire.read();
  uint8_t xH = Wire.read();
  uint8_t xL = Wire.read();
  uint8_t yH = Wire.read();
  uint8_t yL = Wire.read();

  if (fingers > 0) {
    uint16_t tx = ((xH & 0x0F) << 8) | xL;
    uint16_t ty = ((yH & 0x0F) << 8) | yL;
    if (tx > 0 && ty > 0) {
      data->state = LV_INDEV_STATE_PR;
      data->point.x = tx;
      data->point.y = ty;
      return;
    }
  }
  data->state = LV_INDEV_STATE_REL;
}

// ── Public API ──────────────────────────────────────────────────────────
bool hwInputInit() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  // CST816T hardware reset sequence
  pinMode(PIN_TP_RST, OUTPUT);
  digitalWrite(PIN_TP_RST, LOW);
  delay(10);
  digitalWrite(PIN_TP_RST, HIGH);
  delay(50);

  // Probe: read gesture ID register
  Wire.beginTransmission(CST816T_ADDR);
  Wire.write(REG_GESTURE_ID);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)CST816T_ADDR, (uint8_t)1);
  if (Wire.available()) {
    uint8_t gesture = Wire.read();
    s_touchOk = true;
    Serial.printf("hwInput: CST816T OK (gesture=0x%02X)\n", gesture);
  } else {
    s_touchOk = false;
    Serial.println("hwInput: CST816T NOT FOUND — touch disabled");
  }

  // Register LVGL input device (always, even if touch failed — reads will return REL)
  static lv_indev_drv_t indevDrv;
  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = touchRead;
  lv_indev_drv_register(&indevDrv);

  return s_touchOk;
}

void hwInputUpdate() {
  // LVGL polls touchRead() automatically via lv_timer_handler()
}
