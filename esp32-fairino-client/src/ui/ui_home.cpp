// ui_home.cpp — Page 0: Home — robot joint angles + WiFi + IMU
#include "ui/ui_home.h"
#include "hw/imu.h"
#include "ui/ui_core.h"
#include "wifi_manager.h"
#include "cnde_client.h"
#include <WiFi.h>

static lv_obj_t *scr;
static lv_obj_t *w_title, *w_j1, *w_j2, *w_j3, *w_j4, *w_j5, *w_j6;
static lv_obj_t *w_robotState, *w_cndeDot;
static lv_obj_t *w_ssid, *w_rssi, *w_ip, *w_uptime;
static lv_obj_t *imuVal;

void ui_home_refresh() {
  if (!scr) return;
  const RobotStateData& rs = g_cnde.getState();

  if (rs.valid) {
    ui_label_setf(w_j1, "%.1f", rs.jointPos[0]);
    ui_label_setf(w_j2, "%.1f", rs.jointPos[1]);
    ui_label_setf(w_j3, "%.1f", rs.jointPos[2]);
    ui_label_setf(w_j4, "%.1f", rs.jointPos[3]);
    ui_label_setf(w_j5, "%.1f", rs.jointPos[4]);
    ui_label_setf(w_j6, "%.1f", rs.jointPos[5]);
    const char* stText = rs.robotState == 1 ? "RUNNING" : "IDLE";
    lv_label_set_text(w_robotState, stText);
    uint32_t stColor = rs.robotState == 1 ? UI_GREEN : UI_GREY;
    lv_obj_set_style_text_color(w_robotState, lv_color_hex(stColor), 0);
  } else {
    lv_label_set_text(w_j1, "--.-"); lv_label_set_text(w_j2, "--.-");
    lv_label_set_text(w_j3, "--.-"); lv_label_set_text(w_j4, "--.-");
    lv_label_set_text(w_j5, "--.-"); lv_label_set_text(w_j6, "--.-");
    lv_label_set_text(w_robotState, "NO DATA");
    lv_obj_set_style_text_color(w_robotState, lv_color_hex(UI_RED), 0);
  }

  // CNDE connection dot
  uint32_t cndeColor = g_cnde.isConnected() ? UI_GREEN : UI_DIM;
  lv_obj_set_style_bg_color(w_cndeDot, lv_color_hex(cndeColor), 0);

  // WiFi
  WifiMgrState st = wifiMgrState();
  if (st == WM_OK) {
    ui_label_setf(w_ssid, "%s", WiFi.SSID().c_str());
    ui_label_setf(w_ip, "%s", wifiMgrLocalIP().c_str());
    ui_label_setf(w_rssi, "%ddBm", wifiMgrRssi());
  } else {
    lv_label_set_text(w_ssid, st == WM_CONNECTING ? "Connecting..." : "Offline");
    lv_label_set_text(w_ip, "");
    lv_label_set_text(w_rssi, "");
  }

  uint32_t up = millis() / 1000;
  ui_label_setf(w_uptime, "%uh %um", up / 3600, (up % 3600) / 60);

  if (imuVal) {
    ui_label_setf(imuVal, "X:%+.2f  Y:%+.2f  Z:%+.2f", g_imu_ax, g_imu_ay, g_imu_az);
  }
}

lv_obj_t* ui_home_create() {
  scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG), 0);
  lv_obj_set_style_pad_all(scr, 0, 0);

  w_title = ui_label(scr, 0, 4, 240, UI_AMBER, UI_F12);
  lv_label_set_text(w_title, "Fairino Robot");
  lv_obj_set_style_text_align(w_title, LV_TEXT_ALIGN_CENTER, 0);

  // CNDE status dot
  w_cndeDot = lv_obj_create(scr);
  lv_obj_set_size(w_cndeDot, 8, 8);
  lv_obj_set_pos(w_cndeDot, 222, 6);
  lv_obj_set_style_bg_color(w_cndeDot, lv_color_hex(UI_DIM), 0);
  lv_obj_set_style_radius(w_cndeDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(w_cndeDot, 0, 0);

  // Joint angles — 2 rows of 3
  lv_obj_t* jLabel1 = ui_label(scr, 12, 30, 40, UI_GREY, UI_F10);
  lv_label_set_text(jLabel1, "J1");
  w_j1 = ui_label(scr, 42, 30, 30, UI_WHITE, UI_F12);
  lv_label_set_text(w_j1, "--.-");

  lv_obj_t* jLabel2 = ui_label(scr, 76, 30, 40, UI_GREY, UI_F10);
  lv_label_set_text(jLabel2, "J2");
  w_j2 = ui_label(scr, 106, 30, 30, UI_WHITE, UI_F12);
  lv_label_set_text(w_j2, "--.-");

  lv_obj_t* jLabel3 = ui_label(scr, 140, 30, 40, UI_GREY, UI_F10);
  lv_label_set_text(jLabel3, "J3");
  w_j3 = ui_label(scr, 170, 30, 30, UI_WHITE, UI_F12);
  lv_label_set_text(w_j3, "--.-");

  lv_obj_t* jLabel4 = ui_label(scr, 12, 56, 40, UI_GREY, UI_F10);
  lv_label_set_text(jLabel4, "J4");
  w_j4 = ui_label(scr, 42, 56, 30, UI_WHITE, UI_F12);
  lv_label_set_text(w_j4, "--.-");

  lv_obj_t* jLabel5 = ui_label(scr, 76, 56, 40, UI_GREY, UI_F10);
  lv_label_set_text(jLabel5, "J5");
  w_j5 = ui_label(scr, 106, 56, 30, UI_WHITE, UI_F12);
  lv_label_set_text(w_j5, "--.-");

  lv_obj_t* jLabel6 = ui_label(scr, 140, 56, 40, UI_GREY, UI_F10);
  lv_label_set_text(jLabel6, "J6");
  w_j6 = ui_label(scr, 170, 56, 30, UI_WHITE, UI_F12);
  lv_label_set_text(w_j6, "--.-");

  // Robot state
  lv_obj_t* rsLabel = ui_label(scr, 12, 80, 60, UI_GREY, UI_F10);
  lv_label_set_text(rsLabel, "State:");
  w_robotState = ui_label(scr, 60, 80, 80, UI_GREY, UI_F12);
  lv_label_set_text(w_robotState, "NO DATA");

  ui_divider(scr, 104);

  // WiFi info
  w_ssid = ui_label(scr, 12, 114, 150, UI_WHITE, UI_F12);
  w_rssi = ui_label(scr, 180, 114, 50, UI_GREY, UI_F10);
  lv_obj_set_style_text_align(w_rssi, LV_TEXT_ALIGN_RIGHT, 0);
  w_ip   = ui_label(scr, 12, 134, 216, UI_GREY, UI_F10);

  ui_divider(scr, 156);

  // Uptime
  w_uptime = ui_label(scr, 0, 170, 240, UI_WHITE, UI_F20);
  lv_obj_set_style_text_align(w_uptime, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(w_uptime, "0h 0m");
  lv_obj_t* upLabel = ui_label(scr, 0, 198, 240, UI_GREY, UI_F10);
  lv_label_set_text(upLabel, "UPTIME");
  lv_obj_set_style_text_align(upLabel, LV_TEXT_ALIGN_CENTER, 0);

  // IMU
  lv_obj_t* imuLabel = ui_label(scr, 0, 224, 240, UI_DIM, UI_F10);
  lv_label_set_text(imuLabel, "ACCEL");
  lv_obj_set_style_text_align(imuLabel, LV_TEXT_ALIGN_CENTER, 0);
  imuVal = ui_label(scr, 0, 236, 240, UI_WHITE, UI_F12);
  lv_label_set_text(imuVal, "X: ---  Y: ---  Z: ---");
  lv_obj_set_style_text_align(imuVal, LV_TEXT_ALIGN_CENTER, 0);

  ui_add_page_dots(scr, 0);
  return scr;
}
