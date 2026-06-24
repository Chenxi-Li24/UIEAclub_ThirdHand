// ui_dashboard.cpp - Tab 0: Dashboard - 6-axis arc gauges + state + estop
#include "ui/ui_dashboard.h"
#include "ui/ui_core.h"
#include "cnde_client.h"
#include "wifi_manager.h"
#include "config.h"
#include "cmd_queue.h"
#include <WiFi.h>

extern CNDEClient g_cnde;
extern HeartbeatMonitor g_heartbeat;
extern RobotStateMachine g_state;
extern CmdQueue g_cmdQueue;

static lv_obj_t *scr;
static lv_obj_t *arcJ[6], *lblJ[6];
static lv_obj_t *lblRobotState, *cndeDot, *hbDot;
static lv_obj_t *lblSsid, *lblIp, *lblRssi, *lblUptime;
static lv_obj_t *selfTestBar, *selfTestLabel;
static lv_obj_t *btnEstop;
static lv_obj_t *lblState;

static void onEstopBtn(lv_event_t* e) {
    CmdEntry ce;
    ce.type = CMD_ESTOP;
    ce.ts = millis();
    g_cmdQueue.enqueue(ce);
    Serial.println("[UI-DASH] E-STOP button pressed");
}

void ui_dashboard_refresh() {
    if (!scr) return;
    const RobotStateData& rs = g_cnde.getState();

    // Joint arcs + labels
    for (int i = 0; i < 6; i++) {
        if (rs.valid) {
            int val = (int)rs.jointPos[i];
            int arcVal = (val + 180) * 10;
            if (arcVal < 0) arcVal = 0;
            if (arcVal > 3600) arcVal = 3600;
            lv_arc_set_value(arcJ[i], arcVal);
            ui_label_setf(lblJ[i], "J%d %.1f\xC2\xB0", i+1, rs.jointPos[i]);
        } else {
            lv_arc_set_value(arcJ[i], 1800);
            ui_label_setf(lblJ[i], "J%d ---.-", i+1);
        }
    }

    // Robot state machine
    uint32_t stColor = UI_GREEN;
    switch (g_state.state()) {
        case RSTATE_IDLE:   stColor = UI_GREEN;  break;
        case RSTATE_MOVING: stColor = UI_CYAN;   break;
        case RSTATE_ESTOP:  stColor = UI_RED;    break;
        case RSTATE_ERROR:  stColor = UI_RED;    break;
        case RSTATE_LOCKED: stColor = UI_AMBER;  break;
    }
    ui_label_setf(lblState, "%s", g_state.stateName());
    lv_obj_set_style_text_color(lblState, lv_color_hex(stColor), 0);
    // Blink E-STOP label
    if (g_state.state() == RSTATE_ESTOP && (millis() / 400) % 2) {
        lv_obj_set_style_text_color(lblState, lv_color_hex(UI_WHITE), 0);
    }

    // Robot CNDE state
    if (rs.valid) {
        const char* stText = rs.robotState == 1 ? "RUNNING" : "IDLE";
        uint32_t cl = rs.robotState == 1 ? UI_GREEN : UI_GREY;
        lv_label_set_text(lblRobotState, stText);
        lv_obj_set_style_text_color(lblRobotState, lv_color_hex(cl), 0);
    } else {
        lv_label_set_text(lblRobotState, "NO DATA");
        lv_obj_set_style_text_color(lblRobotState, lv_color_hex(UI_RED), 0);
    }

    // CNDE dot
    uint32_t cd = g_cnde.isConnected() ? UI_GREEN : UI_DIM;
    lv_obj_set_style_bg_color(cndeDot, lv_color_hex(cd), 0);

    // Heartbeat dot: green=alive, red=timeout
    uint32_t hb = g_heartbeat.isTimeout() ? UI_RED :
                  (g_heartbeat.isActive() ? UI_GREEN : UI_DIM);
    lv_obj_set_style_bg_color(hbDot, lv_color_hex(hb), 0);

    // E-STOP button: show red when estop, hidden otherwise
    if (g_state.state() == RSTATE_ESTOP) {
        lv_obj_clear_flag(btnEstop, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(btnEstop, lv_color_hex(UI_RED), 0);
    } else {
        lv_obj_add_flag(btnEstop, LV_OBJ_FLAG_HIDDEN);
    }

    // WiFi
    WifiMgrState st = wifiMgrState();
    if (st == WM_OK) {
        ui_label_setf(lblSsid, "%s", WiFi.SSID().c_str());
        ui_label_setf(lblIp, "%s  RSSI:%ddBm", wifiMgrLocalIP().c_str(), wifiMgrRssi());
    } else {
        lv_label_set_text(lblSsid, st == WM_CONNECTING ? "Connecting..." : "OFFLINE");
        lv_label_set_text(lblIp, "");
        lv_label_set_text(lblRssi, "");
    }

    // Uptime
    uint32_t up = millis() / 1000;
    ui_label_setf(lblUptime, "UPTIME  %uh %um", up / 3600, (up % 3600) / 60);
}

// Helper: create one arc + label
static void makeArc(lv_obj_t* parent, int col, int row, const char* name,
                     lv_obj_t** arcOut, lv_obj_t** lblOut) {
    int x = 14 + col * 264;
    int y = 48 + row * 410;
    int arcW = 250, arcH = 360;

    *arcOut = lv_arc_create(parent);
    lv_obj_set_size(*arcOut, arcW, arcH);
    lv_obj_set_pos(*arcOut, x, y);
    lv_obj_remove_style(*arcOut, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(*arcOut, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_width(*arcOut, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(*arcOut, lv_color_hex(0x2104), LV_PART_MAIN);
    lv_obj_set_style_arc_color(*arcOut, lv_color_hex(UI_CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(*arcOut, lv_color_hex(UI_BG), 0);
    lv_arc_set_range(*arcOut, 0, 3600);
    lv_arc_set_value(*arcOut, 1800);
    lv_arc_set_angles(*arcOut, 135, 405);
    lv_arc_set_mode(*arcOut, LV_ARC_MODE_NORMAL);

    *lblOut = lv_label_create(*arcOut);
    lv_label_set_text(*lblOut, name);
    lv_obj_set_style_text_color(*lblOut, lv_color_hex(UI_WHITE), 0);
    lv_obj_set_style_text_font(*lblOut, UI_F32, 0);
    lv_obj_center(*lblOut);
}

lv_obj_t* ui_dashboard_create(lv_obj_t* parent) {
    scr = lv_obj_create(parent);
    lv_obj_set_size(scr, UI_W, UI_H - UI_TAB_H);
    lv_obj_set_pos(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Title bar
    lv_obj_t* title = ui_label(scr, 0, 4, UI_W, UI_CYAN, UI_F32);
    lv_label_set_text(title, LV_SYMBOL_HOME "  Fairino Robot");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    // CNDE status dot (top right)
    cndeDot = lv_obj_create(scr);
    lv_obj_set_size(cndeDot, 12, 12);
    lv_obj_set_pos(cndeDot, UI_W - 30, 14);
    lv_obj_set_style_bg_color(cndeDot, lv_color_hex(UI_DIM), 0);
    lv_obj_set_style_radius(cndeDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(cndeDot, 0, 0);

    // Heartbeat dot (next to CNDE dot)
    hbDot = lv_obj_create(scr);
    lv_obj_set_size(hbDot, 12, 12);
    lv_obj_set_pos(hbDot, UI_W - 55, 14);
    lv_obj_set_style_bg_color(hbDot, lv_color_hex(UI_DIM), 0);
    lv_obj_set_style_radius(hbDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(hbDot, 0, 0);

    // 6-axis arc gauges (3 cols x 2 rows)
    makeArc(scr, 0, 0, "J1 --.-", &arcJ[0], &lblJ[0]);
    makeArc(scr, 1, 0, "J2 --.-", &arcJ[1], &lblJ[1]);
    makeArc(scr, 2, 0, "J3 --.-", &arcJ[2], &lblJ[2]);
    makeArc(scr, 0, 1, "J4 --.-", &arcJ[3], &lblJ[3]);
    makeArc(scr, 1, 1, "J5 --.-", &arcJ[4], &lblJ[4]);
    makeArc(scr, 2, 1, "J6 --.-", &arcJ[5], &lblJ[5]);

    // Bottom section
    int yBot = 870;

    // Self-test progress bar
    selfTestBar = lv_bar_create(scr);
    lv_obj_set_size(selfTestBar, UI_W - 40, 10);
    lv_obj_set_pos(selfTestBar, 20, yBot);
    lv_obj_set_style_bg_color(selfTestBar, lv_color_hex(0x2104), LV_PART_MAIN);
    lv_obj_set_style_bg_color(selfTestBar, lv_color_hex(UI_AMBER), LV_PART_INDICATOR);
    lv_obj_set_style_radius(selfTestBar, 5, LV_PART_MAIN);
    lv_obj_set_style_radius(selfTestBar, 5, LV_PART_INDICATOR);
    lv_bar_set_range(selfTestBar, 0, 100);
    lv_bar_set_value(selfTestBar, 0, LV_ANIM_OFF);
    lv_obj_add_flag(selfTestBar, LV_OBJ_FLAG_HIDDEN);

    selfTestLabel = ui_label(scr, 20, yBot + 16, UI_W - 40, UI_AMBER, UI_F16);
    lv_obj_add_flag(selfTestLabel, LV_OBJ_FLAG_HIDDEN);

    // State label (centered, large)
    yBot = 910;
    lblState = ui_label(scr, 0, yBot, UI_W, UI_GREEN, UI_F48);
    lv_label_set_text(lblState, "IDLE");
    lv_obj_set_style_text_align(lblState, LV_TEXT_ALIGN_CENTER, 0);

    // CNDE robot state label
    lblRobotState = ui_label(scr, 0, yBot + 50, UI_W, UI_WHITE, UI_F32);
    lv_label_set_text(lblRobotState, "NO DATA");
    lv_obj_set_style_text_align(lblRobotState, LV_TEXT_ALIGN_CENTER, 0);

    // E-STOP button (full-width, red, hidden unless estop state)
    btnEstop = lv_btn_create(scr);
    lv_obj_set_size(btnEstop, UI_W - 40, 64);
    lv_obj_set_pos(btnEstop, 20, yBot + 100);
    lv_obj_set_style_bg_color(btnEstop, lv_color_hex(UI_RED), 0);
    lv_obj_set_style_radius(btnEstop, 12, 0);
    lv_obj_set_style_border_width(btnEstop, 0, 0);
    lv_obj_add_event_cb(btnEstop, onEstopBtn, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbE = lv_label_create(btnEstop);
    lv_label_set_text(lbE, LV_SYMBOL_STOP "  E-STOP");
    lv_obj_set_style_text_font(lbE, UI_F28, 0);
    lv_obj_center(lbE);
    lv_obj_add_flag(btnEstop, LV_OBJ_FLAG_HIDDEN);

    // WiFi info bar
    int yWiFi = yBot + 180;
    lblSsid = ui_label(scr, 20, yWiFi, UI_W - 40, UI_WHITE, UI_F20);
    lblIp = ui_label(scr, 20, yWiFi + 28, UI_W - 40, UI_DIM, UI_F16);
    lblRssi = ui_label(scr, 20, yWiFi + 52, UI_W - 40, UI_DIM, UI_F16);

    // Uptime
    lblUptime = ui_label(scr, 0, 1060, UI_W, UI_GREY, UI_F24);
    lv_label_set_text(lblUptime, "UPTIME 0h 0m");
    lv_obj_set_style_text_align(lblUptime, LV_TEXT_ALIGN_CENTER, 0);

    return scr;
}
