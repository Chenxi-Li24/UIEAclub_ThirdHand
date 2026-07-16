#include "ui/ui_system.h"

#include "hw/power.h"
#include "ui/ui_core.h"

#include <LittleFS.h>

static lv_obj_t* s_screen = nullptr;
static lv_obj_t* s_heap = nullptr;
static lv_obj_t* s_uptime = nullptr;
static lv_obj_t* s_flash = nullptr;
static lv_obj_t* s_battery = nullptr;
static lv_obj_t* s_heapBar = nullptr;
static lv_obj_t* s_batteryBar = nullptr;

static lv_obj_t* makeBar(lv_obj_t* parent, int x, int y, int width) {
  lv_obj_t* bar = lv_bar_create(parent);
  lv_obj_set_pos(bar, x, y);
  lv_obj_set_size(bar, width, 7);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x2104), LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(bar, lv_color_hex(UI_GREEN), LV_PART_INDICATOR);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  return bar;
}

void ui_system_refresh() {
  if (!s_screen) return;

  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint32_t totalHeap = ESP.getHeapSize();
  ui_label_setf(s_heap, "%u KB free / %u KB", freeHeap / 1024,
                totalHeap / 1024);
  lv_bar_set_range(s_heapBar, 0, static_cast<int32_t>(totalHeap / 1024));
  lv_bar_set_value(s_heapBar, static_cast<int32_t>(freeHeap / 1024), LV_ANIM_ON);
  const uint32_t heapColor =
      freeHeap > 100 * 1024 ? UI_GREEN : freeHeap > 40 * 1024 ? UI_AMBER : UI_RED;
  lv_obj_set_style_bg_color(s_heapBar, lv_color_hex(heapColor), LV_PART_INDICATOR);

  const uint32_t uptime = millis() / 1000;
  ui_label_setf(s_uptime, "%u h  %u min", uptime / 3600,
                (uptime % 3600) / 60);
  ui_label_setf(s_flash, "%.2f MB used / %.2f MB",
                LittleFS.usedBytes() / 1048576.0f,
                LittleFS.totalBytes() / 1048576.0f);

  const HwBattery battery = hwBattery();
  if (battery.mV > 0) {
    ui_label_setf(s_battery, "%d%%   %u mV", battery.pct, battery.mV);
    lv_bar_set_value(s_batteryBar, battery.pct, LV_ANIM_ON);
    const uint32_t color =
        battery.pct > 50 ? UI_GREEN : battery.pct > 20 ? UI_AMBER : UI_RED;
    lv_obj_set_style_bg_color(s_batteryBar, lv_color_hex(color),
                              LV_PART_INDICATOR);
  } else {
    lv_label_set_text(s_battery, "Not detected");
    lv_bar_set_value(s_batteryBar, 0, LV_ANIM_OFF);
  }
}

lv_obj_t* ui_system_create() {
  s_screen = ui_screen_create();
  lv_obj_t* title = ui_label(s_screen, 12, 4, 216, UI_GREY, UI_F12);
  lv_label_set_text(title, "SYSTEM STATUS");

  lv_obj_t* body = ui_page_content(s_screen, true);

  lv_obj_t* heapCard = ui_card(body, 8, 4, 224, 64, UI_CARD);
  lv_obj_t* heapTitle = ui_label(heapCard, 10, 5, 196, UI_GREY, UI_F10);
  lv_label_set_text(heapTitle, "MEMORY");
  s_heap = ui_label(heapCard, 10, 23, 196, UI_WHITE, UI_F12);
  lv_label_set_text(s_heap, "-- KB free");
  s_heapBar = makeBar(heapCard, 10, 45, 196);

  lv_obj_t* uptimeCard = ui_card(body, 8, 78, 224, 58, 0x1082);
  lv_obj_t* uptimeTitle = ui_label(uptimeCard, 10, 6, 196, UI_GREY, UI_F10);
  lv_label_set_text(uptimeTitle, "UPTIME");
  s_uptime = ui_label(uptimeCard, 10, 26, 196, UI_WHITE, UI_F20);
  lv_label_set_text(s_uptime, "0 h  0 min");

  lv_obj_t* flashCard = ui_card(body, 8, 146, 224, 58, UI_CARD);
  lv_obj_t* flashTitle = ui_label(flashCard, 10, 6, 196, UI_GREY, UI_F10);
  lv_label_set_text(flashTitle, "LITTLEFS STORAGE");
  s_flash = ui_label(flashCard, 10, 28, 196, UI_WHITE, UI_F12);
  lv_label_set_text(s_flash, "0 / 0 MB");

  lv_obj_t* batteryCard = ui_card(body, 8, 214, 224, 66, UI_CARD);
  lv_obj_t* batteryTitle = ui_label(batteryCard, 10, 5, 196, UI_GREY, UI_F10);
  lv_label_set_text(batteryTitle, "BATTERY");
  s_battery = ui_label(batteryCard, 10, 23, 196, UI_WHITE, UI_F16);
  lv_label_set_text(s_battery, "Not detected");
  s_batteryBar = makeBar(batteryCard, 10, 49, 196);
  lv_bar_set_range(s_batteryBar, 0, 100);

  lv_obj_t* hint = ui_label(body, 8, 291, 224, UI_DIM, UI_F10);
  lv_label_set_text(hint, "Swipe up/down to view all system data");
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

  ui_add_page_dots(s_screen, 2);
  return s_screen;
}
