#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "fairino_udp.h"

class SafeServoMotion {
public:
    void begin(FairinoUDPClient* client);
    int setTarget(const float target[6], const float current[6], bool continuous);
    int tick(bool forcePeriod = false);
    int stop();
    void reset();

    bool active() const { return _active; }
    bool atTarget() const { return _atTarget; }
    int lastError() const { return _lastError; }

private:
    static constexpr uint32_t PERIOD_US = 16000;
    static constexpr float CMD_T_SEC = 0.016f;
    static constexpr float COMMAND_SPEED_DEG_S = 20.0f;
    static constexpr float J1_COMMAND_SPEED_DEG_S = 20.0f;
    static constexpr float HARD_SPEED_LIMIT_DEG_S = 20.0f;
    static constexpr float MAX_ACCEL_DEG_S2 = 10.0f;
    static constexpr float J1_MAX_ACCEL_DEG_S2 = 10.0f;
    static constexpr float MAX_JERK_DEG_S3 = 80.0f;
    static constexpr float CONTROLLER_OMEGA = 1.5f;
    static constexpr float POSITION_EPSILON_DEG = 0.05f;

    FairinoUDPClient* _client = nullptr;
    float _target[6] = {};
    float _command[6] = {};
    float _velocity[6] = {};
    float _acceleration[6] = {};
    uint32_t _lastTickUs = 0;
    volatile bool _active = false;
    bool _continuous = false;
    volatile bool _atTarget = false;
    volatile int _lastError = FR_OK;
    SemaphoreHandle_t _mutex = nullptr;

    static bool validJoints(const float joints[6]);
    bool lock();
    void unlock();
    void clearMotionLocked();
};
