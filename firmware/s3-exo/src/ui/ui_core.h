// ui_core.h — Screen manager: 5 swipeable pages + gesture navigation
#pragma once
#include <lvgl.h>

#define UI_SCREEN_COUNT 5

void ui_core_init();
void ui_screen_next();
void ui_screen_prev();
void ui_screen_goto(uint8_t idx, bool animate = true);
uint8_t ui_current_screen();
void ui_refresh_all();
void ui_gesture_enable(bool enable);

// Shared palette (RGB565)
#define UI_BG 0x0000
#define UI_CARD 0x18E3
#define UI_WHITE 0xFFFF
#define UI_GREY 0x8410
#define UI_DIM 0x630C
#define UI_GREEN 0x368B
#define UI_AMBER 0xFD20
#define UI_RED 0xF8A7
#define UI_BLUE 0x0A9F

extern const lv_font_t* UI_F10;
extern const lv_font_t* UI_F12;
extern const lv_font_t* UI_F16;
extern const lv_font_t* UI_F20;
extern const lv_font_t* UI_F24;

lv_obj_t* ui_label(lv_obj_t* parent, int x, int y, int w, uint32_t color, const lv_font_t* font);
void ui_label_setf(lv_obj_t* label, const char* fmt, ...);
lv_obj_t* ui_card(lv_obj_t* parent, int x, int y, int w, int h, uint32_t bg_color = UI_CARD);
void ui_add_page_dots(lv_obj_t* scr, uint8_t page_idx);
void ui_divider(lv_obj_t* parent, int y);
