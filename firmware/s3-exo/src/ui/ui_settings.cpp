#include "ui/ui_settings.h"

#include "config.h"
#include "hw/display.h"
#include "stats.h"
#include "ui/ui_core.h"
#include "ui/ui_wifi.h"
#include "wifi_manager.h"

static lv_obj_t* s_screen = nullptr;
static lv_obj_t* s_wifiStatus = nullptr;
static lv_obj_t* s_brightnessLabel = nullptr;
static lv_obj_t* s_brightnessButtons = nullptr;
static lv_obj_t* s_sleepSwitch = nullptr;
static bool s_displaySleeping = false;

static void restoreBrightnessSelection() {
  if (!s_brightnessButtons) return;
  lv_btnmatrix_clear_btn_ctrl_all(s_brightnessButtons,
                                  LV_BTNMATRIX_CTRL_CHECKED);
  const uint8_t brightness = settings().brightness <= 4 ? settings().brightness : 2;
  lv_btnmatrix_set_btn_ctrl(s_brightnessButtons, brightness,
                            LV_BTNMATRIX_CTRL_CHECKED);
}

static void openWifi(lv_event_t* event) {
  if (!ui_event_is_tap(event)) return;
  ui_wifi_show();
}

static void selectBrightness(lv_event_t* event) {
  lv_obj_t* matrix = lv_event_get_target(event);
  if (!ui_event_is_tap(event)) {
    restoreBrightnessSelection();
    return;
  }

  const uint16_t selected = lv_btnmatrix_get_selected_btn(matrix);
  if (selected > 4) {
    restoreBrightnessSelection();
    return;
  }

  const uint8_t level = static_cast<uint8_t>(selected);
  hwDisplayBrightness(level);
  settings().brightness = level;
  settingsSave();
  ui_label_setf(s_brightnessLabel, "BRIGHTNESS  %u", level);
}

static void toggleSleep(lv_event_t* event) {
  if (!ui_event_is_tap(event)) {
    if (s_displaySleeping) lv_obj_add_state(s_sleepSwitch, LV_STATE_CHECKED);
    else lv_obj_clear_state(s_sleepSwitch, LV_STATE_CHECKED);
    return;
  }
  s_displaySleeping = lv_obj_has_state(s_sleepSwitch, LV_STATE_CHECKED);
  hwDisplaySleep(s_displaySleeping);
}

void ui_settings_refresh() {
  if (!s_wifiStatus) return;
  if (wifiMgrConnected()) {
    ui_label_setf(s_wifiStatus, "Connected  %s", wifiMgrLocalIP().c_str());
    lv_obj_set_style_text_color(s_wifiStatus, lv_color_hex(UI_GREEN), 0);
  } else {
    lv_label_set_text(s_wifiStatus, "Offline - tap to configure");
    lv_obj_set_style_text_color(s_wifiStatus, lv_color_hex(UI_AMBER), 0);
  }
}

lv_obj_t* ui_settings_create() {
  s_screen = ui_screen_create();
  lv_obj_t* title = ui_label(s_screen, 12, 4, 216, UI_GREY, UI_F12);
  lv_label_set_text(title, "SETTINGS");

  lv_obj_t* body = ui_page_content(s_screen, true);

  lv_obj_t* wifiCard = ui_card(body, 8, 4, 224, 62, UI_CARD);
  ui_prepare_clickable(wifiCard);
  lv_obj_add_event_cb(wifiCard, openWifi, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* wifiTitle = ui_label(wifiCard, 10, 6, 174, UI_WHITE, UI_F16);
  lv_label_set_text(wifiTitle, "WiFi Setup");
  s_wifiStatus = ui_label(wifiCard, 10, 33, 178, UI_GREY, UI_F10);
  lv_label_set_text(s_wifiStatus, "Tap to scan networks");
  lv_obj_t* wifiArrow = ui_label(wifiCard, 199, 18, 16, UI_AMBER, UI_F20);
  lv_label_set_text(wifiArrow, ">");

  lv_obj_t* brightnessCard = ui_card(body, 8, 76, 224, 64, 0x1082);
  s_brightnessLabel = ui_label(brightnessCard, 10, 5, 196, UI_WHITE, UI_F10);
  const uint8_t brightness = settings().brightness <= 4 ? settings().brightness : 2;
  ui_label_setf(s_brightnessLabel, "BRIGHTNESS  %u", brightness);
  static const char* brightnessMap[] = {"0", "1", "2", "3", "4", ""};
  s_brightnessButtons = lv_btnmatrix_create(brightnessCard);
  lv_btnmatrix_set_map(s_brightnessButtons, brightnessMap);
  lv_btnmatrix_set_btn_ctrl_all(s_brightnessButtons, LV_BTNMATRIX_CTRL_CHECKABLE);
  lv_btnmatrix_set_one_checked(s_brightnessButtons, true);
  lv_obj_set_pos(s_brightnessButtons, 10, 25);
  lv_obj_set_size(s_brightnessButtons, 196, 32);
  lv_obj_set_style_pad_all(s_brightnessButtons, 1, 0);
  lv_obj_set_style_pad_gap(s_brightnessButtons, 2, 0);
  lv_obj_set_style_text_font(s_brightnessButtons, UI_F10, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(s_brightnessButtons, lv_color_hex(UI_BLUE),
                            LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_add_flag(s_brightnessButtons, LV_OBJ_FLAG_GESTURE_BUBBLE);
  restoreBrightnessSelection();
  lv_obj_add_event_cb(s_brightnessButtons, selectBrightness,
                      LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t* sleepCard = ui_card(body, 8, 150, 224, 52, UI_CARD);
  lv_obj_t* sleepTitle = ui_label(sleepCard, 10, 6, 142, UI_WHITE, UI_F12);
  lv_label_set_text(sleepTitle, "Display sleep");
  lv_obj_t* sleepSub = ui_label(sleepCard, 10, 28, 142, UI_DIM, UI_F10);
  lv_label_set_text(sleepSub, "Tap again to wake");
  s_sleepSwitch = lv_switch_create(sleepCard);
  lv_obj_set_pos(s_sleepSwitch, 162, 8);
  lv_obj_add_flag(s_sleepSwitch, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(s_sleepSwitch, toggleSleep, LV_EVENT_VALUE_CHANGED,
                      nullptr);

  lv_obj_t* networkCard = ui_card(body, 8, 212, 224, 70, 0x1082);
  lv_obj_t* networkTitle = ui_label(networkCard, 10, 6, 196, UI_GREY, UI_F10);
  lv_label_set_text(networkTitle, "ROBOT NETWORK");
  lv_obj_t* espIp = ui_label(networkCard, 10, 27, 196, UI_WHITE, UI_F12);
  lv_label_set_text_fmt(espIp, "ESP32  %s", STATIC_IP);
  lv_obj_t* robotIp = ui_label(networkCard, 10, 48, 196, UI_GREY, UI_F10);
  lv_label_set_text_fmt(robotIp, "Robot  %s", ROBOT_IP);

  lv_obj_t* guardCard = ui_card(body, 8, 292, 224, 62, UI_CARD);
  lv_obj_t* guardTitle = ui_label(guardCard, 10, 6, 196, UI_WHITE, UI_F12);
  lv_label_set_text(guardTitle, "Touch protection");
  lv_obj_t* guardSub = ui_label(guardCard, 10, 30, 196, UI_GREY, UI_F10);
  lv_label_set_text(guardSub, "12 px drag guard enabled\nSwipes will not press controls");

  ui_add_page_dots(s_screen, 3);
  return s_screen;
}
