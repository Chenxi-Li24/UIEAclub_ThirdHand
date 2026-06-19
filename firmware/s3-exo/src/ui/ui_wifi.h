// src/ui/ui_wifi.h — LVGL WiFi 设置屏幕
#pragma once
#include <lvgl.h>

// 创建 WiFi 扫描 + 密码输入 + 手动 SSID 屏幕
void ui_wifi_create();

// 加载 WiFi 扫描屏幕，启动异步扫描
void ui_wifi_show();

// 轮询扫描完成（main loop 中调用）
bool ui_wifi_poll_scan();

// 返回 WiFi 扫描屏幕对象
lv_obj_t *ui_wifi_screen();
