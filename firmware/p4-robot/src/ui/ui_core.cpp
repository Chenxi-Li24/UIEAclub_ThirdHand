// ui_core.cpp - TabView navigation + theme + shared helpers
// Board: JC8012P4A1, Display: JD9365DA MIPI DSI 800x1280
#include <Arduino.h>
#include "ui/ui_core.h"
#include "ui/ui_dashboard.h"
#include "ui/ui_control.h"
#include "ui/ui_system.h"
#include "ui/ui_wifi.h"
#include "ui/ui_about.h"
#include <cstdarg>
#include <cstdio>

const lv_font_t* UI_F14 = &lv_font_montserrat_14;
const lv_font_t* UI_F16 = &lv_font_montserrat_16;
const lv_font_t* UI_F18 = &lv_font_montserrat_18;
const lv_font_t* UI_F20 = &lv_font_montserrat_20;
const lv_font_t* UI_F24 = &lv_font_montserrat_24;
const lv_font_t* UI_F28 = &lv_font_montserrat_28;
const lv_font_t* UI_F32 = &lv_font_montserrat_32;
const lv_font_t* UI_F48 = &lv_font_montserrat_48;

// 鈹€鈹€ Tab names 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
static const char* TAB_NAMES[] = {
    LV_SYMBOL_HOME " Dashboard",
    LV_SYMBOL_SETTINGS " Control",
    LV_SYMBOL_LIST " System",
    LV_SYMBOL_WIFI " WiFi",
    LV_SYMBOL_FILE " About"
};
#define TAB_COUNT 5

static lv_obj_t* s_tabview = nullptr;
static uint8_t s_current = 0;

typedef void (*refresh_fn_t)();
static refresh_fn_t s_refreshers[TAB_COUNT] = {nullptr};

void ui_core_init() {
    s_tabview = lv_tabview_create(lv_scr_act(), LV_DIR_BOTTOM, UI_TAB_H);

    // Tab pages (order must match s_refreshers)
    lv_obj_t* tabs[TAB_COUNT];
    tabs[0] = lv_tabview_add_tab(s_tabview, TAB_NAMES[0]);
    tabs[1] = lv_tabview_add_tab(s_tabview, TAB_NAMES[1]);
    tabs[2] = lv_tabview_add_tab(s_tabview, TAB_NAMES[2]);
    tabs[3] = lv_tabview_add_tab(s_tabview, TAB_NAMES[3]);
    tabs[4] = lv_tabview_add_tab(s_tabview, TAB_NAMES[4]);

    // Create page content on each tab (pass correct tab page as parent)
    ui_dashboard_create(tabs[0]);
    ui_control_create(tabs[1]);
    ui_system_create(tabs[2]);
    ui_wifi_create(tabs[3]);
    ui_about_create(tabs[4]);

    // Register refresh callbacks
    s_refreshers[0] = ui_dashboard_refresh;
    s_refreshers[1] = ui_control_refresh;
    s_refreshers[2] = ui_system_refresh;
    s_refreshers[3] = ui_wifi_refresh;
    s_refreshers[4] = ui_about_refresh;

    // Set tab bar style
    lv_obj_t* tabBar = lv_tabview_get_tab_btns(s_tabview);
    lv_obj_set_style_bg_color(tabBar, lv_color_hex(0x0841), 0);
    lv_obj_set_style_border_width(tabBar, 0, 0);
    lv_obj_set_style_pad_top(tabBar, 8, 0);

    // Initial tab
    lv_tabview_set_act(s_tabview, 0, LV_ANIM_OFF);
    s_current = 0;

    Serial.println("[UI] TabView 5 pages ready");
}

void ui_refresh_all() {
    uint8_t idx = lv_tabview_get_tab_act(s_tabview);
    if (idx < TAB_COUNT && s_refreshers[idx]) {
        s_refreshers[idx]();
    }
}

// 鈹€鈹€ Helpers 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
lv_obj_t* ui_label(lv_obj_t* parent, int x, int y, int w, uint32_t color, const lv_font_t* font) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_pos(l, x, y);
    if (w) lv_obj_set_width(l, w);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    if (font) lv_obj_set_style_text_font(l, font, 0);
    return l;
}

void ui_label_setf(lv_obj_t* label, const char* fmt, ...) {
    if (!label) return;
    char buf[64];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    lv_label_set_text(label, buf);
}

lv_obj_t* ui_card(lv_obj_t* parent, int x, int y, int w, int h, uint32_t bg_color) {
    lv_obj_t* b = lv_obj_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(bg_color), 0);
    lv_obj_set_style_radius(b, 16, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 12, 0);
    return b;
}

void ui_divider(lv_obj_t* parent, int y, int w) {
    lv_obj_t* d = lv_obj_create(parent);
    lv_obj_set_size(d, w, 2);
    lv_obj_set_pos(d, 10, y);
    lv_obj_set_style_bg_color(d, lv_color_hex(0x3186), 0);
    lv_obj_set_style_border_width(d, 0, 0);
}

lv_obj_t* ui_tabview() { return s_tabview; }
