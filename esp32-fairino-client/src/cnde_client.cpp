#include "cnde_client.h"

// CNDE frame: 0x5A 0x5A | count(1) | type(1) | len(2) | data(len) | 0xA5 0xA5

bool CNDEClient::sendFrame(uint8_t type, const uint8_t* data, uint16_t len) {
    uint8_t header[6];
    header[0] = 0x5A;
    header[1] = 0x5A;
    header[2] = _sendCount++;
    header[3] = type;
    header[4] = len & 0xFF;
    header[5] = (len >> 8) & 0xFF;

    if (_tcp.write(header, 6) != 6) return false;
    if (len > 0 && _tcp.write(data, len) != len) return false;
    uint8_t tail[2] = {0xA5, 0xA5};
    if (_tcp.write(tail, 2) != 2) return false;
    _tcp.flush();
    return true;
}

bool CNDEClient::sendOutputConfig() {
    // Request: actual_joint_pos, robot_state, program_state, main_code, sub_code
    const char* states = "actual_joint_pos,robot_state,program_state,main_code,sub_code";
    uint16_t nameLen = strlen(states);
    uint16_t dataLen = nameLen + 2;

    uint8_t buf[256];
    buf[0] = 8;   // period = 8ms (low byte)
    buf[1] = 0;   // period (high byte)
    memcpy(buf + 2, states, nameLen);

    return sendFrame(CNDE_TYPE_OUTPUT_CONFIG, buf, dataLen);
}

bool CNDEClient::sendStart() {
    return sendFrame(CNDE_TYPE_START, nullptr, 0);
}

int CNDEClient::recvFrame(uint8_t* buf, int maxLen) {
    if (_tcp.available() < 6) return 0;

    // Sync to 0x5A 0x5A
    while (_tcp.available() >= 2) {
        if (_tcp.peek() != 0x5A) { _tcp.read(); continue; }
        _tcp.read();
        if (_tcp.available() < 1) return 0;
        if (_tcp.peek() != 0x5A) continue;
        _tcp.read();

        while (_tcp.available() < 4) { delay(1); }
        uint8_t hdr[4];
        _tcp.read(hdr, 4);
        uint16_t dataLen = hdr[2] | (hdr[3] << 8);

        if (dataLen > maxLen - 8) return -1;

        int need = dataLen + 2; // data + tail
        int got = 0;
        while (got < need) {
            int n = _tcp.read(buf + got, need - got);
            if (n > 0) got += n;
            else delay(1);
        }

        if (buf[dataLen] != 0xA5 || buf[dataLen + 1] != 0xA5) return -2;

        // Layout: count(1) type(1) dataLen(2) data(dataLen)
        memmove(buf + 4, buf, dataLen);
        buf[0] = hdr[0];
        buf[1] = hdr[1];
        buf[2] = dataLen & 0xFF;
        buf[3] = (dataLen >> 8) & 0xFF;
        return dataLen + 4;
    }
    return 0;
}

void CNDEClient::parseStateData(const uint8_t* data, uint16_t len) {
    int offset = 0;

    // actual_joint_pos: 6 x double (48 bytes)
    if (offset + 48 <= len) {
        for (int i = 0; i < 6; i++) {
            double val;
            memcpy(&val, data + offset, 8);
            _state.jointPos[i] = (float)val;
            offset += 8;
        }
    }

    // robot_state: uint8
    if (offset + 1 <= len) _state.robotState = data[offset++];

    // program_state: uint8
    if (offset + 1 <= len) _state.programState = data[offset++];

    // main_code: int32
    if (offset + 4 <= len) {
        memcpy(&_state.mainCode, data + offset, 4);
        offset += 4;
    }

    // sub_code: int32
    if (offset + 4 <= len) {
        memcpy(&_state.subCode, data + offset, 4);
        offset += 4;
    }

    _state.valid = true;
    _state.lastUpdate = millis();
}

void CNDEClient::begin(const char* ip, uint16_t port) {
    _ip = ip;
    _port = port;
    _connected = false;
    _configured = false;
    _state = {};
    Serial.printf("[CNDE] Target: %s:%d\n", ip, port);
}

void CNDEClient::tick() {
    unsigned long now = millis();

    if (!_connected) {
        if (now - _lastConnect < 5000) return;
        _lastConnect = now;
        Serial.printf("[CNDE] Connecting to %s:%d...\n", _ip.c_str(), _port);
        if (_tcp.connect(_ip.c_str(), _port)) {
            _connected = true;
            _configured = false;
            _tcp.setTimeout(100);
            Serial.println("[CNDE] Connected");
        } else {
            Serial.println("[CNDE] Connect failed");
            return;
        }
    }

    if (!_tcp.connected()) {
        Serial.println("[CNDE] Disconnected");
        _connected = false;
        _configured = false;
        _tcp.stop();
        _state.valid = false;
        return;
    }

    if (!_configured) {
        if (sendOutputConfig()) {
            Serial.println("[CNDE] Config sent");
            delay(100);
            if (sendStart()) {
                Serial.println("[CNDE] Start sent");
                _configured = true;
                _lastRecv = now;
            }
        }
    }

    if (_configured) {
        uint8_t buf[512];
        int ret = recvFrame(buf, sizeof(buf));
        if (ret > 4) {
            uint8_t type = buf[1];
            uint16_t dataLen = buf[2] | (buf[3] << 8);
            if (type == CNDE_TYPE_OUTPUT_STATE) {
                parseStateData(buf + 4, dataLen);
                _lastRecv = now;
            }
        }

        if (now - _lastRecv > 10000 && _state.valid) {
            Serial.println("[CNDE] State timeout");
            _state.valid = false;
        }
    }
}

bool CNDEClient::isConnected() {
    return _connected && _configured && _state.valid;
}
