// ui_melody.cpp — Melody player screen (page 4)
#include "ui/ui_melody.h"
#include "ui/ui_core.h"
#include "hw/melody.h"
#include "hw/audio.h"

static lv_obj_t *scr;

static void melody_btn_cb(lv_event_t *e) {
  const MelodyDef *m = (const MelodyDef *)lv_event_get_user_data(e);
  if (m) melodyPlay(m);
}

static void piano_cb(lv_event_t *e) {
  uintptr_t freq = (uintptr_t)lv_event_get_user_data(e);
  hwBeep((uint16_t)freq, 200);
}

static lv_obj_t *make_btn(lv_obj_t *parent, int x, int y, int w, int h,
                          const char *text, lv_event_cb_t cb, const void *user_data) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_bg_color(btn, lv_color_hex(UI_CARD), 0);
  lv_obj_set_style_radius(btn, 10, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, (void *)user_data);

  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_color(lbl, lv_color_hex(UI_WHITE), 0);
  lv_obj_set_style_text_font(lbl, UI_F12, 0);
  lv_obj_center(lbl);
  return btn;
}

lv_obj_t *ui_melody_create() {
  scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG), 0);
  lv_obj_set_style_pad_all(scr, 0, 0);

  lv_obj_t *title = ui_label(scr, 0, 4, 240, UI_WHITE, UI_F16);
  lv_label_set_text(title, "Melody Player");
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

  const int col_w = 108, btn_h = 42, gap = 8;
  const int start_x = 12, start_y = 30;

  for (uint8_t i = 0; i < MELODY_COUNT; i++) {
    int col = i % 2;
    int row = i / 2;
    make_btn(scr, start_x + col * (col_w + gap), start_y + row * (btn_h + gap),
             col_w, btn_h, MELODY_LIST[i]->name, melody_btn_cb, MELODY_LIST[i]);
  }

  ui_divider(scr, 192);

  static const uint16_t PIANO_FREQ[] = {262, 294, 330, 349, 392, 440, 494, 523};
  static const char *const PIANO_NAME[] = {"C", "D", "E", "F", "G", "A", "B", "c"};
  const int piano_y = 202, key_w = 28, key_h = 44;

  for (int i = 0; i < 8; i++) {
    lv_obj_t *key = lv_btn_create(scr);
    lv_obj_set_pos(key, 8 + i * (key_w + 2), piano_y);
    lv_obj_set_size(key, key_w, key_h);
    lv_obj_set_style_bg_color(key, lv_color_hex(0x0A9F), 0);
    lv_obj_set_style_radius(key, 6, 0);
    lv_obj_set_style_border_width(key, 0, 0);
    lv_obj_add_event_cb(key, piano_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)PIANO_FREQ[i]);
    lv_obj_t *kl = lv_label_create(key);
    lv_label_set_text(kl, PIANO_NAME[i]);
    lv_obj_set_style_text_color(kl, lv_color_hex(UI_WHITE), 0);
    lv_obj_set_style_text_font(kl, UI_F10, 0);
    lv_obj_center(kl);
  }

  ui_add_page_dots(scr, 4);
  return scr;
}

void ui_melody_refresh() {}
