// ui_system.cpp — Page 2: System screen
#include "ui/ui_system.h"

#include "hw/power.h"
#include "ui/ui_core.h"

#include <LittleFS.h>

static lv_obj_t *scr;
static lv_obj_t *w_heap, *w_uptime, *w_flash, *w_bat;
static lv_obj_t *bar_heap, *bar_bat;

void ui_system_refresh() {
  if (!scr)
    return;

  // Heap
  uint32_t freeH = ESP.getFreeHeap();
  uint32_t totalH = ESP.getHeapSize();
  ui_label_setf(w_heap, "%uK free", freeH / 1024);
  if (bar_heap) {
    lv_bar_set_value(bar_heap, (int32_t)(freeH / 1024), LV_ANIM_ON);
    lv_bar_set_range(bar_heap, 0, (int32_t)(totalH / 1024));
    uint32_t hc = freeH > 100 * 1024 ? UI_GREEN : freeH > 50 * 1024 ? UI_AMBER : UI_RED;
    lv_obj_set_style_bg_color(bar_heap, lv_color_hex(hc), LV_PART_INDICATOR);
  }

  // Uptime
  uint32_t up = millis() / 1000;
  ui_label_setf(w_uptime, "%uh %um", up / 3600, (up % 3600) / 60);

  // Flash
  ui_label_setf(w_flash, "%.1f / %.1f MB", LittleFS.usedBytes() / 1048576.0f,
                LittleFS.totalBytes() / 1048576.0f);

  // Battery
  HwBattery bat = hwBattery();
  if (bat.mV > 0) {
    ui_label_setf(w_bat, "%d%%", bat.pct);
    if (bar_bat) {
      lv_bar_set_value(bar_bat, bat.pct, LV_ANIM_ON);
      uint32_t bc = bat.pct > 50 ? UI_GREEN : bat.pct > 20 ? UI_AMBER : UI_RED;
      lv_obj_set_style_bg_color(bar_bat, lv_color_hex(bc), LV_PART_INDICATOR);
    }
  } else {
    lv_label_set_text(w_bat, "--");
    if (bar_bat)
      lv_bar_set_value(bar_bat, 100, LV_ANIM_ON);
  }
}

lv_obj_t *ui_system_create() {
  scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG), 0);
  lv_obj_set_style_pad_all(scr, 0, 0);

  lv_obj_t *title = ui_label(scr, 12, 4, 216, UI_GREY, UI_F12);
  lv_label_set_text(title, "SYSTEM");

  int y = 30;
  ui_label(scr, 12, y, 100, UI_GREY, UI_F10);
  lv_label_set_text(lv_obj_get_child(scr, lv_obj_get_child_cnt(scr) - 1), "HEAP");

  bar_heap = lv_bar_create(scr);
  lv_obj_set_size(bar_heap, 180, 8);
  lv_obj_set_pos(bar_heap, 12, y + 14);
  lv_obj_set_style_bg_color(bar_heap, lv_color_hex(0x2104), LV_PART_MAIN);
  lv_obj_set_style_radius(bar_heap, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(bar_heap, 4, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(bar_heap, lv_color_hex(UI_GREEN), LV_PART_INDICATOR);
  lv_bar_set_range(bar_heap, 0, 512);

  w_heap = ui_label(scr, 12, y + 26, 216, UI_WHITE, UI_F12);
  lv_label_set_text(w_heap, "0K free");

  y += 60;
  ui_label(scr, 12, y, 100, UI_GREY, UI_F10);
  lv_label_set_text(lv_obj_get_child(scr, lv_obj_get_child_cnt(scr) - 1), "UPTIME");
  w_uptime = ui_label(scr, 12, y + 14, 216, UI_WHITE, UI_F20);
  lv_label_set_text(w_uptime, "0h 0m");

  y += 52;
  ui_label(scr, 12, y, 100, UI_GREY, UI_F10);
  lv_label_set_text(lv_obj_get_child(scr, lv_obj_get_child_cnt(scr) - 1), "FLASH");
  lv_obj_t *flashCard = ui_card(scr, 12, y + 14, 216, 32, UI_CARD);
  w_flash = ui_label(flashCard, 8, 8, 200, UI_WHITE, UI_F12);
  lv_label_set_text(w_flash, "0 / 0 MB");

  y += 60;
  ui_label(scr, 12, y, 100, UI_GREY, UI_F10);
  lv_label_set_text(lv_obj_get_child(scr, lv_obj_get_child_cnt(scr) - 1), "BATTERY");

  w_bat = ui_label(scr, 12, y + 14, 72, UI_WHITE, UI_F20);
  lv_label_set_text(w_bat, "--");

  bar_bat = lv_bar_create(scr);
  lv_obj_set_size(bar_bat, 140, 6);
  lv_obj_set_pos(bar_bat, 88, y + 24);
  lv_obj_set_style_bg_color(bar_bat, lv_color_hex(0x2104), LV_PART_MAIN);
  lv_obj_set_style_radius(bar_bat, 3, LV_PART_MAIN);
  lv_obj_set_style_radius(bar_bat, 3, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(bar_bat, lv_color_hex(UI_GREEN), LV_PART_INDICATOR);
  lv_bar_set_range(bar_bat, 0, 100);

  ui_add_page_dots(scr, 2);
  return scr;
}
