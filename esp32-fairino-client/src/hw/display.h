// src/hw/display.h — ST7789V 240x280 + LVGL 渲染

#pragma once
#include "hw/pins.h"

#include <Arduino_GFX_Library.h>
#include <lvgl.h>

// 底层 GFX 对象（供 LVGL flush 回调使用）
Arduino_GFX *hwGfx();

bool hwDisplayInit();      // 初始化 ST7789 + 背光
bool hwDisplayInitLVGL();  // 初始化 LVGL（需先调 hwDisplayInit）
void hwDisplayBrightness(uint8_t lvl_0_4);
void hwDisplaySleep(bool off);
