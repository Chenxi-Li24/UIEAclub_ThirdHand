// ui_voice.cpp - Voice Control page (Tab "Voice")
// 800x1280 screen: mic button, status, recognized text, history
#include <Arduino.h>
#include "ui/ui_voice.h"
#include "ui/ui_core.h"
#include "config.h"
#if VOICE_ENABLED
#include "voice/voice_cmd.h"
#endif
#include <cstdio>

static lv_obj_t *s_scr, *s_statusLabel, *s_textLabel, *s_micBtn;
static lv_obj_t *s_micBtnLabel, *s_errorLabel, *s_responseLabel, *s_historyArea;

#define HIST_MAX 20
static String s_history[HIST_MAX];
static int s_histIdx = 0, s_histCount = 0;

static void addHistory(const char* text) {
    s_history[s_histIdx] = text;
    s_histIdx = (s_histIdx + 1) % HIST_MAX;
    if (s_histCount < HIST_MAX) s_histCount++;
    if (!s_historyArea) return;
    String all;
    for (int i = 0; i < s_histCount; i++) {
        int idx = (s_histIdx - s_histCount + i + HIST_MAX) % HIST_MAX;
        all += LV_SYMBOL_AUDIO " ";
        all += s_history[idx];
        if (i < s_histCount - 1) all += "\n";
    }
    lv_label_set_text(s_historyArea, all.c_str());
}

static void onMicPress(lv_event_t*) {
#if VOICE_ENABLED
    voiceCmdStart();
#endif
    lv_label_set_text(s_micBtnLabel, LV_SYMBOL_AUDIO " Listening...");
    lv_obj_set_style_bg_color(s_micBtn, lv_color_hex(0xFF0000), 0);
}

static void onMicRelease(lv_event_t*) {
    lv_label_set_text(s_micBtnLabel, LV_SYMBOL_AUDIO "\nSpeak");
    lv_obj_set_style_bg_color(s_micBtn, lv_color_hex(UI_BLUE), 0);
}

lv_obj_t* ui_voice_create(lv_obj_t* parent) {
    s_scr = parent;
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(UI_BG), 0);

    lv_obj_t* title = ui_label(s_scr, 0, 20, UI_W, UI_WHITE, UI_F24);
    lv_label_set_text(title, LV_SYMBOL_AUDIO " Voice Control");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    s_statusLabel = ui_label(s_scr, 0, 70, UI_W, UI_GREY, UI_F18);
    lv_label_set_text(s_statusLabel, "Tap & hold mic to speak");
    lv_obj_set_style_text_align(s_statusLabel, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* card = ui_card(s_scr, 40, 120, UI_W - 80, 160, UI_CARD);
    s_textLabel = ui_label(card, 0, 0, UI_W - 100, UI_WHITE, UI_F20);
    lv_label_set_text(s_textLabel, "");
    lv_obj_set_style_text_align(s_textLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_textLabel);

    s_errorLabel = ui_label(s_scr, 0, 290, UI_W, UI_RED, UI_F16);
    lv_label_set_text(s_errorLabel, "");
    lv_obj_set_style_text_align(s_errorLabel, LV_TEXT_ALIGN_CENTER, 0);

    const int bs = 200;
    s_micBtn = lv_btn_create(s_scr);
    lv_obj_set_size(s_micBtn, bs, bs);
    lv_obj_set_pos(s_micBtn, (UI_W - bs) / 2, 330);
    lv_obj_set_style_bg_color(s_micBtn, lv_color_hex(UI_BLUE), 0);
    lv_obj_set_style_radius(s_micBtn, bs / 2, 0);
    lv_obj_set_style_border_width(s_micBtn, 4, 0);
    lv_obj_set_style_border_color(s_micBtn, lv_color_hex(UI_CYAN), 0);
    lv_obj_set_style_shadow_width(s_micBtn, 20, 0);
    lv_obj_set_style_shadow_color(s_micBtn, lv_color_hex(UI_BLUE), 0);

    s_micBtnLabel = lv_label_create(s_micBtn);
    lv_label_set_text(s_micBtnLabel, LV_SYMBOL_AUDIO "\nSpeak");
    lv_obj_set_style_text_color(s_micBtnLabel, lv_color_hex(UI_WHITE), 0);
    lv_obj_set_style_text_font(s_micBtnLabel, UI_F24, 0);
    lv_obj_set_style_text_align(s_micBtnLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_micBtnLabel);

    lv_obj_add_event_cb(s_micBtn, onMicPress, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(s_micBtn, onMicRelease, LV_EVENT_RELEASED, nullptr);

    s_responseLabel = ui_label(s_scr, 40, 560, UI_W - 80, UI_AMBER, UI_F16);
    lv_label_set_text(s_responseLabel, "");
    lv_obj_set_style_text_align(s_responseLabel, LV_TEXT_ALIGN_CENTER, 0);

    ui_divider(s_scr, 10, 630, UI_W - 20);
    lv_obj_t* ht = ui_label(s_scr, 40, 645, UI_W - 80, UI_GREY, UI_F16);
    lv_label_set_text(ht, "Command History");

    s_historyArea = ui_label(s_scr, 40, 680, UI_W - 80, UI_WHITE, UI_F14);
    lv_label_set_text(s_historyArea, "");
    lv_obj_set_style_text_line_space(s_historyArea, 4, 0);

    return s_scr;
}

void ui_voice_refresh() {
    if (!s_scr) return;
#if VOICE_ENABLED
    VoiceState st = voiceCmdState();
    switch (st) {
    case VOICE_IDLE:
        lv_label_set_text(s_statusLabel, "Ready — tap mic to speak");
        lv_obj_set_style_bg_color(s_micBtn, lv_color_hex(UI_BLUE), 0);
        break;
    case VOICE_LISTENING:
        lv_label_set_text(s_statusLabel, "Listening...");
        lv_obj_set_style_bg_color(s_micBtn, lv_color_hex(UI_RED), 0);
        break;
    case VOICE_ASR_PROCESSING:
        lv_label_set_text(s_statusLabel, "Recognizing speech...");
        break;
    case VOICE_AGENT_RELAY:
        lv_label_set_text(s_statusLabel, "Asking Claude...");
        break;
    case VOICE_EXECUTING:
        lv_label_set_text(s_statusLabel, "Robot executing...");
        break;
    case VOICE_SPEAKING:
        lv_label_set_text(s_statusLabel, "Speaking response...");
        break;
    case VOICE_ERROR:
        lv_label_set_text(s_statusLabel, "Error occurred");
        lv_obj_set_style_bg_color(s_micBtn, lv_color_hex(UI_BG), 0);
        break;
    }

    const char* txt = voiceCmdLastText();
    if (txt && strlen(txt) > 0) {
        lv_label_set_text(s_textLabel, txt);
        static VoiceState lastSt = VOICE_IDLE;
        if (lastSt == VOICE_AGENT_RELAY && st != VOICE_AGENT_RELAY) addHistory(txt);
        lastSt = st;
    }

    const char* err = voiceCmdLastError();
    lv_label_set_text(s_errorLabel, err && strlen(err) > 0 ? err : "");

    const char* resp = voiceCmdLastResponse();
    if (resp && strlen(resp) > 0)
        lv_label_set_text(s_responseLabel, resp);
#else
    lv_label_set_text(s_statusLabel, "Voice disabled (set VOICE_ENABLED=1 in config.h)");
    lv_obj_set_style_bg_color(s_micBtn, lv_color_hex(0x528A), 0);
#endif
}
