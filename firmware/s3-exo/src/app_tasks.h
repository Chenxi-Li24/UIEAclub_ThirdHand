#pragma once

#include <Arduino.h>

// Core 1 remains dedicated to the Arduino loop, LVGL and touch polling.
// Blocking robot networking and the time-critical ServoJ stream run on Core 0.
bool appTasksStart();
int appTasksTakeMotionError();
void appTasksLogStats();

