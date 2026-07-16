// ui_voice.h - Voice Control page (Tab 6)
// PTT microphone button, status, recognized text
#pragma once
#include <lvgl.h>

lv_obj_t* ui_voice_create(lv_obj_t* parent);
void ui_voice_refresh();
