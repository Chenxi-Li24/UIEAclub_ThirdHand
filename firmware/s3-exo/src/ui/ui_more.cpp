#include "ui/ui_more.h"

#include "ui/ui_core.h"
#include "ui/ui_melody.h"

static void openBuzzer(lv_event_t* event) {
  if (!ui_event_is_tap(event)) return;
  ui_melody_show();
}

lv_obj_t* ui_more_create() {
  lv_obj_t* screen = ui_screen_create();

  lv_obj_t* title = ui_label(screen, 12, 4, 216, UI_GREY, UI_F12);
  lv_label_set_text(title, "MORE");

  lv_obj_t* body = ui_page_content(screen, true);

  lv_obj_t* sound = ui_card(body, 8, 4, 224, 62, UI_CARD);
  ui_prepare_clickable(sound);
  lv_obj_add_event_cb(sound, openBuzzer, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* soundTitle = ui_label(sound, 12, 7, 172, UI_WHITE, UI_F16);
  lv_label_set_text(soundTitle, "Buzzer");
  lv_obj_t* soundSub = ui_label(sound, 12, 31, 178, UI_GREY, UI_F10);
  lv_label_set_text(soundSub, "Melodies and piano keyboard");
  lv_obj_t* arrow = ui_label(sound, 198, 18, 18, UI_AMBER, UI_F20);
  lv_label_set_text(arrow, ">");

  lv_obj_t* touch = ui_card(body, 8, 78, 224, 78, 0x1082);
  lv_obj_t* touchTitle = ui_label(touch, 12, 8, 196, UI_WHITE, UI_F12);
  lv_label_set_text(touchTitle, "Touch controls");
  lv_obj_t* touchSub = ui_label(touch, 12, 31, 196, UI_GREY, UI_F10);
  lv_label_set_text(touchSub, "Swipe left/right: change page\nSwipe up/down: scroll page\nTap: activate a control");
  lv_label_set_long_mode(touchSub, LV_LABEL_LONG_WRAP);

  lv_obj_t* about = ui_card(body, 8, 168, 224, 58, UI_CARD);
  lv_obj_t* aboutTitle = ui_label(about, 12, 7, 196, UI_WHITE, UI_F12);
  lv_label_set_text(aboutTitle, "Fairino Mini Controller");
  lv_obj_t* aboutSub = ui_label(about, 12, 31, 196, UI_DIM, UI_F10);
  lv_label_set_text(aboutSub, "ESP32-S3  |  LVGL 8.4");

  ui_add_page_dots(screen, 4);
  return screen;
}

void ui_more_refresh() {}
