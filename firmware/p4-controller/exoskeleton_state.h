#pragma once

#include <Arduino.h>

struct ExoskeletonTelemetry {
    uint32_t sequence = 0;
    float angles[6] = {0};
    float robotTargets[6] = {0};
    float millivolts[6] = {0};
    float zeroOffsets[6] = {0};
    uint32_t lastUpdate = 0;
    bool valid = false;
    bool calibrated = false;
};

ExoskeletonTelemetry exoskeletonGetTelemetry();
bool exoskeletonControlEnabled();
bool exoskeletonSetControlEnabled(bool enabled);
bool exoskeletonCalibrateZero();
