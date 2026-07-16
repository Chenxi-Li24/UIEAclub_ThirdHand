// ui_wifi.h — Tab 3: WiFi setup (scan, connect, status)
#pragma once
#include <lvgl.h>

lv_obj_t *ui_wifi_create(lv_obj_t *parent);
void ui_wifi_refresh(void);

// Call from loop() after WiFi.scanComplete() returns
// Returns true if scan results were just updated
bool ui_wifi_setup_poll_scan(void);
