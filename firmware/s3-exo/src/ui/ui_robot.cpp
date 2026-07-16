#include "ui/ui_robot.h"

#include "config.h"
#include "robot/cnde_client.h"
#include "robot/fairino_udp.h"
#include "ui/ui_core.h"
#include "wifi_manager.h"

static lv_obj_t* s_screen = nullptr;
static lv_obj_t* s_jointValues[6] = {nullptr};
static lv_obj_t* s_deltaValue = nullptr;
static lv_obj_t* s_status = nullptr;
static float s_jogDelta = 5.0f;

struct JogAction {
  uint8_t axis;
  int8_t direction;
};

static JogAction s_jogActions[12];

static void jogJoint(uint8_t axis, float delta) {
  if (!wifiMgrConnected()) return;
  const RobotStateData state = g_cnde.getState();
  if (!state.valid) return;

  float joints[6];
  for (int i = 0; i < 6; i++) joints[i] = state.jointPos[i];
  joints[axis] += delta;
  safeMotionSetTarget(joints, false);
}

static void onJog(lv_event_t* event) {
  if (!ui_event_is_tap(event)) return;
  const JogAction* action = static_cast<const JogAction*>(lv_event_get_user_data(event));
  if (!action) return;
  jogJoint(action->axis, action->direction * s_jogDelta);
}

static void onSelfTest(lv_event_t* event) {
  if (!ui_event_is_tap(event) || !wifiMgrConnected()) return;
  selfTestStart();
  lv_label_set_text(s_status, "Self-test requested");
}

static void onServoStart(lv_event_t* event) {
  if (!ui_event_is_tap(event) || !wifiMgrConnected()) return;
  lv_label_set_text(s_status,
                    safeMotionStartHold() ? "Servo hold active" : "Robot data unavailable");
}

static void onServoEnd(lv_event_t* event) {
  if (!ui_event_is_tap(event)) return;
  safeMotionStop();
  lv_label_set_text(s_status, "Servo stopped");
}

static void onTimingTest(lv_event_t* event) {
  if (!ui_event_is_tap(event) || !wifiMgrConnected()) return;
  g_fairino.servoTimingTest();
  lv_label_set_text(s_status, "Timing test sent");
}

static void updateDeltaLabel() {
  if (s_deltaValue) lv_label_set_text_fmt(s_deltaValue, "%.0f deg", s_jogDelta);
}

static void onDeltaDown(lv_event_t* event) {
  if (!ui_event_is_tap(event)) return;
  if (s_jogDelta > 1.0f) s_jogDelta -= 1.0f;
  updateDeltaLabel();
}

static void onDeltaUp(lv_event_t* event) {
  if (!ui_event_is_tap(event)) return;
  if (s_jogDelta < 20.0f) s_jogDelta += 1.0f;
  updateDeltaLabel();
}

static lv_obj_t* makeButton(lv_obj_t* parent, int x, int y, int w, int h,
                            const char* text, uint32_t color,
                            lv_event_cb_t callback, void* userData = nullptr) {
  lv_obj_t* button = lv_btn_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, w, h);
  lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
  lv_obj_set_style_radius(button, 7, 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_add_flag(button, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, userData);

  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(UI_WHITE), 0);
  lv_obj_set_style_text_font(label, UI_F10, 0);
  lv_obj_center(label);
  return button;
}

static void makeJogRow(lv_obj_t* parent, int y, int axis) {
  lv_obj_t* card = ui_card(parent, 8, y, 224, 34, UI_CARD);
  lv_obj_t* name = ui_label(card, 6, 8, 24, UI_GREY, UI_F10);
  lv_label_set_text_fmt(name, "J%d", axis + 1);

  s_jointValues[axis] = ui_label(card, 32, 7, 72, UI_WHITE, UI_F12);
  lv_label_set_text(s_jointValues[axis], "--.-");
  lv_obj_set_style_text_align(s_jointValues[axis], LV_TEXT_ALIGN_RIGHT, 0);

  JogAction* minus = &s_jogActions[axis * 2];
  minus->axis = axis;
  minus->direction = -1;
  JogAction* plus = &s_jogActions[axis * 2 + 1];
  plus->axis = axis;
  plus->direction = 1;

  makeButton(card, 112, 4, 46, 26, "-", 0x3800, onJog, minus);
  makeButton(card, 166, 4, 46, 26, "+", 0x01E0, onJog, plus);
}

void ui_robot_refresh() {
  if (!s_screen) return;
  const RobotStateData state = g_cnde.getState();
  for (int i = 0; i < 6; i++) {
    if (state.valid) ui_label_setf(s_jointValues[i], "%.1f", state.jointPos[i]);
    else lv_label_set_text(s_jointValues[i], "--.-");
  }
}

lv_obj_t* ui_robot_create() {
  s_screen = ui_screen_create();

  lv_obj_t* title = ui_label(s_screen, 12, 4, 216, UI_GREY, UI_F12);
  lv_label_set_text(title, "ROBOT CONTROL");

  lv_obj_t* body = ui_page_content(s_screen, true);
  for (int i = 0; i < 6; i++) makeJogRow(body, 2 + i * 38, i);

  lv_obj_t* delta = ui_card(body, 8, 234, 224, 44, 0x1082);
  lv_obj_t* deltaTitle = ui_label(delta, 8, 4, 92, UI_GREY, UI_F10);
  lv_label_set_text(deltaTitle, "JOG STEP");
  s_deltaValue = ui_label(delta, 8, 21, 92, UI_AMBER, UI_F12);
  updateDeltaLabel();
  makeButton(delta, 112, 9, 46, 28, "-", UI_CARD, onDeltaDown);
  makeButton(delta, 166, 9, 46, 28, "+", UI_CARD, onDeltaUp);

  lv_obj_t* actionsTitle = ui_label(body, 8, 290, 224, UI_DIM, UI_F10);
  lv_label_set_text(actionsTitle, "ACTIONS - TAP ONLY");
  makeButton(body, 8, 309, 108, 36, "Self-Test", 0x6B20, onSelfTest);
  makeButton(body, 124, 309, 108, 36, "Servo Start", 0x01E0, onServoStart);
  makeButton(body, 8, 353, 108, 36, "Servo End", 0x3800, onServoEnd);
  makeButton(body, 124, 353, 108, 36, "Timing Test", UI_CARD, onTimingTest);

  s_status = ui_label(body, 8, 399, 224, UI_DIM, UI_F10);
  lv_label_set_text(s_status, "Swipe vertically for actions");

  ui_add_page_dots(s_screen, 1);
  return s_screen;
}
