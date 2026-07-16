// hw/display.h — JD9365DA MIPI DSI 800x1280 + LVGL framebuffer
#pragma once

#include "pins_config.h"
#include <lvgl.h>

// JD9365DA LCD object (defined in display.cpp)
class jd9365_lcd;
extern jd9365_lcd g_lcd;

// Initialize MIPI DSI display + backlight
bool hwDisplayInit();

// Initialize LVGL with PSRAM double-buffer (full_refresh)
bool hwDisplayInitLVGL();

// Backlight control
void hwDisplayBrightness(uint8_t lvl_0_4);
void hwDisplaySleep(bool off);
