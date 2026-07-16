#include "safe_motion.h"

#include <math.h>

void SafeServoMotion::begin(FairinoUDPClient* client) {
    _client = client;
    if (!_mutex) _mutex = xSemaphoreCreateMutex();
    reset();
}

bool SafeServoMotion::lock() {
    return !_mutex || xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE;
}

void SafeServoMotion::unlock() {
    if (_mutex) xSemaphoreGive(_mutex);
}

void SafeServoMotion::clearMotionLocked() {
    memset(_velocity, 0, sizeof(_velocity));
    memset(_acceleration, 0, sizeof(_acceleration));
    _lastTickUs = 0;
    _active = false;
    _continuous = false;
    _atTarget = false;
}

bool SafeServoMotion::validJoints(const float joints[6]) {
    if (!joints) return false;
    for (int i = 0; i < 6; ++i) {
        if (!isfinite(joints[i])) return false;
    }
    return true;
}

int SafeServoMotion::setTarget(const float target[6], const float current[6],
                               bool continuous) {
    if (!_client || !validJoints(target)) return FR_ERR_BAD_FRAME;
    if (!lock()) return FR_ERR_TIMEOUT;

    memcpy(_target, target, sizeof(_target));
    _continuous = continuous;
    _atTarget = false;

    if (_active) {
        unlock();
        return FR_OK;
    }
    if (!validJoints(current)) {
        unlock();
        return FR_ERR_BAD_FRAME;
    }

    memcpy(_command, current, sizeof(_command));
    memset(_velocity, 0, sizeof(_velocity));
    memset(_acceleration, 0, sizeof(_acceleration));

    _lastError = _client->servoMoveStart();
    if (_lastError != FR_OK) {
        int result = _lastError;
        unlock();
        return result;
    }

    _active = true;
    _lastTickUs = micros() - PERIOD_US;
    Serial.printf("[SAFE-MOTION] start: %.3fs, J1-J6 %.1f deg/s, hard %.1f deg/s\n",
                  CMD_T_SEC, COMMAND_SPEED_DEG_S, HARD_SPEED_LIMIT_DEG_S);
    unlock();
    return FR_OK;
}

int SafeServoMotion::tick(bool forcePeriod) {
    if (!_active || !_client) return FR_OK;
    if (!lock()) return FR_OK;
    if (!_active) {
        unlock();
        return FR_OK;
    }

    const uint32_t now = micros();
    if (!forcePeriod && (uint32_t)(now - _lastTickUs) < PERIOD_US) {
        unlock();
        return FR_OK;
    }
    _lastTickUs = now;

    constexpr float dt = CMD_T_SEC;
    constexpr float maxAccelerationChange = MAX_JERK_DEG_S3 * dt;
    constexpr float hardMaxStep = HARD_SPEED_LIMIT_DEG_S * dt;
    bool reached = true;

    for (int i = 0; i < 6; ++i) {
        const float commandSpeed = i == 0 ? J1_COMMAND_SPEED_DEG_S : COMMAND_SPEED_DEG_S;
        const float maxAcceleration = i == 0 ? J1_MAX_ACCEL_DEG_S2 : MAX_ACCEL_DEG_S2;
        const float delta = _target[i] - _command[i];
        if (fabsf(delta) <= POSITION_EPSILON_DEG &&
            fabsf(_velocity[i]) <= 0.1f &&
            fabsf(_acceleration[i]) <= maxAccelerationChange) {
            _command[i] = _target[i];
            _velocity[i] = 0.0f;
            _acceleration[i] = 0.0f;
            continue;
        }

        reached = false;
        const float desiredAcceleration = constrain(
            CONTROLLER_OMEGA * CONTROLLER_OMEGA * delta -
                2.0f * CONTROLLER_OMEGA * _velocity[i],
            -maxAcceleration, maxAcceleration);
        const float accelerationDelta = constrain(
            desiredAcceleration - _acceleration[i],
            -maxAccelerationChange, maxAccelerationChange);
        _acceleration[i] = constrain(
            _acceleration[i] + accelerationDelta,
            -maxAcceleration, maxAcceleration);
        _velocity[i] = constrain(
            _velocity[i] + _acceleration[i] * dt,
            -commandSpeed, commandSpeed);

        const float step = constrain(_velocity[i] * dt, -hardMaxStep, hardMaxStep);
        if ((delta > 0.0f && step >= delta) || (delta < 0.0f && step <= delta)) {
            _command[i] = _target[i];
            _velocity[i] = 0.0f;
            _acceleration[i] = 0.0f;
        } else {
            _command[i] += step;
        }
    }

    // These SDK parameters are currently unavailable and must stay zero.
    _lastError = _client->servoJ(
        _command[0], _command[1], _command[2],
        _command[3], _command[4], _command[5],
        0.0f, 0.0f, CMD_T_SEC, 0.0f, 0.0f);
    if (_lastError != FR_OK) {
        _client->servoMoveEnd();
        const int result = _lastError;
        clearMotionLocked();
        _lastError = result;
        unlock();
        return result;
    }

    _atTarget = reached;
    if (_atTarget && !_continuous) {
        const int result = _client->servoMoveEnd();
        clearMotionLocked();
        _lastError = result;
        unlock();
        return result;
    }
    unlock();
    return FR_OK;
}

int SafeServoMotion::stop() {
    if (!lock()) return FR_ERR_TIMEOUT;

    const int result = (_active && _client) ? _client->servoMoveEnd() : FR_OK;
    clearMotionLocked();
    _lastError = result;
    unlock();
    return result;
}

void SafeServoMotion::reset() {
    if (!lock()) return;
    clearMotionLocked();
    _lastError = FR_OK;
    unlock();
}
