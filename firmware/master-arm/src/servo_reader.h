#pragma once
// ServoReader — reads 6x STS3215 servo angles via UART0
// Wraps the FTServo SCServo SDK (SMS_STS class)

#include <Arduino.h>
#include "config.h"

class ServoReader {
public:
    void begin(HardwareSerial& serial);
    bool readAngles(float angles[SERVO_COUNT]);   // output in degrees
    int  readPos(uint8_t id);                      // raw position 0-4095, -1 on error
    bool ping(uint8_t id);                         // check if servo at id responds
    bool allAlive();                                // true if all 6 servos responding

private:
    HardwareSerial* _serial;
    int   _lastPos[SERVO_COUNT];                   // fallback on error
    float _hist[SERVO_COUNT][3];                   // moving average window
    int   _histIdx[SERVO_COUNT];

    float _posToDeg(int pos);                       // 0-4095 -> degrees
    float _filter(uint8_t idx, float newVal);       // moving average (window=3)
};
