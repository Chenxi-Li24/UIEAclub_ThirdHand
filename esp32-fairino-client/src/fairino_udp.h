#pragma once
// Fairino Robot UDP Frame Client (ESP32 Arduino)
// Implements the UDP text-frame protocol on port 20007
// Frame format: /f/bIII{count}III{cmdID}III{contentLen}III{content}III/b/f

#include <Arduino.h>
#include <WiFiUdp.h>

// ── Protocol Constants ────────────────────────────────────────────────
#define FR_HEAD          "/f/b"
#define FR_TAIL          "/b/f"
#define FR_DELIM         "III"

#define FR_ROBOT_UDP_PORT    20007
#define FR_ROBOT_DEFAULT_IP  "192.168.58.2"
#define FR_UDP_TIMEOUT_MS    500
#define FR_UDP_RECV_BUF      2048

// ── Command IDs (from robot.cpp) ──────────────────────────────────────
enum FrCmdID : int {
    CMD_SERVO_J           = 376,   // ServoJ(jointPos, axisPos, acc, vel, cmdT, filterT, gain, id)
    CMD_SERVO_MOVE_START  = 689,   // ServoMoveStart()
    CMD_SERVO_MOVE_END    = 690,   // ServoMoveEnd()
    CMD_SERVO_JT_START    = 1199,  // ServoJTStart()
    CMD_SERVO_JT          = 1200,  // ServoJT(torques, interval, checkFlag, powerLimit, velLimit)
    CMD_SERVO_JT_END      = 1201,  // ServoJTEnd()
};

// ── Error Codes ────────────────────────────────────────────────────────
#define FR_OK              0
#define FR_ERR_NOT_CONN   -1
#define FR_ERR_SEND_FAIL  -2
#define FR_ERR_TIMEOUT    -3
#define FR_ERR_BAD_FRAME  -4

class FairinoUDPClient {
public:
    void     begin();
    void     setTarget(const char* ip, uint16_t port = FR_ROBOT_UDP_PORT);
    bool     isReady() const { return _ready; }

    // Frame pack/unpack
    String   packFrame(int cmdID, const String& content);
    int      sendCommand(int cmdID, const String& content);
    int      recvResponse(String& content, uint32_t timeoutMs = FR_UDP_TIMEOUT_MS);

    // High-level wrapped commands
    int      servoMoveStart();
    int      servoMoveEnd();
    int      servoJ(float j1, float j2, float j3, float j4, float j5, float j6,
                    float acc = 0, float vel = 0, float cmdT = 0.008f,
                    float filterT = 0, float gain = 0);
    int      servoTimingTest();  // Start + immediate End — safe timing check

private:
    WiFiUDP  _udp;
    String   _ip;
    uint16_t _port  = FR_ROBOT_UDP_PORT;
    bool     _ready = false;
    int      _count = 0;    // frame sequence counter
};

// Global instance (defined in main.cpp)
extern FairinoUDPClient g_fairino;
