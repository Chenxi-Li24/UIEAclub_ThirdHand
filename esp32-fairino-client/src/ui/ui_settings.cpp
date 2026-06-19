// ui_settings.cpp — Page 3: Settings (WiFi, brightness, sleep, robot IP, about)
#include "ui/ui_settings.h"
#include "hw/display.h"
#include "ui/ui_core.h"
#include "ui/ui_wifi.h"

static lv_obj_t *scr, *s_brightLabel;

static void onWifiSetup(lv_event_t* e) {
  ui_gesture_enable(false);
  ui_wifi_show();
}

static void onBrightChange(lv_event_t* e) {
  lv_obj_t* slider = lv_event_get_target(e);
  uint8_t lvl = (uint8_t)lv_slider_get_value(slider);
  hwDisplayBrightness(lvl);
  ui_label_setf(s_brightLabel, "Brightness  %d", lvl);
}

static void onSleepToggle(lv_event_t* e) {
  lv_obj_t* sw = lv_event_get_target(e);
  hwDisplaySleep(lv_obj_has_state(sw, LV_STATE_CHECKED));
}

void ui_settings_refresh() {}

lv_obj_t* ui_settings_create() {
  scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG), 0);
  lv_obj_set_style_pad_all(scr, 0, 0);

  lv_obj_t* title = ui_label(scr, 12, 4, 216, UI_GREY, UI_F12);
  lv_label_set_text(title, "SETTINGS");

  int y = 32;

  // WiFi Setup card
  lv_obj_t* wifiCard = ui_card(scr, 12, y, 216, 44, UI_CARD);
  lv_obj_add_flag(wifiCard, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(wifiCard, onWifiSetup, LV_EVENT_CLICKED, NULL);
  lv_obj_t* wifiLabel = ui_label(wifiCard, 12, 12, 160, UI_WHITE, UI_F12);
  lv_label_set_text(wifiLabel, "WiFi Setup");
  lv_obj_t* arrow = ui_label(wifiCard, 190, 12, 20, UI_GREY, UI_F16);
  lv_label_set_text(arrow, ">");

  // Brightness
  y += 58;
  lv_obj_t* briCard = ui_card(scr, 12, y, 216, 44, UI_CARD);
  s_brightLabel = ui_label(briCard, 12, 4, 120, UI_WHITE, UI_F10);
  lv_label_set_text(s_brightLabel, "Brightness  2");
  lv_obj_t* slider = lv_slider_create(briCard);
  lv_obj_set_size(slider, 80, 8);
  lv_obj_set_pos(slider, 130, 18);
  lv_slider_set_range(slider, 0, 4);
  lv_slider_set_value(slider, 2, LV_ANIM_OFF);
  lv_obj_add_event_cb(slider, onBrightChange, LV_EVENT_VALUE_CHANGED, NULL);

  // Display Sleep
  y += 58;
  lv_obj_t* sleepCard = ui_card(scr, 12, y, 216, 44, UI_CARD);
  lv_obj_t* sleepLabel = ui_label(sleepCard, 12, 12, 140, UI_WHITE, UI_F12);
  lv_label_set_text(sleepLabel, "Display Sleep");
  lv_obj_t* sw = lv_switch_create(sleepCard);
  lv_obj_set_pos(sw, 170, 8);
  lv_obj_add_event_cb(sw, onSleepToggle, LV_EVENT_VALUE_CHANGED, NULL);

  // Robot IP info card
  y += 58;
  lv_obj_t* ipCard = ui_card(scr, 12, y, 216, 44, UI_CARD);
  lv_obj_t* ipTitle = ui_label(ipCard, 12, 6, 192, UI_WHITE, UI_F10);
  lv_label_set_text(ipTitle, "Robot Network");
  lv_obj_t* ipVal = ui_label(ipCard, 12, 22, 192, UI_DIM, UI_F10);
  lv_label_set_text(ipVal, "ESP32: 192.168.58.100  Robot: 192.168.58.2");

  // About
  y += 58;
  lv_obj_t* aboutCard = ui_card(scr, 12, y, 216, 48, UI_CARD);
  lv_obj_t* aboutTitle = ui_label(aboutCard, 12, 6, 192, UI_WHITE, UI_F12);
  lv_label_set_text(aboutTitle, "Fairino Robot Controller");
  lv_obj_t* aboutVer = ui_label(aboutCard, 12, 26, 192, UI_DIM, UI_F10);
  lv_label_set_text(aboutVer, "DeskPet HW + LVGL  |  platformio");

  ui_add_page_dots(scr, 3);
  return scr;
}
