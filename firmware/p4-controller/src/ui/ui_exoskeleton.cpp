#include <Arduino.h>
#include "ui/ui_exoskeleton.h"
#include "ui/ui_core.h"
#include "exoskeleton_state.h"
#include "config.h"

static lv_obj_t* scr = nullptr;
static lv_obj_t* w_link = nullptr;
static lv_obj_t* w_sequence = nullptr;
static lv_obj_t* w_control = nullptr;
static lv_obj_t* w_controlStatus = nullptr;
static lv_obj_t* w_angles[6] = {nullptr};
static lv_obj_t* w_targets[6] = {nullptr};
static lv_obj_t* w_voltages[6] = {nullptr};
static lv_obj_t* w_voltageBars[6] = {nullptr};
static lv_obj_t* w_calibration = nullptr;
static bool s_updatingSwitch = false;

static void onControlChanged(lv_event_t* event) {
    if (s_updatingSwitch) return;

    lv_obj_t* control = lv_event_get_target(event);
    bool requested = lv_obj_has_state(control, LV_STATE_CHECKED);
    bool enabled = exoskeletonSetControlEnabled(requested);
    if (requested && !enabled) {
        lv_label_set_text(w_controlStatus, "NOT READY");
        lv_obj_set_style_text_color(w_controlStatus, lv_color_hex(UI_RED), 0);
    }
    if (enabled != requested) {
        s_updatingSwitch = true;
        if (enabled) lv_obj_add_state(control, LV_STATE_CHECKED);
        else lv_obj_clear_state(control, LV_STATE_CHECKED);
        s_updatingSwitch = false;
    }
}

static void onCalibrateZero(lv_event_t* event) {
    bool calibrated = exoskeletonCalibrateZero();
    lv_label_set_text(w_calibration,
        calibrated ? "ZERO CALIBRATED" : "NEEDS LIVE SENSOR DATA");
    lv_obj_set_style_text_color(w_calibration,
        lv_color_hex(calibrated ? UI_GREEN : UI_RED), 0);
}

void ui_exoskeleton_refresh() {
    if (!scr) return;

    ExoskeletonTelemetry telemetry = exoskeletonGetTelemetry();
    uint32_t age = telemetry.valid ? millis() - telemetry.lastUpdate : UINT32_MAX;
    bool live = telemetry.valid && age <= EXO_PACKET_TIMEOUT_MS;
    bool enabled = exoskeletonControlEnabled();

    lv_label_set_text(w_calibration,
        telemetry.calibrated ? "ZERO CALIBRATED" : "ZERO NOT CALIBRATED");
    lv_obj_set_style_text_color(w_calibration,
        lv_color_hex(telemetry.calibrated ? UI_GREEN : UI_AMBER), 0);
    lv_label_set_text(w_controlStatus,
        enabled ? (live ? "FOLLOWING" : "WAITING DATA") : (live ? "READY" : "DATA STALE"));
    lv_obj_set_style_text_color(w_controlStatus,
        lv_color_hex(enabled ? (live ? UI_GREEN : UI_AMBER) : (live ? UI_CYAN : UI_RED)), 0);

    if (!telemetry.valid) {
        lv_label_set_text(w_link, "WAITING FOR SENSOR");
        lv_obj_set_style_text_color(w_link, lv_color_hex(UI_AMBER), 0);
        lv_label_set_text(w_sequence, "No EXO packets received");
    } else if (live) {
        lv_label_set_text(w_link, "LIVE");
        lv_obj_set_style_text_color(w_link, lv_color_hex(UI_GREEN), 0);
        ui_label_setf(w_sequence, "Seq %lu  |  updated %lu ms ago",
                      (unsigned long)telemetry.sequence, (unsigned long)age);
    } else {
        lv_label_set_text(w_link, "STALE");
        lv_obj_set_style_text_color(w_link, lv_color_hex(UI_RED), 0);
        ui_label_setf(w_sequence, "Seq %lu  |  last update %lu ms ago",
                      (unsigned long)telemetry.sequence, (unsigned long)age);
    }

    s_updatingSwitch = true;
    if (enabled) lv_obj_add_state(w_control, LV_STATE_CHECKED);
    else lv_obj_clear_state(w_control, LV_STATE_CHECKED);
    lv_obj_clear_state(w_control, LV_STATE_DISABLED);
    s_updatingSwitch = false;

    for (int i = 0; i < 6; ++i) {
        if (telemetry.valid) {
            ui_label_setf(w_angles[i], "%.2f deg", telemetry.angles[i]);
            ui_label_setf(w_targets[i], "%.2f deg", telemetry.robotTargets[i]);
            ui_label_setf(w_voltages[i], "%.3f V  |  %.0f mV",
                          telemetry.millivolts[i] / 1000.0f, telemetry.millivolts[i]);
            lv_bar_set_value(w_voltageBars[i], (int32_t)telemetry.millivolts[i], LV_ANIM_OFF);
        } else {
            lv_label_set_text(w_angles[i], "--- deg");
            lv_label_set_text(w_targets[i], "--- deg");
            lv_label_set_text(w_voltages[i], "--- V  |  --- mV");
            lv_bar_set_value(w_voltageBars[i], 0, LV_ANIM_OFF);
        }
    }
}

lv_obj_t* ui_exoskeleton_create(lv_obj_t* parent) {
    scr = lv_obj_create(parent);
    lv_obj_set_size(scr, UI_W, UI_H - UI_TAB_H);
    lv_obj_set_pos(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = ui_label(scr, 0, 4, UI_W, UI_GREY, UI_F32);
    lv_label_set_text(title, LV_SYMBOL_CHARGE "  Exoskeleton Control");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    const int pageMargin = 20;
    const int contentW = UI_W - pageMargin * 2;
    lv_obj_t* status = ui_card(scr, pageMargin, 55, contentW, 142, UI_CARD);

    w_link = ui_label(status, 0, 4, 290, UI_AMBER, UI_F28);
    lv_label_set_text(w_link, "WAITING FOR SENSOR");
    w_sequence = ui_label(status, 0, 44, 470, UI_DIM, UI_F18);
    lv_label_set_text(w_sequence, "No EXO packets received");

    lv_obj_t* controlLabel = ui_label(status, 500, 10, 210, UI_WHITE, UI_F20);
    lv_label_set_text(controlLabel, "Robot control");
    lv_obj_set_style_text_align(controlLabel, LV_TEXT_ALIGN_RIGHT, 0);

    w_control = lv_switch_create(status);
    lv_obj_set_size(w_control, 92, 48);
    lv_obj_set_pos(w_control, 618, 48);
    lv_obj_set_style_bg_color(w_control, lv_color_hex(UI_GREEN), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(w_control, onControlChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_state(w_control, LV_STATE_DISABLED);
    w_controlStatus = ui_label(status, 490, 102, 220, UI_RED, UI_F14);
    lv_label_set_text(w_controlStatus, "DATA STALE");
    lv_obj_set_style_text_align(w_controlStatus, LV_TEXT_ALIGN_RIGHT, 0);

    const int cardW = 365;
    const int cardH = 205;
    const int gapX = 10;
    const int gapY = 12;
    const int startY = 215;

    for (int i = 0; i < 6; ++i) {
        int col = i % 2;
        int row = i / 2;
        int x = pageMargin + col * (cardW + gapX);
        int y = startY + row * (cardH + gapY);
        lv_obj_t* card = ui_card(scr, x, y, cardW, cardH, UI_CARD);

        lv_obj_t* joint = ui_label(card, 0, 0, 70, UI_CYAN, UI_F28);
        ui_label_setf(joint, "J%d", i + 1);

        lv_obj_t* angleCaption = ui_label(card, 82, 5, 105, UI_DIM, UI_F16);
        lv_label_set_text(angleCaption, "ANGLE");
        w_angles[i] = ui_label(card, 82, 30, 235, UI_WHITE, UI_F32);
        lv_label_set_text(w_angles[i], "--- deg");

        lv_obj_t* targetCaption = ui_label(card, 0, 78, 110, UI_DIM, UI_F16);
        lv_label_set_text(targetCaption, "ROBOT");
        w_targets[i] = ui_label(card, 112, 74, 220, UI_CYAN, UI_F20);
        lv_label_set_text(w_targets[i], "--- deg");
        lv_obj_set_style_text_align(w_targets[i], LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t* voltageCaption = ui_label(card, 0, 112, 110, UI_DIM, UI_F16);
        lv_label_set_text(voltageCaption, "VOLTAGE");
        w_voltages[i] = ui_label(card, 112, 108, 220, UI_WHITE, UI_F18);
        lv_label_set_text(w_voltages[i], "--- V");
        lv_obj_set_style_text_align(w_voltages[i], LV_TEXT_ALIGN_RIGHT, 0);

        w_voltageBars[i] = lv_bar_create(card);
        lv_obj_set_size(w_voltageBars[i], cardW - 24, 14);
        lv_obj_set_pos(w_voltageBars[i], 0, 145);
        lv_bar_set_range(w_voltageBars[i], 0, 3300);
        lv_bar_set_value(w_voltageBars[i], 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(w_voltageBars[i], lv_color_hex(0x3186), LV_PART_MAIN);
        lv_obj_set_style_bg_color(w_voltageBars[i], lv_color_hex(UI_CYAN), LV_PART_INDICATOR);
        lv_obj_set_style_radius(w_voltageBars[i], 7, LV_PART_MAIN);
        lv_obj_set_style_radius(w_voltageBars[i], 7, LV_PART_INDICATOR);

        lv_obj_t* range = ui_label(card, 0, 163, cardW - 24, UI_DIM, UI_F14);
        lv_label_set_text(range, "0 V                                      3.3 V");
    }

    lv_obj_t* calibrate = lv_btn_create(scr);
    lv_obj_set_size(calibrate, 500, 62);
    lv_obj_set_pos(calibrate, 150, 875);
    lv_obj_set_style_bg_color(calibrate, lv_color_hex(UI_CARD), 0);
    lv_obj_set_style_border_color(calibrate, lv_color_hex(UI_CYAN), 0);
    lv_obj_set_style_border_width(calibrate, 2, 0);
    lv_obj_set_style_radius(calibrate, 8, 0);
    lv_obj_add_event_cb(calibrate, onCalibrateZero, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* calibrateLabel = lv_label_create(calibrate);
    lv_label_set_text(calibrateLabel, "SET CURRENT EXO POSE AS ROBOT ZERO");
    lv_obj_set_style_text_font(calibrateLabel, UI_F18, 0);
    lv_obj_center(calibrateLabel);

    w_calibration = ui_label(scr, pageMargin, 945, contentW, UI_AMBER, UI_F16);
    lv_label_set_text(w_calibration, "ZERO NOT CALIBRATED");
    lv_obj_set_style_text_align(w_calibration, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* safety = ui_label(scr, pageMargin, 975, contentW, UI_DIM, UI_F16);
    lv_label_set_text(safety, "Data loss pauses motion; follow remains on until switched off.");
    lv_obj_set_style_text_align(safety, LV_TEXT_ALIGN_CENTER, 0);

    return scr;
}
