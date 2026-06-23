// CNDE Client — 实时状态回读（TCP port 20005）
// 参考文档: https://fairino-doc-zhs.readthedocs.io/latest/SDKManual/CPPRobotCnde.html
// 帧格式: 0x5A5A | count(1) | type(1) | len(2) | data | 0xA5A5
// 关节位置类型: float (4字节/个), 6个关节 = 24字节

#include "cnde_client.h"
#include "config.h"

// CNDE output config: period(2B, little-endian) + comma-separated field names
// 文档字段: JointCurPos → actual_joint_pos (float×6)
static const char* CNDE_FIELDS =
    "actual_joint_pos,robot_state,program_state,main_code,sub_code";

// ---- 连接 ----

void CNDEClient::begin(const char* ip, uint16_t port) {
    _ip = ip;
    _port = port;
    _connected = false;
    _configured = false;
    _frameCount = 0;
    memset(&_state, 0, sizeof(_state));
    Serial.printf("[CNDE] target %s:%u fields:\"%s\"\n", ip, port, CNDE_FIELDS);
}

// ---- 帧收发 ----

bool CNDEClient::sendFrame(uint8_t type, const uint8_t* data, uint16_t len) {
    if (!_tcp.connected()) return false;
    uint8_t hdr[6];
    hdr[0] = 0x5A; hdr[1] = 0x5A;           // sync head
    hdr[2] = _sendCount++;                    // count
    hdr[3] = type;                            // type
    hdr[4] = len & 0xFF;                      // len low
    hdr[5] = (len >> 8) & 0xFF;              // len high
    _tcp.write(hdr, 6);
    if (len > 0 && data) _tcp.write(data, len);
    uint8_t tail[2] = { 0xA5, 0xA5 };
    _tcp.write(tail, 2);
    return true;
}

bool CNDEClient::sendOutputConfig() {
    // data = period(2B) + field string
    size_t strLen = strlen(CNDE_FIELDS);
    uint8_t buf[2 + 128];
    buf[0] = 5;  buf[1] = 0;                 // period = 5ms (little-endian)
    memcpy(buf + 2, CNDE_FIELDS, strLen);
    bool ok = sendFrame(CNDE_TYPE_OUTPUT_CONFIG, buf, 2 + strLen);
    Serial.printf("[CNDE] config sent (%s)\n", ok ? "OK" : "FAIL");
    return ok;
}

bool CNDEClient::sendStart() {
    bool ok = sendFrame(CNDE_TYPE_START, nullptr, 0);
    Serial.printf("[CNDE] start sent (%s)\n", ok ? "OK" : "FAIL");
    return ok;
}

// 接收一个完整帧, 返回数据长度, -1=无完整帧, -2=帧尾错误
int CNDEClient::recvFrame(uint8_t* buf, int maxLen) {
    // 逐字节寻找帧头 0x5A5A
    while (_tcp.available() >= 6) {
        int b = _tcp.read();
        if (b != 0x5A) continue;

        int b2 = _tcp.peek();
        if (b2 != 0x5A) continue;
        _tcp.read(); // 消费第二个 0x5A

        // 读取 count(1) + type(1) + len(2)
        uint8_t hdr[4];
        if (_tcp.read(hdr, 4) < 4) return -1;

        uint16_t dataLen = hdr[2] | (hdr[3] << 8);
        int totalRemain = dataLen + 2;  // data + tail 0xA5A5

        if (6 + totalRemain > maxLen) return -1;
        if (_tcp.available() < totalRemain) return -1;

        // 组装完整帧
        buf[0] = 0x5A; buf[1] = 0x5A;
        buf[2] = hdr[0]; buf[3] = hdr[1];
        buf[4] = hdr[2]; buf[5] = hdr[3];
        _tcp.read(buf + 6, totalRemain);

        // 验证帧尾
        if (buf[6 + dataLen] != 0xA5 || buf[6 + dataLen + 1] != 0xA5) {
            return -2;
        }
        return dataLen;
    }
    return -1;
}

// ---- 解析状态数据 ----
// 数据格式与配置的字段顺序一致:
//   actual_joint_pos: 6 × float (24字节)
//   robot_state:      1 × uint8
//   program_state:    1 × uint8
//   main_code:        1 × int32 (4字节)
//   sub_code:         1 × int32 (4字节)
//   总计: 32字节

void CNDEClient::parseStateData(const uint8_t* data, uint16_t len) {
    if (len < 48) {
        // 数据太短, 打印原始hex帮助调试
        Serial.printf("[CNDE] short(%u) ", len);
        for (int i = 0; i < min((int)len, 32); i++) Serial.printf("%02X", data[i]);
        Serial.printf("\n");
        return;
    }

    // 按 double 解析关节角度 (已验证是double格式)
    double joints[6];
    memcpy(joints, data, 48);

    // 状态字段 (offset 48: rs+ps+mainCode+subCode)
    if (len >= 56) {
        _state.robotState   = data[48];
        _state.programState = data[49];
        memcpy(&_state.mainCode, data + 50, 4);
        memcpy(&_state.subCode,  data + 54, 4);
    }

    for (int i = 0; i < 6; i++) _state.jointPos[i] = (float)joints[i];
    _state.valid = true;
    _state.lastUpdate = millis();

    // 原始hex + 角度 (每200帧打印一次)
    _frameCount++;
    if (_frameCount % 200 == 1) {
        Serial.printf("[CNDE] raw(%u) ", len);
        for (int i = 0; i < min((int)len, 48); i++) Serial.printf("%02X", data[i]);
        Serial.printf("\n");
        Serial.printf("[CNDE] J1:%.1f J2:%.1f J3:%.1f J4:%.1f J5:%.1f J6:%.1f rs:%u ps:%u\n",
                      joints[0], joints[1], joints[2],
                      joints[3], joints[4], joints[5],
                      _state.robotState, _state.programState);
    }
}

// ---- 主循环 ----

void CNDEClient::tick() {
    unsigned long now = millis();

    // 未连接时每3秒重连
    if (!_tcp.connected()) {
        _connected = false;
        _configured = false;
        if (now - _lastConnect >= 3000) {
            _lastConnect = now;
            Serial.printf("[CNDE] connecting %s:%u ...\n", _ip.c_str(), _port);
            if (_tcp.connect(_ip.c_str(), _port, 3000)) {
                _connected = true;
                Serial.println("[CNDE] connected OK");
                _lastRecv = now;
                // 先发 config, 再发 start
                sendOutputConfig();
                delay(50);
                sendStart();
                _configured = true;
            } else {
                Serial.println("[CNDE] connect FAIL");
                _tcp.stop();
            }
        }
        return;
    }

    // 每3秒打印连接状态
    static unsigned long lastDbg = 0;
    if (now - lastDbg >= 3000) {
        lastDbg = now;
        Serial.printf("[CNDE] frames:%u avail:%d\n", _frameCount, _tcp.available());
    }

    // 循环接收所有可用帧 (防止缓冲区积压)
    uint8_t buf[512];
    int processed = 0;
    while (_tcp.available() >= 8 && processed < 20) {
        int dataLen = recvFrame(buf, sizeof(buf));
        if (dataLen <= 0) break;
        processed++;
        uint8_t type = buf[3];
        _lastRecv = now;

        if (type == CNDE_TYPE_OUTPUT_STATE) {
            parseStateData(buf + 6, dataLen);
        }
    }

    // 连接超时 (10秒无数据)
    if (_connected && _configured && now - _lastRecv >= 10000) {
        Serial.println("[CNDE] timeout, reconnecting");
        _tcp.stop();
        _connected = false;
        _configured = false;
    }
}

bool CNDEClient::isConnected() {
    return _connected && _tcp.connected();
}
