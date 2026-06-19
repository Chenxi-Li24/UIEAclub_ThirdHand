// src/hw/audio.cpp — PWM 蜂鸣器 + AI 语音 (预留)

#include "hw/audio.h"

#include "hw/pins.h"

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

static const int BEEP_CH = 1;
static QueueHandle_t s_beepQ = nullptr;
struct BeepReq {
  uint16_t freq;
  uint16_t dur;
};

static void beepTask(void *) {
  BeepReq r;
  while (xQueueReceive(s_beepQ, &r, portMAX_DELAY) == pdTRUE) {
    ledcWriteTone(BEEP_CH, r.freq);
    vTaskDelay(pdMS_TO_TICKS(r.dur));
    ledcWriteTone(BEEP_CH, 0);
  }
}

bool hwAudioInit() {
  ledcSetup(BEEP_CH, 2000, 8);
  ledcAttachPin(PIN_BEEP, BEEP_CH);
  ledcWrite(BEEP_CH, 0);

  s_beepQ = xQueueCreate(64, sizeof(BeepReq));
  if (!s_beepQ)
    return false;
  xTaskCreatePinnedToCore(beepTask, "beep", 2048, nullptr, 2, nullptr, tskNO_AFFINITY);

  Serial.println("hwAudio: PWM beeper OK, voice stub");
  return true;
}

void hwBeep(uint16_t freqHz, uint16_t durMs) {
  if (!s_beepQ)
    return;
  BeepReq r{freqHz, durMs};
  xQueueSend(s_beepQ, &r, 0);
}

void hwVoiceSay(const char *text) {
  (void)text;
}
bool hwVoiceCmdAvailable() {
  return false;
}
int hwVoiceReadCmd() {
  return -1;
}
