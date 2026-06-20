// ui_dashboard.h — Tab 0: Joint angle dashboard + status
#pragma once
#include <lvgl.h>

// Create dashboard page, return content object
lv_obj_t *ui_dashboard_create(lv_obj_t *parent);

// Called periodically to update arcs + status
void ui_dashboard_refresh(void);
