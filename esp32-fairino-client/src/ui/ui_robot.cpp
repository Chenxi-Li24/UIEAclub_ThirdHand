// ui_robot.cpp — Page 1: Robot Control — jog + self-test + quick commands
#include "ui/ui_robot.h"
#include "ui/ui_core.h"
#include "wifi_manager.h"
#include "fairino_udp.h"
#include "cnde_client.h"
#include "config.h"

static lv_obj_t *scr;
static lv_obj_t *w_jogLabels[6], *w_testStatus, *w_testProgress;
static float jogDelta = 5.0f;

// Helper: send ServoJ with delta from current position
static void jogJoint(int axis, float delta) {
  if (!wifiMgrConnected()) return;
  const RobotStateData& rs = g_cnde.getState();
  float joints[6] = {0};
  if (rs.valid) {
    for (int i = 0; i < 6; i++) joints[i] = rs.jointPos[i];
  }
  joints[axis] += delta;
  g_fairino.servoJ(joints[0], joints[1], joints[2], joints[3], joints[4], joints[5],
                   SELF_TEST_ACC, SELF_TEST_VEL, SELF_TEST_CMDT, 0, 0);
}

// Jog button callbacks
static void jogJ1p(lv_event_t* e) { jogJoint(0,  jogDelta); }
static void jogJ1m(lv_event_t* e) { jogJoint(0, -jogDelta); }
static void jogJ2p(lv_event_t* e) { jogJoint(1,  jogDelta); }
static void jogJ2m(lv_event_t* e) { jogJoint(1, -jogDelta); }
static void jogJ3p(lv_event_t* e) { jogJoint(2,  jogDelta); }
static void jogJ3m(lv_event_t* e) { jogJoint(2, -jogDelta); }
static void jogJ4p(lv_event_t* e) { jogJoint(3,  jogDelta); }
static void jogJ4m(lv_event_t* e) { jogJoint(3, -jogDelta); }
static void jogJ5p(lv_event_t* e) { jogJoint(4,  jogDelta); }
static void jogJ5m(lv_event_t* e) { jogJoint(4, -jogDelta); }
static void jogJ6p(lv_event_t* e) { jogJoint(5,  jogDelta); }
static void jogJ6m(lv_event_t* e) { jogJoint(5, -jogDelta); }

// Self-test trigger
extern void selfTestStart();
static void onSelfTest(lv_event_t* e) {
  if (!wifiMgrConnected()) return;
  selfTestStart();
}

// Quick commands
static void onServoStart(lv_event_t* e) {
  if (!wifiMgrConnected()) return;
  g_fairino.servoMoveStart();
}
static void onServoEnd(lv_event_t* e) {
  g_fairino.servoMoveEnd();
}
static void onTimingTest(lv_event_t* e) {
  if (!wifiMgrConnected()) return;
  g_fairino.servoTimingTest();
}

// Delta adjust
static void onDeltaUp(lv_event_t* e) {
  if (jogDelta < 20.0f) jogDelta += 1.0f;
}
static void onDeltaDown(lv_event_t* e) {
  if (jogDelta > 1.0f) jogDelta -= 1.0f;
}

static lv_obj_t* makeJogRow(lv_obj_t* parent, int y, const char* name,
                             lv_event_cb_t cbMinus, lv_event_cb_t cbPlus, int idx) {
  lv_obj_t* lbl = ui_label(parent, 8, y+2, 24, UI_GREY, UI_F10);
  lv_label_set_text(lbl, name);

  w_jogLabels[idx] = ui_label(parent, 36, y+2, 44, UI_WHITE, UI_F12);
  lv_label_set_text(w_jogLabels[idx], "--.-");

  lv_obj_t* btnM = lv_btn_create(parent);
  lv_obj_set_size(btnM, 28, 22);
  lv_obj_set_pos(btnM, 84, y);
  lv_obj_set_style_bg_color(btnM, lv_color_hex(0x330000), 0);
  lv_obj_set_style_radius(btnM, 4, 0);
  lv_obj_set_style_border_width(btnM, 0, 0);
  lv_obj_set_style_shadow_width(btnM, 0, 0);
  lv_obj_add_event_cb(btnM, cbMinus, LV_EVENT_CLICKED, NULL);
  lv_obj_t* tM = lv_label_create(btnM);
  lv_label_set_text(tM, "-");
  lv_obj_set_style_text_color(tM, lv_color_hex(UI_RED), 0);
  lv_obj_center(tM);

  lv_obj_t* btnP = lv_btn_create(parent);
  lv_obj_set_size(btnP, 28, 22);
  lv_obj_set_pos(btnP, 116, y);
  lv_obj_set_style_bg_color(btnP, lv_color_hex(0x003300), 0);
  lv_obj_set_style_radius(btnP, 4, 0);
  lv_obj_set_style_border_width(btnP, 0, 0);
  lv_obj_set_style_shadow_width(btnP, 0, 0);
  lv_obj_add_event_cb(btnP, cbPlus, LV_EVENT_CLICKED, NULL);
  lv_obj_t* tP = lv_label_create(btnP);
  lv_label_set_text(tP, "+");
  lv_obj_set_style_text_color(tP, lv_color_hex(UI_GREEN), 0);
  lv_obj_center(tP);

  return btnP;
}

void ui_robot_refresh() {
  if (!scr) return;
  const RobotStateData& rs = g_cnde.getState();
  if (rs.valid) {
    for (int i = 0; i < 6; i++) {
      ui_label_setf(w_jogLabels[i], "%.1f", rs.jointPos[i]);
    }
  }
}

lv_obj_t* ui_robot_create() {
  scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG), 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_set_scroll_dir(scr, LV_DIR_VER);

  lv_obj_t* title = ui_label(scr, 12, 4, 216, UI_GREY, UI_F12);
  lv_label_set_text(title, "ROBOT CONTROL");

  // Jog section — 6 rows
  int y = 28;
  makeJogRow(scr, y,      "J1", jogJ1m, jogJ1p, 0);
  makeJogRow(scr, y+26,   "J2", jogJ2m, jogJ2p, 1);
  makeJogRow(scr, y+52,   "J3", jogJ3m, jogJ3p, 2);
  makeJogRow(scr, y+78,   "J4", jogJ4m, jogJ4p, 3);
  makeJogRow(scr, y+104,  "J5", jogJ5m, jogJ5p, 4);
  makeJogRow(scr, y+130,  "J6", jogJ6m, jogJ6p, 5);

  // Delta adjust
  y = 190;
  lv_obj_t* deltaLabel = ui_label(scr, 12, y, 144, UI_WHITE, UI_F10);
  lv_label_set_text(deltaLabel, "Jog delta");

  lv_obj_t* btnDown = lv_btn_create(scr);
  lv_obj_set_size(btnDown, 24, 20);
  lv_obj_set_pos(btnDown, 12, y+16);
  lv_obj_set_style_bg_color(btnDown, lv_color_hex(UI_CARD), 0);
  lv_obj_set_style_radius(btnDown, 4, 0);
  lv_obj_set_style_border_width(btnDown, 0, 0);
  lv_obj_set_style_shadow_width(btnDown, 0, 0);
  lv_obj_add_event_cb(btnDown, onDeltaDown, LV_EVENT_CLICKED, NULL);
  lv_obj_t* td = lv_label_create(btnDown);
  lv_label_set_text(td, "-");
  lv_obj_center(td);

  static lv_obj_t* deltaVal;
  deltaVal = ui_label(scr, 42, y+16, 60, UI_AMBER, UI_F16);
  lv_label_set_text_fmt(deltaVal, "%.0f deg", jogDelta);

  lv_obj_t* btnUp = lv_btn_create(scr);
  lv_obj_set_size(btnUp, 24, 20);
  lv_obj_set_pos(btnUp, 72, y+16);
  lv_obj_set_style_bg_color(btnUp, lv_color_hex(UI_CARD), 0);
  lv_obj_set_style_radius(btnUp, 4, 0);
  lv_obj_set_style_border_width(btnUp, 0, 0);
  lv_obj_set_style_shadow_width(btnUp, 0, 0);
  lv_obj_add_event_cb(btnUp, onDeltaUp, LV_EVENT_CLICKED, NULL);
  lv_obj_t* tu = lv_label_create(btnUp);
  lv_label_set_text(tu, "+");
  lv_obj_center(tu);

  y = 232;

  // Self-test button
  lv_obj_t* btnST = lv_btn_create(scr);
  lv_obj_set_size(btnST, 100, 32);
  lv_obj_set_pos(btnST, 12, y);
  lv_obj_set_style_bg_color(btnST, lv_color_hex(0x663300), 0);
  lv_obj_set_style_radius(btnST, 6, 0);
  lv_obj_set_style_border_width(btnST, 0, 0);
  lv_obj_set_style_shadow_width(btnST, 0, 0);
  lv_obj_add_event_cb(btnST, onSelfTest, LV_EVENT_CLICKED, NULL);
  lv_obj_t* stLabel = lv_label_create(btnST);
  lv_label_set_text(stLabel, "Self-Test");
  lv_obj_set_style_text_color(stLabel, lv_color_hex(UI_AMBER), 0);
  lv_obj_center(stLabel);

  // Servo Start button
  lv_obj_t* btnSS = lv_btn_create(scr);
  lv_obj_set_size(btnSS, 100, 32);
  lv_obj_set_pos(btnSS, 126, y);
  lv_obj_set_style_bg_color(btnSS, lv_color_hex(0x003300), 0);
  lv_obj_set_style_radius(btnSS, 6, 0);
  lv_obj_set_style_border_width(btnSS, 0, 0);
  lv_obj_set_style_shadow_width(btnSS, 0, 0);
  lv_obj_add_event_cb(btnSS, onServoStart, LV_EVENT_CLICKED, NULL);
  stLabel = lv_label_create(btnSS);
  lv_label_set_text(stLabel, "Servo Start");
  lv_obj_set_style_text_color(stLabel, lv_color_hex(UI_GREEN), 0);
  lv_obj_center(stLabel);

  y += 40;

  // Servo End button
  lv_obj_t* btnSE = lv_btn_create(scr);
  lv_obj_set_size(btnSE, 100, 32);
  lv_obj_set_pos(btnSE, 12, y);
  lv_obj_set_style_bg_color(btnSE, lv_color_hex(0x330000), 0);
  lv_obj_set_style_radius(btnSE, 6, 0);
  lv_obj_set_style_border_width(btnSE, 0, 0);
  lv_obj_set_style_shadow_width(btnSE, 0, 0);
  lv_obj_add_event_cb(btnSE, onServoEnd, LV_EVENT_CLICKED, NULL);
  stLabel = lv_label_create(btnSE);
  lv_label_set_text(stLabel, "Servo End");
  lv_obj_set_style_text_color(stLabel, lv_color_hex(UI_RED), 0);
  lv_obj_center(stLabel);

  // Timing test
  lv_obj_t* btnTT = lv_btn_create(scr);
  lv_obj_set_size(btnTT, 100, 32);
  lv_obj_set_pos(btnTT, 126, y);
  lv_obj_set_style_bg_color(btnTT, lv_color_hex(UI_CARD), 0);
  lv_obj_set_style_radius(btnTT, 6, 0);
  lv_obj_set_style_border_width(btnTT, 0, 0);
  lv_obj_set_style_shadow_width(btnTT, 0, 0);
  lv_obj_add_event_cb(btnTT, onTimingTest, LV_EVENT_CLICKED, NULL);
  stLabel = lv_label_create(btnTT);
  lv_label_set_text(stLabel, "Timing Test");
  lv_obj_center(stLabel);

  y += 40;
  w_testStatus = ui_label(scr, 12, y, 214, UI_DIM, UI_F10);

  // Make sure the content area is tall enough for scrolling
  lv_obj_set_height(scr, 300);

  ui_add_page_dots(scr, 1);
  return scr;
}
