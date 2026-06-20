// ui_control.cpp - Tab 1: Robot Jog Control + Self-Test buttons
#include <Arduino.h>
#include "ui/ui_control.h"
#include "ui/ui_core.h"
#include "wifi_manager.h"
#include "fairino_udp.h"
#include "cnde_client.h"
#include "config.h"

static lv_obj_t *scr;
static lv_obj_t *w_jogLabels[6];
static lv_obj_t *deltaLabel;
static float jogDelta = 5.0f;

extern CNDEClient g_cnde;
extern FairinoUDPClient g_fairino;
extern void selfTestStart();

static void jogJoint(int axis, float delta) {
    if (!wifiMgrConnected()) return;
    const RobotStateData& rs = g_cnde.getState();
    float joints[6] = {0};
    if (rs.valid) for (int i = 0; i < 6; i++) joints[i] = rs.jointPos[i];
    joints[axis] += delta;
    g_fairino.servoJ(joints[0], joints[1], joints[2], joints[3], joints[4], joints[5],
                     SELF_TEST_ACC, SELF_TEST_VEL, SELF_TEST_CMDT, 0, 0);
}

static void jogJ0_plus(lv_event_t* e)  { jogJoint(0,  jogDelta); }
static void jogJ0_minus(lv_event_t* e) { jogJoint(0, -jogDelta); }
static void jogJ1_plus(lv_event_t* e)  { jogJoint(1,  jogDelta); }
static void jogJ1_minus(lv_event_t* e) { jogJoint(1, -jogDelta); }
static void jogJ2_plus(lv_event_t* e)  { jogJoint(2,  jogDelta); }
static void jogJ2_minus(lv_event_t* e) { jogJoint(2, -jogDelta); }
static void jogJ3_plus(lv_event_t* e)  { jogJoint(3,  jogDelta); }
static void jogJ3_minus(lv_event_t* e) { jogJoint(3, -jogDelta); }
static void jogJ4_plus(lv_event_t* e)  { jogJoint(4,  jogDelta); }
static void jogJ4_minus(lv_event_t* e) { jogJoint(4, -jogDelta); }
static void jogJ5_plus(lv_event_t* e)  { jogJoint(5,  jogDelta); }
static void jogJ5_minus(lv_event_t* e) { jogJoint(5, -jogDelta); }

static void onSelfTest(lv_event_t* e) { if (wifiMgrConnected()) selfTestStart(); }
static void onServoStart(lv_event_t* e) { if (wifiMgrConnected()) g_fairino.servoMoveStart(); }
static void onServoEnd(lv_event_t* e) { g_fairino.servoMoveEnd(); }
static void onTimingTest(lv_event_t* e) { if (wifiMgrConnected()) g_fairino.servoTimingTest(); }

static void onDeltaUp(lv_event_t* e)   { if (jogDelta < 20.0f) jogDelta += 1.0f; }
static void onDeltaDown(lv_event_t* e) { if (jogDelta > 1.0f) jogDelta -= 1.0f; }

static lv_event_cb_t JOG_CALLBACKS[6][2] = {
    {jogJ0_plus, jogJ0_minus},
    {jogJ1_plus, jogJ1_minus},
    {jogJ2_plus, jogJ2_minus},
    {jogJ3_plus, jogJ3_minus},
    {jogJ4_plus, jogJ4_minus},
    {jogJ5_plus, jogJ5_minus},
};

void ui_control_refresh() {
    if (!scr) return;
    const RobotStateData& rs = g_cnde.getState();
    for (int i = 0; i < 6; i++) {
        if (rs.valid) {
            ui_label_setf(w_jogLabels[i], "J%d: %.1f deg", i+1, rs.jointPos[i]);
        } else {
            ui_label_setf(w_jogLabels[i], "J%d: ---", i+1);
        }
    }
    if (deltaLabel) ui_label_setf(deltaLabel, "Step: %.0f deg", jogDelta);
}

lv_obj_t* ui_control_create(lv_obj_t* parent) {
    scr = lv_obj_create(parent);
    lv_obj_set_size(scr, UI_W, UI_H - UI_TAB_H);
    lv_obj_set_pos(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_scroll_dir(scr, LV_DIR_VER);

    lv_obj_t* title = ui_label(scr, 0, 4, UI_W, UI_GREY, UI_F32);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS "  Control");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    int colW = 260; int colGap = 10; int btnW = 68; int btnH = 52;
    int startX = 10; int startY = 50; int rowH = 64;

    for (int i = 0; i < 6; i++) {
        int col = i % 3; int row = i / 3;
        int cx = startX + col * (colW + colGap);
        int cy = startY + row * rowH * 2;

        lv_obj_t* lbl = ui_label(scr, cx, cy, colW - 10, UI_CYAN, UI_F24);
        ui_label_setf(lbl, "J%d", i + 1);

        w_jogLabels[i] = ui_label(scr, cx, cy + 30, colW - 10, UI_WHITE, UI_F20);
        lv_label_set_text(w_jogLabels[i], "---");

        lv_obj_t* btnMinus = lv_btn_create(scr);
        lv_obj_set_size(btnMinus, btnW, btnH);
        lv_obj_set_pos(btnMinus, cx, cy + 60);
        lv_obj_set_style_bg_color(btnMinus, lv_color_hex(UI_CARD), 0);
        lv_obj_set_style_radius(btnMinus, 10, 0);
        lv_obj_set_style_border_width(btnMinus, 0, 0);
        lv_obj_add_event_cb(btnMinus, JOG_CALLBACKS[i][1], LV_EVENT_CLICKED, NULL);
        lv_obj_t* bl = lv_label_create(btnMinus);
        lv_label_set_text(bl, LV_SYMBOL_MINUS);
        lv_obj_set_style_text_font(bl, UI_F20, 0);
        lv_obj_center(bl);

        lv_obj_t* btnPlus = lv_btn_create(scr);
        lv_obj_set_size(btnPlus, btnW, btnH);
        lv_obj_set_pos(btnPlus, cx + btnW + 8, cy + 60);
        lv_obj_set_style_bg_color(btnPlus, lv_color_hex(UI_CARD), 0);
        lv_obj_set_style_radius(btnPlus, 10, 0);
        lv_obj_set_style_border_width(btnPlus, 0, 0);
        lv_obj_add_event_cb(btnPlus, JOG_CALLBACKS[i][0], LV_EVENT_CLICKED, NULL);
        lv_obj_t* pl = lv_label_create(btnPlus);
        lv_label_set_text(pl, LV_SYMBOL_PLUS);
        lv_obj_set_style_text_font(pl, UI_F20, 0);
        lv_obj_center(pl);
    }

    int yDelta = startY + 4 * rowH + 16;
    deltaLabel = ui_label(scr, startX + 10, yDelta, 240, UI_WHITE, UI_F20);
    ui_label_setf(deltaLabel, "Step: %.0f deg", jogDelta);

    lv_obj_t* btnDn = lv_btn_create(scr);
    lv_obj_set_size(btnDn, 60, 48);
    lv_obj_set_pos(btnDn, startX + 260, yDelta - 4);
    lv_obj_set_style_bg_color(btnDn, lv_color_hex(UI_CARD), 0);
    lv_obj_set_style_radius(btnDn, 10, 0);
    lv_obj_set_style_border_width(btnDn, 0, 0);
    lv_obj_add_event_cb(btnDn, onDeltaDown, LV_EVENT_CLICKED, NULL);
    lv_obj_t* dl = lv_label_create(btnDn);
    lv_label_set_text(dl, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(dl, UI_F20, 0);
    lv_obj_center(dl);

    lv_obj_t* btnUp = lv_btn_create(scr);
    lv_obj_set_size(btnUp, 60, 48);
    lv_obj_set_pos(btnUp, startX + 326, yDelta - 4);
    lv_obj_set_style_bg_color(btnUp, lv_color_hex(UI_CARD), 0);
    lv_obj_set_style_radius(btnUp, 10, 0);
    lv_obj_set_style_border_width(btnUp, 0, 0);
    lv_obj_add_event_cb(btnUp, onDeltaUp, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ul = lv_label_create(btnUp);
    lv_label_set_text(ul, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(ul, UI_F20, 0);
    lv_obj_center(ul);

    int btnY = yDelta + 60;
    int btnW2 = 240; int btnH2 = 80;
    int bGap = 16;

    lv_obj_t* btnST = lv_btn_create(scr);
    lv_obj_set_size(btnST, btnW2, btnH2);
    lv_obj_set_pos(btnST, startX, btnY);
    lv_obj_set_style_bg_color(btnST, lv_color_hex(0x7E40), 0);
    lv_obj_set_style_radius(btnST, 10, 0);
    lv_obj_set_style_border_width(btnST, 0, 0);
    lv_obj_add_event_cb(btnST, onSelfTest, LV_EVENT_CLICKED, NULL);
    lv_obj_t* tST = lv_label_create(btnST);
    lv_label_set_text(tST, "Self-Test");
    lv_obj_set_style_text_font(tST, UI_F24, 0);
    lv_obj_center(tST);

    lv_obj_t* btnSS = lv_btn_create(scr);
    lv_obj_set_size(btnSS, btnW2, btnH2);
    lv_obj_set_pos(btnSS, startX + btnW2 + bGap, btnY);
    lv_obj_set_style_bg_color(btnSS, lv_color_hex(0x0300), 0);
    lv_obj_set_style_radius(btnSS, 12, 0);
    lv_obj_set_style_border_width(btnSS, 0, 0);
    lv_obj_add_event_cb(btnSS, onServoStart, LV_EVENT_CLICKED, NULL);
    lv_obj_t* tSS = lv_label_create(btnSS);
    lv_label_set_text(tSS, "Servo Start");
    lv_obj_set_style_text_font(tSS, UI_F24, 0);
    lv_obj_center(tSS);

    lv_obj_t* btnSE = lv_btn_create(scr);
    lv_obj_set_size(btnSE, btnW2, btnH2);
    lv_obj_set_pos(btnSE, startX + (btnW2 + bGap) * 2, btnY);
    lv_obj_set_style_bg_color(btnSE, lv_color_hex(0x7000), 0);
    lv_obj_set_style_radius(btnSE, 12, 0);
    lv_obj_set_style_border_width(btnSE, 0, 0);
    lv_obj_add_event_cb(btnSE, onServoEnd, LV_EVENT_CLICKED, NULL);
    lv_obj_t* tSE = lv_label_create(btnSE);
    lv_label_set_text(tSE, "Servo End");
    lv_obj_set_style_text_font(tSE, UI_F24, 0);
    lv_obj_center(tSE);

    lv_obj_t* btnTT = lv_btn_create(scr);
    lv_obj_set_size(btnTT, btnW2, btnH2);
    lv_obj_set_pos(btnTT, startX, btnY + btnH2 + 16);
    lv_obj_set_style_bg_color(btnTT, lv_color_hex(UI_CARD), 0);
    lv_obj_set_style_radius(btnTT, 12, 0);
    lv_obj_set_style_border_width(btnTT, 0, 0);
    lv_obj_add_event_cb(btnTT, onTimingTest, LV_EVENT_CLICKED, NULL);
    lv_obj_t* tTT = lv_label_create(btnTT);
    lv_label_set_text(tTT, "Timing Test");
    lv_obj_set_style_text_font(tTT, UI_F24, 0);
    lv_obj_center(tTT);

    lv_obj_set_height(scr, 1200);
    return scr;
}
