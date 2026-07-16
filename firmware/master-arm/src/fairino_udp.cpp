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

// ── XML-RPC (TCP port 20003) — Robot Enable / Mode ───────────────────
// RobotEnable(1) is REQUIRED before ServoJ. Without it, robot returns
// Lua errcode 204 for all servo commands.

int FairinoUDPClient::sendXmlRpc(const char* method, const char* paramXml) {
    String body;
    body += "<?xml version=\"1.0\"?>\r\n";
    body += "<methodCall><methodName>";
    body += method;
    body += "</methodName><params><param><value>";
    body += paramXml;
    body += "</value></param></params></methodCall>\r\n";

    String req;
    req += "POST /RPC2 HTTP/1.1\r\n";
    req += "Host: ";
    req += _ip;
    req += ":20003\r\n";
    req += "User-Agent: ESP32_Fairino\r\n";
    req += "Content-Type: text/xml\r\n";
    req += "Content-length: ";
    req += String(body.length());
    req += "\r\n\r\n";
    req += body;

    WiFiClient tcp;
    if (!tcp.connect(_ip.c_str(), 20003)) {
        Serial.printf("[FR-XML] connect to %s:20003 FAILED\n", _ip.c_str());
        return -1;
    }
    tcp.print(req);
    // Read response (non-blocking, short timeout)
    unsigned long t0 = millis();
    String resp;
    while (millis() - t0 < 2000) {
        while (tcp.available()) {
            char c = tcp.read();
            resp += c;
        }
        if (resp.indexOf("</methodResponse>") > 0) break;
        delay(10);
    }
    tcp.stop();

    // Check for success (<i4>0</i4> in response)
    if (resp.indexOf("<i4>0</i4>") > 0) {
        Serial.printf("[FR-XML] %s OK\n", method);
        return 0;
    }
    // Log failure
    int codeStart = resp.indexOf("<i4>");
    String code = "?";
    if (codeStart > 0) {
        int codeEnd = resp.indexOf("</i4>", codeStart);
        if (codeEnd > 0) code = resp.substring(codeStart + 4, codeEnd);
    }
    Serial.printf("[FR-XML] %s FAILED code=%s\n  resp: %s\n", method, code.c_str(), resp.c_str());
    return 1;
}

int FairinoUDPClient::robotEnable(uint8_t state) {
    const char* p = state ? "<i4>1</i4>" : "<i4>0</i4>";
    int r = sendXmlRpc("RobotEnable", p);
    if (r == 0) _robotEnabled = (state == 1);
    return r;
}

int FairinoUDPClient::mode(int m) {
    String p = "<i4>";
    p += String(m);
    p += "</i4>";
    return sendXmlRpc("Mode", p.c_str());
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

    // Retry up to 3 times with backoff on ENOMEM (TX buffer full)
    for (int attempt = 0; attempt < 3; attempt++) {
        if (!_udp.beginPacket(_ip.c_str(), _port)) {
            delay(10);
            continue;
        }
        _udp.write((const uint8_t*)frame.c_str(), frame.length());
        if (_udp.endPacket()) {
            static unsigned long lastOk = 0;
            if (millis() - lastOk > 2000) {
                Serial.printf("[FR-UDP] SEND OK count=%d cmdID=%d len=%d\n", _count-1, cmdID, frame.length());
                Serial.printf("  frame: %s\n", frame.c_str());
                lastOk = millis();
            }
            return FR_OK;
        }
        // endPacket failed — likely TX buffer full, wait and retry
        Serial.printf("[FR-UDP] endPacket fail attempt %d\n", attempt+1);
        delay(50);
    }
    return FR_ERR_SEND_FAIL;
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

// ── Immediate Stop (official SDK cmdID=102, content="STOP") ──────────
// This matches FRRobot::StopMotion() in the official Fairino C++ SDK:
//   sprintf(g_sendbuf, "/f/bIII44III102III4IIISTOPIII/b/f");

int FairinoUDPClient::stopMotion() {
    return sendCommand(102, "STOP");
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
