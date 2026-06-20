// ui_core.h - LVGL UI core: TabView navigation, theme, helpers
#pragma once
#include <lvgl.h>

// Geometry
#define UI_W        800
#define UI_H        1280
#define UI_TAB_H    80

// Dark theme palette (RGB565)
#define UI_BG       0x0000
#define UI_CARD     0x2104
#define UI_WHITE    0xFFFF
#define UI_GREY     0x8410
#define UI_DIM      0x528A
#define UI_GREEN    0x07E0
#define UI_AMBER    0xFD20
#define UI_RED      0xF800
#define UI_BLUE     0x001F
#define UI_CYAN     0x07FF

// Font references (set after lv_init)
extern const lv_font_t* UI_F14;
extern const lv_font_t* UI_F16;
extern const lv_font_t* UI_F18;
extern const lv_font_t* UI_F20;
extern const lv_font_t* UI_F24;
extern const lv_font_t* UI_F28;
extern const lv_font_t* UI_F32;
extern const lv_font_t* UI_F48;

// Tab view
lv_obj_t* ui_tabview(void);

// Init: create tabview, add 5 pages, setup theme
void ui_core_init(void);

// Called every ~200ms: dispatch refresh to current tab
void ui_refresh_all(void);

// Helper: create a label at (x,y) with given width, color, font
lv_obj_t* ui_label(lv_obj_t* parent, int x, int y, int w, uint32_t color, const lv_font_t* font);

// Helper: update label text with printf format
void ui_label_setf(lv_obj_t* label, const char* fmt, ...);

// Helper: card container (radius=16)
lv_obj_t* ui_card(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color);

// Helper: thin divider line
lv_obj_t* ui_divider(lv_obj_t* parent, int x, int y, int w);