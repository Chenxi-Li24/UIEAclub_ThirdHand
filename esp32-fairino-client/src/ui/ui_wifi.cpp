// src/ui/ui_wifi.cpp — LVGL WiFi 设置屏幕（扫描 + 密码 + 手动输入）

#include "ui/ui_wifi.h"

#include "hw/pins.h"
#include "ui/ui_core.h"
#include "wifi_manager.h"

#include <WiFi.h>

static lv_obj_t *s_scrWifi = nullptr;
static lv_obj_t *s_scrPass = nullptr;
static lv_obj_t *s_scrManual = nullptr;
static lv_obj_t *s_list = nullptr;
static lv_obj_t *s_kb = nullptr;

static char s_selSsid[33] = {0};
static bool s_scanDone = false;
static uint8_t s_returnScreen = 0;

// ── Navigation helpers ─────────────────────────────────────────────────
static void goHome() {
  if (s_kb) {
    lv_obj_del(s_kb);
    s_kb = nullptr;
  }
  ui_gesture_enable(true);
  ui_screen_goto(s_returnScreen, false);
}

static void showPassScreen(const char *ssid) {
  strncpy(s_selSsid, ssid, 32);
  s_selSsid[32] = 0;
  lv_obj_t *lbl = lv_obj_get_child(s_scrPass, 0);
  lv_label_set_text_fmt(lbl, "SSID: %s", s_selSsid);
  lv_textarea_set_text(lv_obj_get_child(s_scrPass, 1), "");
  lv_scr_load(s_scrPass);
  if (!s_kb) {
    s_kb = lv_keyboard_create(s_scrPass);
    lv_keyboard_set_textarea(s_kb, lv_obj_get_child(s_scrPass, 1));
  } else {
    lv_keyboard_set_textarea(s_kb, lv_obj_get_child(s_scrPass, 1));
  }
}

// ── WiFi scan start ────────────────────────────────────────────────────
void ui_wifi_show() {
  s_returnScreen = ui_current_screen();
  ui_gesture_enable(false);
  if (s_kb) {
    lv_obj_del(s_kb);
    s_kb = nullptr;
  }
  lv_obj_clean(s_list);
  lv_list_add_text(s_list, "Scanning...");
  s_scanDone = false;
  WiFi.scanNetworks(true);  // async scan
  lv_scr_load(s_scrWifi);
}

// ── Populate scan list ─────────────────────────────────────────────────
static void populateScanList(int n) {
  lv_obj_clean(s_list);
  for (int i = 0; i < n; i++) {
    String label = WiFi.SSID(i);
    label += "  (";
    label += WiFi.RSSI(i);
    label += "dBm)";
    if (WiFi.encryptionType(i) != WIFI_AUTH_OPEN)
      label += " *";
    lv_obj_t *btn = lv_list_add_btn(s_list, NULL, label.c_str());
    lv_obj_set_user_data(btn, (void *)(intptr_t)i);
    lv_obj_add_event_cb(
        btn,
        [](lv_event_t *ev) {
          int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(ev));
          String ssid = WiFi.SSID(idx);
          showPassScreen(ssid.c_str());
        },
        LV_EVENT_CLICKED, NULL);
  }
  if (n == 0)
    lv_list_add_text(s_list, "No networks found");
}

// ── Manual SSID entry screen ───────────────────────────────────────────
static lv_obj_t *s_taSsid, *s_taPass;

static void onManualWifi(lv_event_t *e) {
  if (s_kb) {
    lv_obj_del(s_kb);
    s_kb = nullptr;
  }
  lv_textarea_set_text(s_taSsid, "");
  lv_textarea_set_text(s_taPass, "");
  lv_scr_load(s_scrManual);
  s_kb = lv_keyboard_create(s_scrManual);
  lv_keyboard_set_textarea(s_kb, s_taSsid);
}

static void onManualConnect(lv_event_t *e) {
  const char *ssid = lv_textarea_get_text(s_taSsid);
  const char *pass = lv_textarea_get_text(s_taPass);
  if (ssid[0] == 0)
    return;
  wifiMgrConnect(ssid, pass ? pass : "");
  if (s_kb) {
    lv_obj_del(s_kb);
    s_kb = nullptr;
  }
  goHome();
}

// ── Password-done handler ──────────────────────────────────────────────
static void onPassConnect(lv_event_t *e) {
  const char *pass = lv_textarea_get_text(lv_obj_get_child(s_scrPass, 1));
  wifiMgrConnect(s_selSsid, pass ? pass : "");
  if (s_kb) {
    lv_obj_del(s_kb);
    s_kb = nullptr;
  }
  goHome();
}

// ── Create all WiFi screens ────────────────────────────────────────────
void ui_wifi_create() {
  // --- WiFi scan screen ---
  s_scrWifi = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(s_scrWifi, lv_color_black(), 0);

  lv_obj_t *title = lv_label_create(s_scrWifi);
  lv_label_set_text(title, "WiFi Setup");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

  s_list = lv_list_create(s_scrWifi);
  lv_obj_set_size(s_list, LCD_PHYS_W - 10, 180);
  lv_obj_align(s_list, LV_ALIGN_TOP_MID, 0, 28);

  lv_obj_t *btnManual = lv_btn_create(s_scrWifi);
  lv_obj_set_size(btnManual, 80, 30);
  lv_obj_align(btnManual, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
  lv_label_set_text(lv_label_create(btnManual), "Manual");
  lv_obj_center(lv_obj_get_child(btnManual, 0));
  lv_obj_add_event_cb(btnManual, onManualWifi, LV_EVENT_CLICKED, NULL);

  lv_obj_t *btnBack = lv_btn_create(s_scrWifi);
  lv_obj_set_size(btnBack, 60, 30);
  lv_obj_align(btnBack, LV_ALIGN_BOTTOM_LEFT, 5, -5);
  lv_label_set_text(lv_label_create(btnBack), "Back");
  lv_obj_center(lv_obj_get_child(btnBack, 0));
  lv_obj_add_event_cb(btnBack, [](lv_event_t *e) { goHome(); }, LV_EVENT_CLICKED, NULL);

  // --- Password screen ---
  s_scrPass = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(s_scrPass, lv_color_black(), 0);

  lv_obj_t *lblSsid = lv_label_create(s_scrPass);
  lv_obj_align(lblSsid, LV_ALIGN_TOP_LEFT, 5, 5);

  lv_obj_t *taPass = lv_textarea_create(s_scrPass);
  lv_obj_set_size(taPass, 230, 40);
  lv_obj_align(taPass, LV_ALIGN_TOP_MID, 0, 30);
  lv_textarea_set_password_mode(taPass, true);
  lv_textarea_set_max_length(taPass, 63);
  lv_textarea_set_placeholder_text(taPass, "Password");

  lv_obj_t *btnConnect = lv_btn_create(s_scrPass);
  lv_obj_set_size(btnConnect, 80, 30);
  lv_obj_align(btnConnect, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
  lv_label_set_text(lv_label_create(btnConnect), "Connect");
  lv_obj_center(lv_obj_get_child(btnConnect, 0));
  lv_obj_add_event_cb(btnConnect, onPassConnect, LV_EVENT_CLICKED, NULL);

  lv_obj_t *btnPBack = lv_btn_create(s_scrPass);
  lv_obj_set_size(btnPBack, 60, 30);
  lv_obj_align(btnPBack, LV_ALIGN_BOTTOM_LEFT, 5, -5);
  lv_label_set_text(lv_label_create(btnPBack), "Back");
  lv_obj_center(lv_obj_get_child(btnPBack, 0));
  lv_obj_add_event_cb(
      btnPBack, [](lv_event_t *e) { lv_scr_load(s_scrWifi); }, LV_EVENT_CLICKED, NULL);

  // --- Manual SSID entry screen ---
  s_scrManual = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(s_scrManual, lv_color_black(), 0);

  lv_obj_t *mlbl = lv_label_create(s_scrManual);
  lv_label_set_text(mlbl, "SSID:");
  lv_obj_align(mlbl, LV_ALIGN_TOP_LEFT, 5, 5);
  s_taSsid = lv_textarea_create(s_scrManual);
  lv_obj_set_size(s_taSsid, 230, 36);
  lv_obj_align(s_taSsid, LV_ALIGN_TOP_MID, 0, 25);
  lv_textarea_set_max_length(s_taSsid, 32);
  lv_textarea_set_placeholder_text(s_taSsid, "Network name");

  mlbl = lv_label_create(s_scrManual);
  lv_label_set_text(mlbl, "Password:");
  lv_obj_align(mlbl, LV_ALIGN_TOP_LEFT, 5, 68);
  s_taPass = lv_textarea_create(s_scrManual);
  lv_obj_set_size(s_taPass, 230, 36);
  lv_obj_align(s_taPass, LV_ALIGN_TOP_MID, 0, 90);
  lv_textarea_set_password_mode(s_taPass, true);
  lv_textarea_set_max_length(s_taPass, 63);
  lv_textarea_set_placeholder_text(s_taPass, "Password");

  lv_obj_t *btnMConn = lv_btn_create(s_scrManual);
  lv_obj_set_size(btnMConn, 80, 30);
  lv_obj_align(btnMConn, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
  lv_label_set_text(lv_label_create(btnMConn), "Connect");
  lv_obj_center(lv_obj_get_child(btnMConn, 0));
  lv_obj_add_event_cb(btnMConn, onManualConnect, LV_EVENT_CLICKED, NULL);

  lv_obj_t *btnMBack = lv_btn_create(s_scrManual);
  lv_obj_set_size(btnMBack, 60, 30);
  lv_obj_align(btnMBack, LV_ALIGN_BOTTOM_LEFT, 5, -5);
  lv_label_set_text(lv_label_create(btnMBack), "Back");
  lv_obj_center(lv_obj_get_child(btnMBack, 0));
  lv_obj_add_event_cb(
      btnMBack, [](lv_event_t *e) { lv_scr_load(s_scrWifi); }, LV_EVENT_CLICKED, NULL);
}

// ── Poll scan completion ───────────────────────────────────────────────
// Called from main loop; returns true if scan finished
bool ui_wifi_poll_scan() {
  if (s_scanDone)
    return false;
  int n = WiFi.scanComplete();
  if (n >= 0) {
    s_scanDone = true;
    populateScanList(n);
    return true;
  }
  return false;
}

lv_obj_t *ui_wifi_screen() {
  return s_scrWifi;
}
