// src/hw/input.h — CST816T 触摸 + LVGL 输入

#pragma once
#include <stdint.h>

// False while a finger is dragging and briefly after release. UI action
// callbacks use this to prevent a page swipe from becoming a button click.
bool hwInputClickAllowed();

bool hwInputInit();    // 初始化 CST816T I2C + 注册 LVGL 输入设备
void hwInputUpdate();  // LVGL 自行轮询，此函数可为空
