// ui_core.cpp — Screen manager: 5 screens for robot controller
#include "ui/ui_core.h"
#include "ui/ui_home.h"
#include "ui/ui_robot.h"
#include "ui/ui_system.h"
#include "ui/ui_settings.h"
#include "ui/ui_melody.h"
#include "ui/ui_more.h"
#include "hw/input.h"
#include <Arduino.h>
#include <cstdarg>
#include <cstdio>

const lv_font_t* UI_F10 = &lv_font_montserrat_12;
const lv_font_t* UI_F12 = &lv_font_montserrat_14;
const lv_font_t* UI_F16 = &lv_font_montserrat_16;
const lv_font_t* UI_F20 = &lv_font_montserrat_20;
const lv_font_t* UI_F24 = &lv_font_montserrat_24;

lv_obj_t* ui_screen_create(bool vertical_scroll) {
  (void)vertical_scroll;
  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(UI_BG), 0);
  lv_obj_set_style_pad_all(screen, 0, 0);
  lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  return screen;
}

lv_obj_t* ui_page_content(lv_obj_t* screen, bool vertical_scroll) {
  lv_obj_t* body = lv_obj_create(screen);
  lv_obj_set_pos(body, 0, 27);
  lv_obj_set_size(body, 240, 233);
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(body, 0, 0);
  lv_obj_set_style_radius(body, 0, 0);
  lv_obj_set_style_pad_all(body, 0, 0);
  lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);
  if (vertical_scroll) {
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  } else {
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  }
  return body;
}

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
  lv_obj_set_style_radius(b, 12, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_pad_all(b, 6, 0);
  lv_obj_set_scrollbar_mode(b, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);
  return b;
}

void ui_prepare_clickable(lv_obj_t* obj) {
  if (!obj) return;
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
}

bool ui_event_is_tap(lv_event_t* event) {
  (void)event;
  return hwInputClickAllowed();
}

void ui_divider(lv_obj_t* parent, int y) {
  lv_obj_t* d = lv_obj_create(parent);
  lv_obj_set_size(d, 224, 1);
  lv_obj_set_pos(d, 8, y);
  lv_obj_set_style_bg_color(d, lv_color_hex(0x2104), 0);
  lv_obj_set_style_border_width(d, 0, 0);
}

void ui_add_page_dots(lv_obj_t* scr, uint8_t page_idx) {
  int dot_sz = 6, gap = 14;
  int total_w = UI_SCREEN_COUNT * dot_sz + (UI_SCREEN_COUNT - 1) * gap;
  int start_x = (240 - total_w) / 2;
  int y = 268;
  for (int i = 0; i < UI_SCREEN_COUNT; i++) {
    lv_obj_t* dot = lv_obj_create(scr);
    lv_obj_set_size(dot, dot_sz, dot_sz);
    lv_obj_set_pos(dot, start_x + i * (dot_sz + gap), y);
    lv_obj_set_style_bg_color(dot, lv_color_hex(i == page_idx ? UI_WHITE : UI_DIM), 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(dot, LV_OBJ_FLAG_FLOATING);
  }
}

static lv_obj_t* s_screens[UI_SCREEN_COUNT] = {nullptr};
static uint8_t s_current = 0;
static bool s_gesture_enabled = true;
static bool s_animating = false;

typedef void (*refresh_fn_t)();
static refresh_fn_t s_refreshers[UI_SCREEN_COUNT] = {nullptr};

static void gesture_cb(lv_event_t* e) {
  if (!s_gesture_enabled || s_animating) return;
  lv_indev_t* indev = lv_indev_get_act();
  if (!indev) return;
  lv_dir_t dir = lv_indev_get_gesture_dir(indev);
  if (dir == LV_DIR_LEFT)  ui_screen_next();
  if (dir == LV_DIR_RIGHT) ui_screen_prev();
}

static void anim_done_cb(lv_timer_t* t) { s_animating = false; lv_timer_del(t); }

void ui_screen_goto(uint8_t idx, bool animate) {
  if (idx >= UI_SCREEN_COUNT || s_animating) return;
  if (idx == s_current) {
    // Modal screens do not change s_current. Loading the current primary page
    // is therefore also the Back operation for WiFi and Buzzer screens.
    if (lv_scr_act() != s_screens[idx]) lv_scr_load(s_screens[idx]);
    return;
  }
  uint8_t old = s_current;
  s_current = idx;
  s_animating = true;
  if (animate) {
    lv_scr_load_anim(s_screens[idx], idx > old ? LV_SCR_LOAD_ANIM_OVER_LEFT : LV_SCR_LOAD_ANIM_OVER_RIGHT, 280, 0, false);
    lv_timer_create(anim_done_cb, 320, NULL);
  } else {
    lv_scr_load(s_screens[idx]);
    s_animating = false;
  }
}

void ui_screen_next() { if (s_current + 1 < UI_SCREEN_COUNT) ui_screen_goto(s_current + 1); }
void ui_screen_prev() { if (s_current > 0) ui_screen_goto(s_current - 1); }
uint8_t ui_current_screen() { return s_current; }
void ui_gesture_enable(bool e) { s_gesture_enabled = e; }

void ui_refresh_all() {
  if (s_refreshers[s_current]) s_refreshers[s_current]();
}

void ui_core_init() {
  Serial.printf("[UI] create Home, heap=%u\n", ESP.getFreeHeap());
  s_screens[0] = ui_home_create();
  Serial.printf("[UI] create Robot, heap=%u\n", ESP.getFreeHeap());
  s_screens[1] = ui_robot_create();
  Serial.printf("[UI] create System, heap=%u\n", ESP.getFreeHeap());
  s_screens[2] = ui_system_create();
  Serial.printf("[UI] create Settings, heap=%u\n", ESP.getFreeHeap());
  s_screens[3] = ui_settings_create();
  Serial.printf("[UI] create More, heap=%u\n", ESP.getFreeHeap());
  s_screens[4] = ui_more_create();

  s_refreshers[0] = ui_home_refresh;
  s_refreshers[1] = ui_robot_refresh;
  s_refreshers[2] = ui_system_refresh;
  s_refreshers[3] = ui_settings_refresh;
  s_refreshers[4] = ui_more_refresh;

  for (int i = 0; i < UI_SCREEN_COUNT; i++) {
    lv_obj_add_event_cb(s_screens[i], gesture_cb, LV_EVENT_GESTURE, NULL);
  }
  Serial.printf("[UI] primary screens ready, heap=%u\n", ESP.getFreeHeap());
  lv_scr_load(s_screens[0]);
  s_current = 0;
}
