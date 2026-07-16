// ui_system.h — Tab 2: System info (memory, WiFi, connections)
#pragma once
#include <lvgl.h>

lv_obj_t *ui_system_create(lv_obj_t *parent);
void ui_system_refresh(void);
