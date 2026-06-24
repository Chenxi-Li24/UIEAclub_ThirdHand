#pragma once
// Command Queue — Ring Buffer + Heartbeat + Robot State Machine
// Called from p4-robot.ino via #include "cmd_queue.h"

#include <Arduino.h>
#include <stdint.h>

// ── Ring Buffer (power-of-2, overwrites oldest when full) ─────────

template <typename T, uint16_t N>
class RingBuffer {
    static_assert((N & (N - 1)) == 0, "N must be power of 2");
public:
    RingBuffer() : _r(0), _w(0), _count(0) {}

    void clear() { _r = _w = _count = 0; }
    bool empty() const { return _count == 0; }
    uint16_t count() const { return _count; }

    void put(const T& item) {
        _buf[_w & (N - 1)] = item;
        _w++;
        if (_count < N) { _count++; }
        else { _r++; }  // overwrite oldest
    }

    bool get(T& out) {
        if (_count == 0) return false;
        out = _buf[_r & (N - 1)];
        _r++; _count--;
        return true;
    }

private:
    T _buf[N];
    uint16_t _r, _w, _count;
};

// ── Command Entry ──────────────────────────────────────────────────

enum CmdType : uint8_t {
    CMD_NONE = 0,
    CMD_SERVO_MOVE,
    CMD_SERVO_START,
    CMD_SERVO_END,
    CMD_ESTOP,
    CMD_RESET,
    CMD_HEARTBEAT,
};

struct CmdEntry {
    CmdType type;
    float joints[6];
    uint32_t ts;
};

// ── Command Queue (estop priority 1 + normal priority 2) ───────────

class CmdQueue {
public:
    void enqueue(const CmdEntry& cmd) {
        if (cmd.type == CMD_ESTOP) _estop.put(cmd);
        else _normal.put(cmd);
    }

    bool dequeue(CmdEntry& out) {
        if (_estop.get(out)) return true;  // estop always first
        return _normal.get(out);
    }

    void clear() { _estop.clear(); _normal.clear(); }
    bool empty() const { return _estop.empty() && _normal.empty(); }
    bool hasEstop() const { return !_estop.empty(); }

private:
    RingBuffer<CmdEntry, 1>  _estop;   // single slot, highest priority
    RingBuffer<CmdEntry, 16> _normal;  // 16 slots, overwrites oldest
};

// ── Heartbeat Monitor ──────────────────────────────────────────────

class HeartbeatMonitor {
public:
    HeartbeatMonitor(uint32_t timeoutMs = 2000) : _timeout(timeoutMs) {}

    void feed() { _lastBeat = millis(); _active = true; }
    bool isTimeout() const { return _active && (millis() - _lastBeat > _timeout); }
    uint32_t age() const { return _active ? (millis() - _lastBeat) : UINT32_MAX; }
    bool isActive() const { return _active; }

private:
    uint32_t _timeout;
    uint32_t _lastBeat = 0;
    bool _active = false;
};

// ── Robot Control State Machine ────────────────────────────────────

enum RobotCtrlState : uint8_t {
    RSTATE_IDLE = 0,
    RSTATE_MOVING,
    RSTATE_ESTOP,
    RSTATE_ERROR,
    RSTATE_LOCKED,
};

inline const char* rstateName(RobotCtrlState s) {
    switch (s) {
        case RSTATE_IDLE:   return "IDLE";
        case RSTATE_MOVING: return "MOVING";
        case RSTATE_ESTOP:  return "E-STOP";
        case RSTATE_ERROR:  return "ERROR";
        case RSTATE_LOCKED: return "LOCKED";
        default:            return "???";
    }
}

class RobotStateMachine {
public:
    RobotStateMachine() : _state(RSTATE_IDLE), _prevState(RSTATE_IDLE) {}

    RobotCtrlState state() const { return _state; }
    RobotCtrlState prevState() const { return _prevState; }
    const char* stateName() const { return rstateName(_state); }

    bool transition(RobotCtrlState newState) {
        if (!isAllowed(_state, newState)) return false;
        _prevState = _state;
        _state = newState;
        return true;
    }

    void force(RobotCtrlState newState) {
        _prevState = _state;
        _state = newState;
    }

    bool canMove() const {
        return _state == RSTATE_IDLE || _state == RSTATE_MOVING;
    }

private:
    RobotCtrlState _state;
    RobotCtrlState _prevState;

    static bool isAllowed(RobotCtrlState from, RobotCtrlState to) {
        if (to == RSTATE_ESTOP) return true;       // any→ESTOP
        if (from == to) return true;
        if (from == RSTATE_ESTOP) return to == RSTATE_IDLE;
        if (from == RSTATE_ERROR) return to == RSTATE_IDLE;
        if (from == RSTATE_LOCKED) return to == RSTATE_IDLE;
        if (from == RSTATE_IDLE && to == RSTATE_MOVING) return true;
        if (from == RSTATE_MOVING && to == RSTATE_IDLE) return true;
        return false;
    }
};
