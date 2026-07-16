// ui_about.cpp - Tab 4: About page
#include "ui/ui_about.h"
#include "ui/ui_core.h"
#include "wifi_manager.h"
#include "cnde_client.h"
#include "config.h"
#include <WiFi.h>

extern CNDEClient g_cnde;

static lv_obj_t *scr;
static lv_obj_t *w_connDetails;

void ui_about_refresh() {
    if (!scr || !w_connDetails) return;

    const char* wifiStatus;
    WifiMgrState st = wifiMgrState();
    switch (st) {
        case WM_OK:           wifiStatus = "Connected"; break;
        case WM_CONNECTING:
        case WM_AUTO_CONNECT: wifiStatus = "Connecting..."; break;
        case WM_FAIL:         wifiStatus = "Failed"; break;
        default:              wifiStatus = "Idle"; break;
    }

    const RobotStateData& rs = g_cnde.getState();
    bool cndeOk = g_cnde.isConnected();

    ui_label_setf(w_connDetails,
        LV_SYMBOL_WIFI " WiFi:    %s  (%s)\n"
        LV_SYMBOL_WIFI " IP:      %s\n"
        LV_SYMBOL_CHARGE " CNDE:    %s  (%s)\n"
        LV_SYMBOL_CHARGE " Robot:   %s  [%d/%d]\n"
        LV_SYMBOL_CHARGE " CMD UDP: port 20008\n"
        LV_SYMBOL_CHARGE " Fairino: UDP %s:20007",
        wifiStatus, WiFi.SSID().c_str(),
        wifiMgrConnected() ? wifiMgrLocalIP().c_str() : "---",
        cndeOk ? "Connected" : "Disconnected",
        cndeOk ? "streaming" : "---",
        rs.valid ? (rs.robotState == 1 ? "RUNNING" : "IDLE") : "???",
        rs.valid ? rs.mainCode : 0, rs.valid ? rs.subCode : 0,
        ROBOT_IP);
}

lv_obj_t* ui_about_create(lv_obj_t* parent) {
    scr = lv_obj_create(parent);
    lv_obj_set_size(scr, UI_W, UI_H - UI_TAB_H);
    lv_obj_set_pos(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    lv_obj_t* title = ui_label(scr, 0, 4, UI_W, UI_GREY, UI_F32);
    lv_label_set_text(title, LV_SYMBOL_FILE "  About");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    int lw = UI_W - 40;

    // Header card
    lv_obj_t* hdrCard = ui_card(scr, 20, 55, lw, 260, UI_CARD);
    lv_obj_t* hdrTitle = ui_label(hdrCard, 0, 12, lw - 24, UI_CYAN, UI_F48);
    lv_label_set_text(hdrTitle, "Fairino Robot");
    lv_obj_set_style_text_align(hdrTitle, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* hdrVer = ui_label(hdrCard, 0, 72, lw - 24, UI_WHITE, UI_F28);
    lv_label_set_text(hdrVer, "Controller v2.0");
    lv_obj_set_style_text_align(hdrVer, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* hdrBoard = ui_label(hdrCard, 0, 112, lw - 24, UI_DIM, UI_F20);
    lv_label_set_text(hdrBoard, "ESP32-P4NRW32 + C6-MINI-1U\nJC8012P4A1 Board");
    lv_obj_set_style_text_align(hdrBoard, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* hdrDisp = ui_label(hdrCard, 0, 162, lw - 24, UI_DIM, UI_F18);
    lv_label_set_text(hdrDisp, "10.1\" IPS 800x1280  |  JD9365DA MIPI DSI\nGSL3680 Touch  |  LVGL v8.4");
    lv_obj_set_style_text_align(hdrDisp, LV_TEXT_ALIGN_CENTER, 0);

    // Connection details card
    lv_obj_t* connCard = ui_card(scr, 20, 340, lw, 280, UI_CARD);
    lv_obj_t* connTitle = ui_label(connCard, 0, 0, lw - 24, UI_GREY, UI_F24);
    lv_label_set_text(connTitle, "Connection Status");

    w_connDetails = ui_label(connCard, 0, 36, lw - 24, UI_WHITE, UI_F20);
    lv_label_set_text(w_connDetails, "Loading...");

    // Hardware specs card
    lv_obj_t* hwCard = ui_card(scr, 20, 645, lw, 200, UI_CARD);
    lv_obj_t* hwTitle = ui_label(hwCard, 0, 0, lw - 24, UI_GREY, UI_F24);
    lv_label_set_text(hwTitle, "Hardware");

    lv_obj_t* hwInfo = ui_label(hwCard, 0, 36, lw - 24, UI_WHITE, UI_F20);
    lv_label_set_text(hwInfo,
        "SRAM: 768KB HP  |  PSRAM: 32MB\n"
        "Flash: 16MB  |  WiFi: C6-MINI-1U\n"
        "Display: JD9365DA 2-lane MIPI DSI\n"
        "Touch: GSL3680 I2C");

    // Footer
    lv_obj_t* footLabel = ui_label(scr, 0, 870, UI_W, UI_DIM, UI_F18);
    lv_label_set_text(footLabel, "UIEAclub  |  ThirdHand Project  |  2026");
    lv_obj_set_style_text_align(footLabel, LV_TEXT_ALIGN_CENTER, 0);

    return scr;
}
