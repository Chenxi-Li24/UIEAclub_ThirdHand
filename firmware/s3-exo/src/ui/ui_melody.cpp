#include "ui/ui_melody.h"

#include "hw/audio.h"
#include "hw/melody.h"
#include "ui/ui_core.h"

static lv_obj_t* s_screen = nullptr;

static void goBack(lv_event_t* event) {
  if (!ui_event_is_tap(event)) return;
  ui_melody_close();
}

void ui_melody_close() {
  if (!s_screen) return;
  lv_obj_t* oldScreen = s_screen;
  s_screen = nullptr;
  ui_gesture_enable(true);
  ui_screen_goto(4, false);
  if (oldScreen) lv_obj_del_async(oldScreen);
}

static void melodyButton(lv_event_t* event) {
  if (!ui_event_is_tap(event)) return;
  const MelodyDef* melody = static_cast<const MelodyDef*>(lv_event_get_user_data(event));
  if (melody) melodyPlay(melody);
}

static void pianoButton(lv_event_t* event) {
  if (!ui_event_is_tap(event)) return;
  const uintptr_t frequency = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
  hwBeep(static_cast<uint16_t>(frequency), 200);
}

static lv_obj_t* makeButton(lv_obj_t* parent, int x, int y, int w, int h,
                            const char* text, lv_event_cb_t callback,
                            const void* userData) {
  lv_obj_t* button = lv_btn_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, w, h);
  lv_obj_set_style_bg_color(button, lv_color_hex(UI_CARD), 0);
  lv_obj_set_style_radius(button, 9, 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_add_flag(button, LV_OBJ_FLAG_GESTURE_BUBBLE);
  if (callback) lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED,
                                    const_cast<void*>(userData));

  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(UI_WHITE), 0);
  lv_obj_set_style_text_font(label, UI_F10, 0);
  lv_obj_center(label);
  return button;
}

lv_obj_t* ui_melody_create() {
  if (s_screen) return s_screen;
  s_screen = ui_screen_create();

  lv_obj_t* back = lv_btn_create(s_screen);
  lv_obj_set_pos(back, 6, 3);
  lv_obj_set_size(back, 34, 22);
  lv_obj_set_style_bg_color(back, lv_color_hex(UI_CARD), 0);
  lv_obj_set_style_radius(back, 7, 0);
  lv_obj_set_style_shadow_width(back, 0, 0);
  lv_obj_add_event_cb(back, goBack, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* backLabel = lv_label_create(back);
  lv_label_set_text(backLabel, "<");
  lv_obj_set_style_text_color(backLabel, lv_color_hex(UI_AMBER), 0);
  lv_obj_center(backLabel);

  lv_obj_t* title = ui_label(s_screen, 44, 4, 152, UI_WHITE, UI_F16);
  lv_label_set_text(title, "BUZZER");
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t* body = ui_page_content(s_screen, true);
  const int buttonWidth = 108;
  const int buttonHeight = 42;

  for (uint8_t i = 0; i < MELODY_COUNT; i++) {
    const int column = i % 2;
    const int row = i / 2;
    makeButton(body, 8 + column * 116, 3 + row * 48, buttonWidth,
               buttonHeight, MELODY_LIST[i]->name, melodyButton,
               MELODY_LIST[i]);
  }

  ui_divider(body, 151);
  lv_obj_t* pianoTitle = ui_label(body, 8, 160, 224, UI_GREY, UI_F10);
  lv_label_set_text(pianoTitle, "PIANO");

  static const uint16_t frequencies[] = {262, 294, 330, 349, 392, 440, 494, 523};
  static const char* const names[] = {"C", "D", "E", "F", "G", "A", "B", "c"};
  for (int i = 0; i < 8; i++) {
    lv_obj_t* key = makeButton(body, 4 + i * 29, 178, 26, 44, names[i],
                               pianoButton,
                               reinterpret_cast<void*>(static_cast<uintptr_t>(frequencies[i])));
    lv_obj_set_style_bg_color(key, lv_color_hex(UI_BLUE), 0);
  }

  return s_screen;
}

void ui_melody_show() {
  if (!s_screen) ui_melody_create();
  ui_gesture_enable(false);
  lv_scr_load(s_screen);
}

void ui_melody_refresh() {}
