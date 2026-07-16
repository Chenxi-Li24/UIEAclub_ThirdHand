// hw/input.h — GSL3680 I2C touch → LVGL indev
#pragma once

#include <stdint.h>

// Initialize touch controller + register LVGL input device
bool hwInputInit();

// Called from loop() to feed LVGL indev
void hwInputUpdate();
