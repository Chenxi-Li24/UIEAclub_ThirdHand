// ui_system.cpp - Tab 2: System Info
#include "ui/ui_system.h"
#include "ui/ui_core.h"
#include "wifi_manager.h"
#include "cnde_client.h"
#include "config.h"
#include <WiFi.h>

extern CNDEClient g_cnde;

static lv_obj_t *scr;
static lv_obj_t *w_heap, *w_psram, *w_uptime, *w_wifiInfo, *w_connInfo, *w_cndeFrames, *w_errorCode;
static lv_obj_t *bar_heap, *bar_psram;

void ui_system_refresh() {
    if (!scr) return;

    // Heap
    uint32_t freeH = ESP.getFreeHeap();
    uint32_t totalH = ESP.getHeapSize();
    ui_label_setf(w_heap, "SRAM: %u / %u KB free", freeH/1024, totalH/1024);
    if (bar_heap) {
        lv_bar_set_value(bar_heap, (int32_t)(freeH/1024), LV_ANIM_ON);
        lv_bar_set_range(bar_heap, 0, (int32_t)(totalH/1024));
        uint32_t hc = freeH > 200*1024 ? UI_GREEN : freeH > 100*1024 ? UI_AMBER : UI_RED;
        lv_obj_set_style_bg_color(bar_heap, lv_color_hex(hc), LV_PART_INDICATOR);
    }

    // PSRAM
    uint32_t freeP = ESP.getFreePsram();
    uint32_t totalP = ESP.getPsramSize();
    ui_label_setf(w_psram, "PSRAM: %u / %u KB free", freeP/1024, totalP/1024);
    if (bar_psram) {
        lv_bar_set_value(bar_psram, (int32_t)(freeP/1024), LV_ANIM_ON);
        lv_bar_set_range(bar_psram, 0, (int32_t)(totalP/1024));
        uint32_t pc = freeP > 4096*1024 ? UI_GREEN : freeP > 1024*1024 ? UI_AMBER : UI_RED;
        lv_obj_set_style_bg_color(bar_psram, lv_color_hex(pc), LV_PART_INDICATOR);
    }

    // Uptime
    uint32_t up = millis() / 1000;
    ui_label_setf(w_uptime, "Uptime: %uh %um %us", up/3600, (up%3600)/60, up%60);

    // WiFi details
    if (wifiMgrConnected()) {
        ui_label_setf(w_wifiInfo,
            "SSID: %s\nIP: %s\nGW: %s\nMAC: %s\nRSSI: %ddBm  Channel: %d",
            WiFi.SSID().c_str(),
            wifiMgrLocalIP().c_str(),
            WiFi.gatewayIP().toString().c_str(),
            WiFi.macAddress().c_str(),
            wifiMgrRssi(), WiFi.channel());
    } else {
        lv_label_set_text(w_wifiInfo, "WiFi: OFFLINE");
    }

    // Connection status
    ui_label_setf(w_connInfo,
        "Fairino UDP :20007  %s\nCNDE TCP :20005  %s\nCMD UDP :20008  %s",
        wifiMgrConnected() ? "READY" : "---",
        g_cnde.isConnected() ? "CONNECTED" : "---",
        wifiMgrConnected() ? "LISTENING" : "---");

    // CNDE stats
    const RobotStateData& rs = g_cnde.getState();
    if (rs.valid) {
        ui_label_setf(w_cndeFrames, "Last update: %lums ago",
            (unsigned long)(millis() - rs.lastUpdate));
    }

    // Error codes
    if (rs.valid && (rs.mainCode != 0 || rs.subCode != 0)) {
        ui_label_setf(w_errorCode, "Error: main=%ld sub=%ld", rs.mainCode, rs.subCode);
        lv_obj_set_style_text_color(w_errorCode, lv_color_hex(UI_RED), 0);
    } else {
        lv_label_set_text(w_errorCode, "No errors");
        lv_obj_set_style_text_color(w_errorCode, lv_color_hex(UI_GREEN), 0);
    }
}

lv_obj_t* ui_system_create(lv_obj_t* parent) {
    scr = lv_obj_create(parent);
    lv_obj_set_size(scr, UI_W, UI_H - UI_TAB_H);
    lv_obj_set_pos(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_scroll_dir(scr, LV_DIR_VER);

    lv_obj_t* title = ui_label(scr, 0, 4, UI_W, UI_GREY, UI_F32);
    lv_label_set_text(title, LV_SYMBOL_LIST "  System Info");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    int y = 55;
    int lw = UI_W - 40;
    int barH = 16;

    // SRAM
    lv_obj_t* lblSRAM = ui_label(scr, 20, y, 200, UI_GREY, UI_F20);
    lv_label_set_text(lblSRAM, "SRAM");
    bar_heap = lv_bar_create(scr);
    lv_obj_set_size(bar_heap, lw, barH);
    lv_obj_set_pos(bar_heap, 20, y + 28);
    lv_bar_set_range(bar_heap, 0, 512);
    lv_obj_set_style_bg_color(bar_heap, lv_color_hex(0x2104), LV_PART_MAIN);
    lv_obj_set_style_radius(bar_heap, 8, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_heap, 8, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar_heap, lv_color_hex(UI_GREEN), LV_PART_INDICATOR);
    w_heap = ui_label(scr, 20, y + 48, lw, UI_WHITE, UI_F16);

    // PSRAM
    y += 100;
    lv_obj_t* lblPSRAM = ui_label(scr, 20, y, 200, UI_GREY, UI_F20);
    lv_label_set_text(lblPSRAM, "PSRAM");
    bar_psram = lv_bar_create(scr);
    lv_obj_set_size(bar_psram, lw, barH);
    lv_obj_set_pos(bar_psram, 20, y + 28);
    lv_bar_set_range(bar_psram, 0, 32768);
    lv_obj_set_style_bg_color(bar_psram, lv_color_hex(0x2104), LV_PART_MAIN);
    lv_obj_set_style_radius(bar_psram, 8, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_psram, 8, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar_psram, lv_color_hex(UI_GREEN), LV_PART_INDICATOR);
    w_psram = ui_label(scr, 20, y + 48, lw, UI_WHITE, UI_F16);

    // Uptime
    y += 100;
    w_uptime = ui_label(scr, 20, y, lw, UI_WHITE, UI_F28);

    // WiFi info card
    y += 44;
    lv_obj_t* wifiCard = ui_card(scr, 20, y, lw, 220, UI_CARD);
    lv_obj_t* wifiTitle = ui_label(wifiCard, 0, 0, lw - 24, UI_GREY, UI_F20);
    lv_label_set_text(wifiTitle, "WiFi Details");
    w_wifiInfo = ui_label(wifiCard, 0, 32, lw - 24, UI_WHITE, UI_F18);
    lv_label_set_text(w_wifiInfo, "Loading...");

    // Connection status card
    y += 240;
    lv_obj_t* connCard = ui_card(scr, 20, y, lw, 140, UI_CARD);
    lv_obj_t* connTitle = ui_label(connCard, 0, 0, lw - 24, UI_GREY, UI_F20);
    lv_label_set_text(connTitle, "Connections");
    w_connInfo = ui_label(connCard, 0, 32, lw - 24, UI_WHITE, UI_F18);
    lv_label_set_text(w_connInfo, "Loading...");

    // CNDE stats + Error codes
    y += 160;
    w_cndeFrames = ui_label(scr, 20, y, lw, UI_DIM, UI_F16);
    y += 30;
    w_errorCode = ui_label(scr, 20, y, lw, UI_GREEN, UI_F20);

    lv_obj_set_height(scr, 1200);
    return scr;
}
