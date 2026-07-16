#pragma once
// Calibrator — zero-point calibration with NVS persistence
// Stores 6 joint offsets so raw servo angles map to Fairino joint space.

#include <Arduino.h>
#include "config.h"

class Calibrator {
public:
    void begin();
    bool isCalibrated();
    void confirmZero(const float angles[SERVO_COUNT]);  // save current as zero
    void apply(float angles[SERVO_COUNT]);               // in-place apply offset+sign+scale+clamp
    void reset();                                         // clear calibration
    void dump();                                          // print current offsets
    void forceDefaults();                                 // force reload from config.h defaults

private:
    float _offsets[SERVO_COUNT];
    bool  _calibrated;

    void _save();
    void _load();
};
