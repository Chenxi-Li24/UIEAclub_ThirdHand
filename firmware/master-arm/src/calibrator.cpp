#include "calibrator.h"
#include <Preferences.h>

static const char* NVS_NS  = "servo";
static const char* NVS_KEY = "offsets";

void Calibrator::begin() {
    _load();
    if (_calibrated) {
        Serial.println("[Calib] Loaded offsets from NVS:");
        for (int i = 0; i < SERVO_COUNT; i++) {
            Serial.printf("  J%d: %.1f deg\n", i + 1, _offsets[i]);
        }
    } else {
        Serial.println("[Calib] NOT calibrated - waiting for zero-point");
    }
}

bool Calibrator::isCalibrated() {
    return _calibrated;
}

void Calibrator::confirmZero(const float angles[SERVO_COUNT]) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        _offsets[i] = angles[i];
    }
    _calibrated = true;
    _save();

    Serial.println("[Calib] ZERO CONFIRMED:");
    for (int i = 0; i < SERVO_COUNT; i++) {
        Serial.printf("  J%d offset: %.1f deg\n", i + 1, _offsets[i]);
    }
}

void Calibrator::apply(float angles[SERVO_COUNT]) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        angles[i] = (angles[i] - _offsets[i]) * JOINT_SIGN[i] * JOINT_SCALE[i];
        if (angles[i] < JOINT_MIN_DEG) angles[i] = JOINT_MIN_DEG;
        if (angles[i] > JOINT_MAX_DEG) angles[i] = JOINT_MAX_DEG;
    }
}

void Calibrator::reset() {
    memset(_offsets, 0, sizeof(_offsets));
    _calibrated = false;
    _save();
    Serial.println("[Calib] RESET - offsets cleared");
}

void Calibrator::dump() {
    Serial.println("[Calib] Current offsets (deg):");
    for (int i = 0; i < SERVO_COUNT; i++) {
        Serial.printf("  J%d offset=%.1f\n", i + 1, _offsets[i]);
    }
    Serial.printf("[Calib] _calibrated=%d\n", _calibrated);
}

void Calibrator::forceDefaults() {
    // Erase NVS, then reload defaults
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.clear();
    prefs.end();
    // Re-load will trigger len!=sizeof check and use defaults
    _load();
    Serial.println("[Calib] Forced defaults from config.h");
}

void Calibrator::_save() {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.putBytes(NVS_KEY, _offsets, sizeof(_offsets));
    prefs.putBool("calibrated", _calibrated);
    prefs.end();
}

void Calibrator::_load() {
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    size_t len = prefs.getBytes(NVS_KEY, _offsets, sizeof(_offsets));
    _calibrated = prefs.getBool("calibrated", false);
    prefs.end();
    if (len != sizeof(_offsets)) {
        // No saved calibration — use pre-configured zero offsets
        float zeroRaw[] = {ZERO_POS_1, ZERO_POS_2, ZERO_POS_3, ZERO_POS_4, ZERO_POS_5, ZERO_POS_6};
        for (int i = 0; i < SERVO_COUNT; i++) {
            _offsets[i] = zeroRaw[i] / SERVO_POS_MAX * 360.0f;
        }
        _calibrated = true;
        _save();
        Serial.println("[Calib] Auto-loaded pre-configured zero offsets");
    }
}
