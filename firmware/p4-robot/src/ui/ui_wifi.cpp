// ui_wifi.cpp - Tab 3: WiFi Setup
#include "ui/ui_wifi.h"
#include "ui/ui_core.h"
#include "wifi_manager.h"
#include <WiFi.h>

static lv_obj_t *scr;
static lv_obj_t *w_scanBtn, *w_scanList, *w_currSsid, *w_currIp;
static lv_obj_t *s_scrPass = nullptr, *s_kb = nullptr;
static char s_selSsid[33] = {0};
static bool s_scanDone = false;

// Password screen helpers
static void goBack() {
    if (s_kb) { lv_obj_del(s_kb); s_kb = nullptr; }
    lv_scr_load(lv_scr_act());
}

static void onPassConnect(lv_event_t* e) {
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_user_data(e);
    const char* pass = lv_textarea_get_text(ta);
    wifiMgrConnect(s_selSsid, pass ? pass : "");
    goBack();
}

static void showPassScreen(const char* ssid) {
    strncpy(s_selSsid, ssid, 32); s_selSsid[32] = 0;
    if (!s_scrPass) {
        s_scrPass = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(s_scrPass, lv_color_hex(UI_BG), 0);
        lv_obj_t* lbl = lv_label_create(s_scrPass);
        lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 10, 10);
        lv_obj_set_style_text_color(lbl, lv_color_hex(UI_WHITE), 0);
        lv_obj_set_style_text_font(lbl, UI_F28, 0);

        lv_obj_t* ta = lv_textarea_create(s_scrPass);
        lv_obj_set_size(ta, 760, 80);
        lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 80);
        lv_textarea_set_password_mode(ta, true);
        lv_textarea_set_max_length(ta, 63);
        lv_textarea_set_placeholder_text(ta, "Password");
        lv_obj_set_style_text_font(ta, UI_F28, 0);

        lv_obj_t* btnConn = lv_btn_create(s_scrPass);
        lv_obj_set_size(btnConn, 200, 80);
        lv_obj_align(btnConn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
        lv_obj_set_style_bg_color(btnConn, lv_color_hex(0x003300), 0);
        lv_obj_set_style_radius(btnConn, 12, 0);
        lv_obj_set_style_border_width(btnConn, 0, 0);
        lv_obj_add_event_cb(btnConn, onPassConnect, LV_EVENT_CLICKED, ta);
        lv_obj_t* ct = lv_label_create(btnConn);
        lv_label_set_text(ct, "Connect");
        lv_obj_set_style_text_font(ct, UI_F28, 0);
        lv_obj_center(ct);

        lv_obj_t* btnBack = lv_btn_create(s_scrPass);
        lv_obj_set_size(btnBack, 160, 80);
        lv_obj_align(btnBack, LV_ALIGN_BOTTOM_LEFT, 20, -20);
        lv_obj_set_style_bg_color(btnBack, lv_color_hex(UI_CARD), 0);
        lv_obj_set_style_radius(btnBack, 12, 0);
        lv_obj_set_style_border_width(btnBack, 0, 0);
        lv_obj_add_event_cb(btnBack, [](lv_event_t* e) { goBack(); }, LV_EVENT_CLICKED, NULL);
        lv_obj_t* bt = lv_label_create(btnBack);
        lv_label_set_text(bt, "Back");
        lv_obj_set_style_text_font(bt, UI_F28, 0);
        lv_obj_center(bt);
    }
    lv_obj_t* lbl = lv_obj_get_child(s_scrPass, 0);
    lv_label_set_text_fmt(lbl, "Connect to: %s", s_selSsid);
    lv_textarea_set_text(lv_obj_get_child(s_scrPass, 1), "");
    lv_scr_load(s_scrPass);
    if (!s_kb) {
        s_kb = lv_keyboard_create(s_scrPass);
        lv_keyboard_set_textarea(s_kb, lv_obj_get_child(s_scrPass, 1));
    } else {
        lv_keyboard_set_textarea(s_kb, lv_obj_get_child(s_scrPass, 1));
    }
}

// Scan button callback
static void onScanStart(lv_event_t* e) {
    lv_obj_clean(w_scanList);
    lv_list_add_text(w_scanList, "Scanning...");
    s_scanDone = false;
    WiFi.scanNetworks(true);  // async scan
}

void ui_wifi_refresh() {
    if (!scr) return;
    if (wifiMgrConnected()) {
        ui_label_setf(w_currSsid, "Connected: %s", WiFi.SSID().c_str());
        ui_label_setf(w_currIp, "IP: %s", wifiMgrLocalIP().c_str());
    } else {
        lv_label_set_text(w_currSsid, "Not connected");
        lv_label_set_text(w_currIp, "");
    }
}

bool ui_wifi_setup_poll_scan() {
    if (s_scanDone) return false;
    int n = WiFi.scanComplete();
    if (n >= 0) {
        s_scanDone = true;
        lv_obj_clean(w_scanList);
        for (int i = 0; i < n; i++) {
            String label = WiFi.SSID(i);
            label += "  (";
            label += WiFi.RSSI(i);
            label += "dBm)";
            if (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) label += " *";
            lv_obj_t* btn = lv_list_add_btn(w_scanList, NULL, label.c_str());
            lv_obj_set_user_data(btn, (void*)(intptr_t)i);
            lv_obj_set_style_text_font(btn, UI_F20, 0);
            lv_obj_add_event_cb(btn, [](lv_event_t* ev) {
                int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(ev));
                showPassScreen(WiFi.SSID(idx).c_str());
            }, LV_EVENT_CLICKED, NULL);
        }
        if (n == 0) lv_list_add_text(w_scanList, "No networks found");
        return true;
    }
    return false;
}

lv_obj_t* ui_wifi_create(lv_obj_t* parent) {
    scr = lv_obj_create(parent);
    lv_obj_set_size(scr, UI_W, UI_H - UI_TAB_H);
    lv_obj_set_pos(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    lv_obj_t* title = ui_label(scr, 0, 4, UI_W, UI_GREY, UI_F32);
    lv_label_set_text(title, LV_SYMBOL_WIFI "  WiFi Setup");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    // Current connection
    w_currSsid = ui_label(scr, 20, 50, UI_W-40, UI_WHITE, UI_F24);
    lv_label_set_text(w_currSsid, "Not connected");
    w_currIp = ui_label(scr, 20, 82, UI_W-40, UI_DIM, UI_F18);

    // Scan button
    w_scanBtn = lv_btn_create(scr);
    lv_obj_set_size(w_scanBtn, 260, 64);
    lv_obj_set_pos(w_scanBtn, 20, 120);
    lv_obj_set_style_bg_color(w_scanBtn, lv_color_hex(UI_CARD), 0);
    lv_obj_set_style_radius(w_scanBtn, 12, 0);
    lv_obj_set_style_border_width(w_scanBtn, 0, 0);
    lv_obj_set_style_shadow_width(w_scanBtn, 0, 0);
    lv_obj_add_event_cb(w_scanBtn, onScanStart, LV_EVENT_CLICKED, NULL);
    lv_obj_t* sl = lv_label_create(w_scanBtn);
    lv_label_set_text(sl, LV_SYMBOL_REFRESH "  Scan");
    lv_obj_set_style_text_font(sl, UI_F24, 0);
    lv_obj_center(sl);

    // Scan results list
    w_scanList = lv_list_create(scr);
    lv_obj_set_size(w_scanList, UI_W - 20, 950);
    lv_obj_set_pos(w_scanList, 10, 200);
    lv_obj_set_style_bg_color(w_scanList, lv_color_hex(UI_BG), 0);
    lv_obj_set_style_border_width(w_scanList, 0, 0);
    lv_obj_set_style_text_font(w_scanList, UI_F20, 0);
    lv_list_add_text(w_scanList, "Press Scan to search");

    return scr;
}
