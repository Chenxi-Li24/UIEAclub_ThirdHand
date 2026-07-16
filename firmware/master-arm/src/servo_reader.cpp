#include "servo_reader.h"
#include <SCServo.h>

static SMS_STS _servo;

void ServoReader::begin(HardwareSerial& serial) {
    _serial = &serial;
    _serial->begin(SERVO_BAUD, SERIAL_8N1, SERVO_RX, SERVO_TX);
    _servo.pSerial = _serial;
    delay(100);  // let servo bus stabilize

    // Init history buffers
    for (int i = 0; i < SERVO_COUNT; i++) {
        _lastPos[i]   = 2048;
        _histIdx[i]   = 0;
        _hist[i][0] = _hist[i][1] = _hist[i][2] = 0;
    }

    Serial.printf("[Servo] UART0 begin OK, baud=%d, RX=%d TX=%d\n",
                  SERVO_BAUD, SERVO_RX, SERVO_TX);
}

int ServoReader::readPos(uint8_t id) {
    int pos = _servo.ReadPos(id);
    if (pos == -1 || _servo.getLastError()) {
        return -1;
    }
    return pos;
}

bool ServoReader::readAngles(float angles[SERVO_COUNT]) {
    bool allOk = true;
    for (uint8_t id = 1; id <= SERVO_COUNT; id++) {
        int pos = readPos(id);
        if (pos < 0) {
            angles[id - 1] = _posToDeg(_lastPos[id - 1]);
            allOk = false;
        } else {
            _lastPos[id - 1] = pos;
            float deg = _posToDeg(pos);
            angles[id - 1] = _filter(id - 1, deg);
        }
    }
    return allOk;
}

bool ServoReader::ping(uint8_t id) {
    return _servo.Ping(id) == id;
}

bool ServoReader::allAlive() {
    for (uint8_t id = 1; id <= SERVO_COUNT; id++) {
        if (!ping(id)) {
            Serial.printf("[Servo] ID %d no response\n", id);
            return false;
        }
    }
    Serial.println("[Servo] All 6 servos OK");
    return true;
}

float ServoReader::_posToDeg(int pos) {
    return (float)pos / SERVO_POS_MAX * 360.0f;
}

float ServoReader::_filter(uint8_t idx, float newVal) {
    _hist[idx][_histIdx[idx]] = newVal;
    _histIdx[idx] = (_histIdx[idx] + 1) % 3;
    return (_hist[idx][0] + _hist[idx][1] + _hist[idx][2]) / 3.0f;
}
