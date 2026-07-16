#include "ui/ui_wifi.h"

#include "ui/ui_core.h"
#include "wifi_manager.h"

#include <cstring>

static lv_obj_t* s_scanScreen = nullptr;
static lv_obj_t* s_passwordScreen = nullptr;
static lv_obj_t* s_manualScreen = nullptr;
static lv_obj_t* s_list = nullptr;
static lv_obj_t* s_scanStatus = nullptr;
static lv_obj_t* s_passwordTitle = nullptr;
static lv_obj_t* s_passwordInput = nullptr;
static lv_obj_t* s_manualSsid = nullptr;
static lv_obj_t* s_manualPassword = nullptr;
static lv_obj_t* s_keyboard = nullptr;
static char s_selectedSsid[33] = {};
static uint8_t s_returnScreen = 3;

static lv_obj_t* makeButton(lv_obj_t* parent, int x, int y, int w, int h,
                            const char* text, lv_event_cb_t callback) {
  lv_obj_t* button = lv_btn_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, w, h);
  lv_obj_set_style_bg_color(button, lv_color_hex(UI_CARD), 0);
  lv_obj_set_style_radius(button, 7, 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, UI_F10, 0);
  lv_obj_center(label);
  return button;
}

static void deleteKeyboard() {
  if (!s_keyboard) return;
  lv_obj_del(s_keyboard);
  s_keyboard = nullptr;
}

static void returnToSettings(lv_event_t* event) {
  if (event && !ui_event_is_tap(event)) return;
  deleteKeyboard();
  lv_obj_t* oldScan = s_scanScreen;
  lv_obj_t* oldPassword = s_passwordScreen;
  lv_obj_t* oldManual = s_manualScreen;
  s_scanScreen = nullptr;
  s_passwordScreen = nullptr;
  s_manualScreen = nullptr;
  s_list = nullptr;
  s_scanStatus = nullptr;
  s_passwordTitle = nullptr;
  s_passwordInput = nullptr;
  s_manualSsid = nullptr;
  s_manualPassword = nullptr;
  ui_gesture_enable(true);
  ui_screen_goto(s_returnScreen, false);
  if (oldPassword) lv_obj_del_async(oldPassword);
  if (oldManual) lv_obj_del_async(oldManual);
  if (oldScan) lv_obj_del_async(oldScan);
}

static void showScanScreen(lv_event_t* event) {
  if (event && !ui_event_is_tap(event)) return;
  deleteKeyboard();
  lv_scr_load(s_scanScreen);
}

static void passwordConnect(lv_event_t* event) {
  if (!ui_event_is_tap(event)) return;
  const char* password = lv_textarea_get_text(s_passwordInput);
  wifiMgrConnect(s_selectedSsid, password ? password : "");
  returnToSettings(nullptr);
}

static void manualConnect(lv_event_t* event) {
  if (!ui_event_is_tap(event)) return;
  const char* ssid = lv_textarea_get_text(s_manualSsid);
  const char* password = lv_textarea_get_text(s_manualPassword);
  if (!ssid || !ssid[0]) return;
  wifiMgrConnect(ssid, password ? password : "");
  returnToSettings(nullptr);
}

static void manualFieldFocused(lv_event_t* event) {
  if (s_keyboard) lv_keyboard_set_textarea(s_keyboard, lv_event_get_target(event));
}

static void createPasswordScreen() {
  if (s_passwordScreen) return;
  s_passwordScreen = ui_screen_create();
  makeButton(s_passwordScreen, 6, 3, 34, 23, "<", showScanScreen);
  s_passwordTitle = ui_label(s_passwordScreen, 46, 5, 188, UI_WHITE, UI_F12);
  lv_label_set_long_mode(s_passwordTitle, LV_LABEL_LONG_DOT);

  s_passwordInput = lv_textarea_create(s_passwordScreen);
  lv_obj_set_pos(s_passwordInput, 6, 33);
  lv_obj_set_size(s_passwordInput, 228, 38);
  lv_textarea_set_password_mode(s_passwordInput, true);
  lv_textarea_set_max_length(s_passwordInput, 63);
  lv_textarea_set_placeholder_text(s_passwordInput, "Password (blank if open)");

  makeButton(s_passwordScreen, 126, 242, 108, 32, "Connect", passwordConnect);
  makeButton(s_passwordScreen, 6, 242, 108, 32, "Cancel", showScanScreen);
}

static void showPassword(const char* ssid) {
  createPasswordScreen();
  deleteKeyboard();
  strncpy(s_selectedSsid, ssid, sizeof(s_selectedSsid) - 1);
  s_selectedSsid[sizeof(s_selectedSsid) - 1] = 0;
  lv_label_set_text_fmt(s_passwordTitle, "SSID  %s", s_selectedSsid);
  lv_textarea_set_text(s_passwordInput, "");
  lv_scr_load(s_passwordScreen);

  s_keyboard = lv_keyboard_create(s_passwordScreen);
  lv_obj_set_pos(s_keyboard, 0, 77);
  lv_obj_set_size(s_keyboard, 240, 158);
  lv_keyboard_set_textarea(s_keyboard, s_passwordInput);
}

static void networkSelected(lv_event_t* event) {
  if (!ui_event_is_tap(event)) return;
  const uint8_t index = static_cast<uint8_t>(
      reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  WifiScanEntry entry = {};
  if (wifiMgrScanGet(index, entry)) showPassword(entry.ssid);
}

static void populateScanList() {
  lv_obj_clean(s_list);
  const uint8_t count = wifiMgrScanCount();
  if (!count) {
    lv_list_add_text(s_list, "No networks found");
    lv_label_set_text(s_scanStatus, "No networks - tap Rescan");
    return;
  }

  for (uint8_t i = 0; i < count; i++) {
    WifiScanEntry entry = {};
    if (!wifiMgrScanGet(i, entry)) continue;
    char label[64];
    snprintf(label, sizeof(label), "%s   %ld dBm%s", entry.ssid,
             static_cast<long>(entry.rssi), entry.secure ? "  *" : "");
    lv_obj_t* button = lv_list_add_btn(s_list, nullptr, label);
    lv_obj_add_flag(button, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(button, networkSelected, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
    lv_obj_t* labelObj = lv_obj_get_child(button, 0);
    if (labelObj) lv_label_set_long_mode(labelObj, LV_LABEL_LONG_DOT);
  }
  lv_label_set_text_fmt(s_scanStatus, "%u network%s", count,
                        count == 1 ? "" : "s");
}

static void scanFinished(String json) {
  (void)json;
  if (lv_scr_act() == s_scanScreen) populateScanList();
}

static void startScan() {
  lv_obj_clean(s_list);
  lv_list_add_text(s_list, "Scanning...");
  lv_label_set_text(s_scanStatus, "Scanning 2.4 GHz...");
  if (!wifiMgrScan(scanFinished)) {
    lv_obj_clean(s_list);
    lv_list_add_text(s_list, "Scan already running");
    lv_label_set_text(s_scanStatus, "Busy - tap Rescan later");
  }
}

static void rescan(lv_event_t* event) {
  if (ui_event_is_tap(event)) startScan();
}

static void createManualScreen() {
  if (s_manualScreen) return;
  s_manualScreen = ui_screen_create();
  makeButton(s_manualScreen, 6, 3, 34, 23, "<", showScanScreen);
  lv_obj_t* title = ui_label(s_manualScreen, 46, 5, 188, UI_WHITE, UI_F12);
  lv_label_set_text(title, "MANUAL WIFI");

  s_manualSsid = lv_textarea_create(s_manualScreen);
  lv_obj_set_pos(s_manualSsid, 6, 32);
  lv_obj_set_size(s_manualSsid, 228, 34);
  lv_textarea_set_one_line(s_manualSsid, true);
  lv_textarea_set_max_length(s_manualSsid, 32);
  lv_textarea_set_placeholder_text(s_manualSsid, "SSID");
  lv_obj_add_event_cb(s_manualSsid, manualFieldFocused, LV_EVENT_FOCUSED, nullptr);

  s_manualPassword = lv_textarea_create(s_manualScreen);
  lv_obj_set_pos(s_manualPassword, 6, 70);
  lv_obj_set_size(s_manualPassword, 228, 34);
  lv_textarea_set_one_line(s_manualPassword, true);
  lv_textarea_set_password_mode(s_manualPassword, true);
  lv_textarea_set_max_length(s_manualPassword, 63);
  lv_textarea_set_placeholder_text(s_manualPassword, "Password");
  lv_obj_add_event_cb(s_manualPassword, manualFieldFocused, LV_EVENT_FOCUSED,
                      nullptr);

  makeButton(s_manualScreen, 126, 242, 108, 32, "Connect", manualConnect);
  makeButton(s_manualScreen, 6, 242, 108, 32, "Cancel", showScanScreen);
}

static void showManual(lv_event_t* event) {
  if (!ui_event_is_tap(event)) return;
  createManualScreen();
  deleteKeyboard();
  lv_textarea_set_text(s_manualSsid, "");
  lv_textarea_set_text(s_manualPassword, "");
  lv_scr_load(s_manualScreen);
  s_keyboard = lv_keyboard_create(s_manualScreen);
  lv_obj_set_pos(s_keyboard, 0, 109);
  lv_obj_set_size(s_keyboard, 240, 126);
  lv_keyboard_set_textarea(s_keyboard, s_manualSsid);
}

void ui_wifi_create() {
  if (s_scanScreen) return;
  s_scanScreen = ui_screen_create();
  makeButton(s_scanScreen, 6, 3, 34, 23, "<", returnToSettings);
  lv_obj_t* title = ui_label(s_scanScreen, 46, 5, 104, UI_WHITE, UI_F12);
  lv_label_set_text(title, "WIFI SETUP");
  s_scanStatus = ui_label(s_scanScreen, 148, 6, 86, UI_GREY, UI_F10);
  lv_obj_set_style_text_align(s_scanStatus, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(s_scanStatus, LV_LABEL_LONG_DOT);

  s_list = lv_list_create(s_scanScreen);
  lv_obj_set_pos(s_list, 6, 32);
  lv_obj_set_size(s_list, 228, 199);
  lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_color(s_list, lv_color_hex(0x0841), 0);
  lv_obj_set_style_border_width(s_list, 0, 0);
  lv_obj_set_style_radius(s_list, 8, 0);

  makeButton(s_scanScreen, 6, 241, 108, 33, "Rescan", rescan);
  makeButton(s_scanScreen, 126, 241, 108, 33, "Manual", showManual);
}

void ui_wifi_show() {
  s_returnScreen = ui_current_screen();
  ui_gesture_enable(false);
  deleteKeyboard();
  if (!s_scanScreen) ui_wifi_create();
  lv_scr_load(s_scanScreen);
  startScan();
}

void ui_wifi_close() { returnToSettings(nullptr); }

lv_obj_t* ui_wifi_screen() { return s_scanScreen; }
