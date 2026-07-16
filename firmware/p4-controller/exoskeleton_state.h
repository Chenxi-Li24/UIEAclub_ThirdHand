#pragma once

#include <Arduino.h>

struct ExoskeletonTelemetry {
    uint32_t sequence = 0;
    float angles[6] = {0};
    float robotTargets[6] = {0};
    float millivolts[6] = {0};
    float zeroOffsets[6] = {0};
    int8_t directions[6] = {1, 1, 1, 1, 1, 1};
    uint32_t lastUpdate = 0;
    bool valid = false;
    bool calibrated = false;
    bool calibrating = false;
};

// H1->J1, H2->J2, H3->J4, H4->J3, H5->J6, H6->J5.
static constexpr uint8_t EXOSKELETON_TO_ROBOT_JOINT[6] = {0, 1, 3, 2, 5, 4};

ExoskeletonTelemetry exoskeletonGetTelemetry();
bool exoskeletonControlEnabled();
bool exoskeletonSetControlEnabled(bool enabled);
bool exoskeletonCalibrateZero();
bool exoskeletonSetDirection(uint8_t channel, int8_t direction);
