// src/hw/melody.h — 旋律库 + 播放
#pragma once
#include <stdint.h>

struct MelodyNote {
  uint16_t freq;   // Hz, 0 = rest
  uint16_t dur;    // ms
  uint16_t gap;    // ms silence after
};

struct MelodyDef {
  const char *name;
  const MelodyNote *notes;
  uint8_t count;
};

// Play a melody through the buzzer queue
void melodyPlay(const MelodyDef *m);

// Predefined melodies
extern const MelodyDef MELODY_SHUI_CHE;    // 洒水车 — 致爱丽丝
extern const MelodyDef MELODY_LAN_HUA_CAO; // 兰花草
extern const MelodyDef MELODY_HAPPY_BDAY;  // 生日快乐
extern const MelodyDef MELODY_TWINKLE;     // 小星星
extern const MelodyDef MELODY_ODE_JOY;     // 欢乐颂
extern const MelodyDef MELODY_MARIO;       // 超级玛丽

// All melodies in display order
extern const MelodyDef *const MELODY_LIST[];
extern const uint8_t MELODY_COUNT;
