#pragma once
// CNDE Client — 实时状态回读（TCP port 20005）
// 法奥机器人CNDE协议：配置→开始→持续接收状态数据

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>

// CNDE frame types
#define CNDE_TYPE_OUTPUT_CONFIG  1
#define CNDE_TYPE_START          2
#define CNDE_TYPE_STOP           3
#define CNDE_TYPE_OUTPUT_STATE   4
#define CNDE_TYPE_MESSAGE        6

// Robot state data (received from CNDE)
struct RobotStateData {
    float jointPos[6];      // actual_joint_pos (degrees)
    float jointVel[6];      // actual_joint_vel (deg/s)
    float jointTorque[6];   // actual_joint_torque (Nm)
    float tcpPos[6];        // actual_TCP_pos (mm + deg)
    uint8_t robotState;     // 0=idle, 1=running
    uint8_t programState;   // 0=stopped, 1=running
    int32_t mainCode;       // error main code
    int32_t subCode;        // error sub code
    bool valid;             // data is valid
    unsigned long lastUpdate;
};

class CNDEClient {
public:
    void begin(const char* ip, uint16_t port = 20005);
    void tick();
    bool isConnected();
    const RobotStateData& getState() { return _state; }

private:
    WiFiClient _tcp;
    String _ip;
    uint16_t _port = 20005;
    bool _connected = false;
    bool _configured = false;
    unsigned long _lastConnect = 0;
    unsigned long _lastRecv = 0;
    uint8_t _sendCount = 0;
    RobotStateData _state = {};

    bool sendFrame(uint8_t type, const uint8_t* data, uint16_t len);
    bool sendOutputConfig();
    bool sendStart();
    void parseStateData(const uint8_t* data, uint16_t len);
    int recvFrame(uint8_t* buf, int maxLen);
};
