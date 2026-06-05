#include "fairino_udp.h"

// ── Begin / Set Target ────────────────────────────────────────────────

void FairinoUDPClient::begin() {
    _udp.begin(0);       // bind to any available local port
    _ready = true;
    Serial.println("[FR-UDP] Client started (UDP port bound)");
}

void FairinoUDPClient::setTarget(const char* ip, uint16_t port) {
    _ip   = ip;
    _port = port;
    Serial.printf("[FR-UDP] Target set: %s:%d\n", _ip.c_str(), _port);
}

// ── Frame Packing ────────────────────────────────────────────────────
// Matches PackFrame() in FrameHandle.cpp exactly:
//   /f/bIII{count}III{cmdID}III{contentLen}III{content}III/b/f

String FairinoUDPClient::packFrame(int cmdID, const String& content) {
    String frame;
    frame.reserve(content.length() + 32);
    frame += FR_HEAD;
    frame += FR_DELIM;
    frame += String(_count);
    frame += FR_DELIM;
    frame += String(cmdID);
    frame += FR_DELIM;
    frame += String(content.length());
    frame += FR_DELIM;
    frame += content;
    frame += FR_DELIM;
    frame += FR_TAIL;
    return frame;
}

// ── Send Command ─────────────────────────────────────────────────────

int FairinoUDPClient::sendCommand(int cmdID, const String& content) {
    if (!_ready || _ip.length() == 0) {
        Serial.println("[FR-UDP] ERROR: not ready or no target IP");
        return FR_ERR_NOT_CONN;
    }

    String frame = packFrame(cmdID, content);
    _count++;

    if (!_udp.beginPacket(_ip.c_str(), _port)) {
        Serial.println("[FR-UDP] ERROR: beginPacket failed");
        return FR_ERR_SEND_FAIL;
    }

    size_t sent = _udp.write((const uint8_t*)frame.c_str(), frame.length());
    if (!_udp.endPacket()) {
        Serial.println("[FR-UDP] ERROR: endPacket failed");
        return FR_ERR_SEND_FAIL;
    }

    Serial.printf("[FR-UDP] SEND  count=%d cmdID=%d len=%d\n  frame: %s\n",
                  _count - 1, cmdID, content.length(), frame.c_str());
    return FR_OK;
}

// ── Receive Response ─────────────────────────────────────────────────

int FairinoUDPClient::recvResponse(String& content, uint32_t timeoutMs) {
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        int pktSize = _udp.parsePacket();
        if (pktSize <= 0) {
            delay(1);
            continue;
        }

        char buf[FR_UDP_RECV_BUF] = {0};
        int len = (pktSize < FR_UDP_RECV_BUF - 1) ? pktSize : FR_UDP_RECV_BUF - 1;
        int read = _udp.read((uint8_t*)buf, len);
        if (read > 0) {
            buf[read] = 0;
            content = String(buf);
            Serial.printf("[FR-UDP] RECV  %d bytes from %s:%d\n  data: %s\n",
                          read, _udp.remoteIP().toString().c_str(), _udp.remotePort(), content.c_str());
            return FR_OK;
        }
    }
    return FR_ERR_TIMEOUT;
}

// ── High-Level Commands ──────────────────────────────────────────────

int FairinoUDPClient::servoMoveStart() {
    return sendCommand(CMD_SERVO_MOVE_START, "ServoMoveStart()");
}

int FairinoUDPClient::servoMoveEnd() {
    return sendCommand(CMD_SERVO_MOVE_END, "ServoMoveEnd()");
}

int FairinoUDPClient::servoJ(float j1, float j2, float j3, float j4, float j5, float j6,
                              float acc, float vel, float cmdT, float filterT, float gain) {
    // Format: ServoJ({j1,j2,j3,j4,j5,j6},{0,0,0,0},acc,vel,cmdT,filterT,gain,0)
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "ServoJ({%.3f,%.3f,%.3f,%.3f,%.3f,%.3f},{0.000,0.000,0.000,0.000},"
             "%.6f,%.6f,%.6f,%.6f,%.6f,0)",
             j1, j2, j3, j4, j5, j6,
             (double)acc, (double)vel, (double)cmdT, (double)filterT, (double)gain);
    return sendCommand(CMD_SERVO_J, String(cmd));
}

// ── Safety: timing test — Start then immediately End ─────────────────

int FairinoUDPClient::servoTimingTest() {
    Serial.println("[FR-UDP] === Timing Test: Start → End ===");

    int r1 = servoMoveStart();
    if (r1 != FR_OK) {
        Serial.printf("[FR-UDP] ServoMoveStart FAILED: %d\n", r1);
        return r1;
    }

    // Minimal delay to let robot process the start command
    delay(50);

    int r2 = servoMoveEnd();
    if (r2 != FR_OK) {
        Serial.printf("[FR-UDP] ServoMoveEnd FAILED: %d\n", r2);
        return r2;
    }

    // Check for any response
    String resp;
    int r3 = recvResponse(resp, 200);
    if (r3 == FR_OK) {
        Serial.printf("[FR-UDP] Got response: %s\n", resp.c_str());
    } else {
        Serial.println("[FR-UDP] No response (timeout) — this may be normal if robot processes silently");
    }

    Serial.println("[FR-UDP] === Timing Test complete ===");
    return FR_OK;
}
