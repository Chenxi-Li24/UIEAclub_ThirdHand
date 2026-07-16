#include "ui/ui_home.h"

#include "hw/imu.h"
#include "robot/cnde_client.h"
#include "ui/ui_core.h"
#include "wifi_manager.h"

#include <WiFi.h>

static lv_obj_t* s_screen = nullptr;
static lv_obj_t* s_jointValues[6] = {nullptr};
static lv_obj_t* s_robotState = nullptr;
static lv_obj_t* s_cndeDot = nullptr;
static lv_obj_t* s_ssid = nullptr;
static lv_obj_t* s_rssi = nullptr;
static lv_obj_t* s_ip = nullptr;
static lv_obj_t* s_uptime = nullptr;
static lv_obj_t* s_imu = nullptr;

static void makeJointCard(lv_obj_t* parent, int x, int y, int joint) {
  lv_obj_t* card = ui_card(parent, x, y, 70, 41, UI_CARD);
  lv_obj_t* name = ui_label(card, 4, 2, 22, UI_GREY, UI_F10);
  lv_label_set_text_fmt(name, "J%d", joint + 1);
  s_jointValues[joint] = ui_label(card, 4, 16, 62, UI_WHITE, UI_F12);
  lv_label_set_text(s_jointValues[joint], "--.-");
  lv_obj_set_style_text_align(s_jointValues[joint], LV_TEXT_ALIGN_CENTER, 0);
}

void ui_home_refresh() {
  if (!s_screen) return;
  const RobotStateData state = g_cnde.getState();

  if (state.valid) {
    for (int i = 0; i < 6; i++) {
      ui_label_setf(s_jointValues[i], "%.1f", state.jointPos[i]);
    }
    lv_label_set_text(s_robotState, state.robotState == 1 ? "RUNNING" : "IDLE");
    lv_obj_set_style_text_color(
        s_robotState,
        lv_color_hex(state.robotState == 1 ? UI_GREEN : UI_GREY), 0);
  } else {
    for (lv_obj_t* value : s_jointValues) lv_label_set_text(value, "--.-");
    lv_label_set_text(s_robotState, "NO DATA");
    lv_obj_set_style_text_color(s_robotState, lv_color_hex(UI_RED), 0);
  }

  lv_obj_set_style_bg_color(
      s_cndeDot, lv_color_hex(g_cnde.isConnected() ? UI_GREEN : UI_DIM), 0);

  if (wifiMgrState() == WM_OK) {
    ui_label_setf(s_ssid, "%s", WiFi.SSID().c_str());
    ui_label_setf(s_ip, "%s", wifiMgrLocalIP().c_str());
    ui_label_setf(s_rssi, "%d dBm", wifiMgrRssi());
  } else {
    lv_label_set_text(s_ssid,
                      wifiMgrState() == WM_CONNECTING ? "Connecting..." : "Offline");
    lv_label_set_text(s_ip, "");
    lv_label_set_text(s_rssi, "");
  }

  const uint32_t uptime = millis() / 1000;
  ui_label_setf(s_uptime, "%uh %um", uptime / 3600, (uptime % 3600) / 60);
  ui_label_setf(s_imu, "X:%+.2f  Y:%+.2f  Z:%+.2f", g_imu_ax, g_imu_ay,
                g_imu_az);
}

lv_obj_t* ui_home_create() {
  s_screen = ui_screen_create();

  lv_obj_t* title = ui_label(s_screen, 0, 4, 240, UI_AMBER, UI_F12);
  lv_label_set_text(title, "FAIRINO ROBOT");
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

  s_cndeDot = lv_obj_create(s_screen);
  lv_obj_set_size(s_cndeDot, 8, 8);
  lv_obj_set_pos(s_cndeDot, 222, 7);
  lv_obj_set_style_bg_color(s_cndeDot, lv_color_hex(UI_DIM), 0);
  lv_obj_set_style_radius(s_cndeDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(s_cndeDot, 0, 0);
  lv_obj_clear_flag(s_cndeDot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* body = ui_page_content(s_screen, false);
  for (int i = 0; i < 6; i++) {
    makeJointCard(body, 8 + (i % 3) * 77, 1 + (i / 3) * 45, i);
  }

  lv_obj_t* stateCard = ui_card(body, 8, 93, 224, 29, 0x1082);
  lv_obj_t* stateName = ui_label(stateCard, 10, 7, 52, UI_GREY, UI_F10);
  lv_label_set_text(stateName, "ROBOT");
  s_robotState = ui_label(stateCard, 66, 6, 104, UI_RED, UI_F12);
  lv_label_set_text(s_robotState, "NO DATA");

  lv_obj_t* network = ui_card(body, 8, 128, 224, 46, UI_CARD);
  s_ssid = ui_label(network, 10, 5, 142, UI_WHITE, UI_F12);
  lv_label_set_long_mode(s_ssid, LV_LABEL_LONG_DOT);
  s_rssi = ui_label(network, 156, 7, 56, UI_GREY, UI_F10);
  lv_obj_set_style_text_align(s_rssi, LV_TEXT_ALIGN_RIGHT, 0);
  s_ip = ui_label(network, 10, 25, 202, UI_GREY, UI_F10);

  lv_obj_t* uptimeName = ui_label(body, 8, 182, 58, UI_DIM, UI_F10);
  lv_label_set_text(uptimeName, "UPTIME");
  s_uptime = ui_label(body, 68, 178, 164, UI_WHITE, UI_F16);
  lv_label_set_text(s_uptime, "0h 0m");

  lv_obj_t* imuName = ui_label(body, 8, 211, 38, UI_DIM, UI_F10);
  lv_label_set_text(imuName, "ACC");
  s_imu = ui_label(body, 47, 211, 185, UI_WHITE, UI_F10);
  lv_label_set_text(s_imu, "X:--  Y:--  Z:--");
  lv_obj_set_style_text_align(s_imu, LV_TEXT_ALIGN_RIGHT, 0);

  ui_add_page_dots(s_screen, 0);
  return s_screen;
}
