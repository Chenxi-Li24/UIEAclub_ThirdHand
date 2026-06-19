// src/hw/audio.h — PWM 蜂鸣器 + AI 语音预留

#pragma once
#include <stdint.h>

bool hwAudioInit();
void hwBeep(uint16_t freqHz, uint16_t durMs);

// AI 语音预留
void hwVoiceSay(const char *text);
bool hwVoiceCmdAvailable();
int hwVoiceReadCmd();  // 返回命令 ID, -1 无命令
