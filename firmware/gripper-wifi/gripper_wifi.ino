/*
 * Thirdhand 夹爪控制器 / Gripper Controller
 * ESP32-S3 + SCServo + UDP Text Protocol
 *
 * 风格对齐 p4-controller.ino
 *
 * 依赖 / Dependencies:
 *   - ESP32 Arduino Core (board: ESP32S3 Dev Module)
 *   - SCServo library (Arduino Library Manager: "SCServo")
 *
 * 硬件连接 / Wiring:
 *   ESP32 GPIO17 (TX) → Bus Adapter RX
 *   ESP32 GPIO18 (RX) → Bus Adapter TX
 *   ESP32 GND         → Bus Adapter GND
 *   舵机 7.4V 独立供电 → Bus Adapter 电源口
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <SCServo.h>
#include "servo_config.h"

// ===== 网络配置 / Network Config =====
#define WIFI_SSID       "ZTE-P5cS5Y"
#define WIFI_PASS       "12345678"
#define STATIC_IP       "192.168.58.101"
#define STATIC_GW       "192.168.58.1"
#define STATIC_MASK     "255.255.255.0"

#define UDP_PORT        20009
#define UDP_BUF_SIZE    256

// ===== 全局对象 / Globals =====
SMS_STS st;
WiFiUDP udpServer;
IPAddress proxyIP;
uint16_t proxyPort = 0;
bool udpBound = false;

// ===== 状态 / State =====
int  g_pos = 0;
int  g_load = 0;
int  g_moving = 0;
bool g_grasped = false;         // 夹取完成标志, 禁止close
unsigned long g_lastStatus = 0;
unsigned long g_lastHeartbeat = 0;
bool g_heartbeatActive = false;

// ===== 辅助函数 / Helpers =====

float posToMM(int pos) {
  return 84.0 * (1.0 - (float)pos / POS_CLOSE);
}

int readServoLoad() {
  // readWord(ID, 60) = Python read2ByteTxRx(id, SMS_STS_PRESENT_LOAD_L)
  // 同地址、同原始值，阈值1320直接兼容
  return st.readWord(SERVO_ID, 60);
}

void updateStatus() {
  g_pos = st.ReadPos(SERVO_ID);
  g_load = readServoLoad();
  g_moving = st.ReadMove(SERVO_ID);
}

void moveServo(int target) {
  target = constrain(target, POS_OPEN, POS_CLOSE);
  st.WritePosEx(SERVO_ID, target, SERVO_SPEED, SERVO_ACC);
}

// ===== UDP 响应 / UDP Response =====
// 风格对齐 p4-controller.ino 的 cmdRespond()

void udpRespond(const char* msg) {
  Serial.print(msg);
  if (proxyPort > 0) {
    udpServer.beginPacket(proxyIP, proxyPort);
    udpServer.write((const uint8_t*)msg, strlen(msg));
    udpServer.endPacket();
  }
}

void udpRespondF(const char* fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  udpRespond(buf);
}

// ===== 夹取检测 / Grip Detection =====
// 移植自 Python gripper_grasp.py

void doGrip() {
  int startPos = st.ReadPos(SERVO_ID);
  Serial.printf("[GRIP] start pos=%d (%.1fmm)\n", startPos, posToMM(startPos));

  st.WritePosEx(SERVO_ID, POS_CLOSE, SERVO_SPEED, SERVO_ACC);

  unsigned long startMs = millis();
  bool grasped = false;
  int finalPos = startPos;
  int finalLoad = 0;
  int maxLoad = 0;

  while (true) {
    unsigned long elapsed = millis() - startMs;

    int pos = st.ReadPos(SERVO_ID);
    int moving = st.ReadMove(SERVO_ID);
    int load = readServoLoad();

    if (load > maxLoad) maxLoad = load;

    int dist = abs(POS_CLOSE - pos);
    bool hasMoved = abs(pos - startPos) > MIN_MOVEMENT;

    // 负载超阈值 = 夹到物体
    if (hasMoved && load > GRIP_LOAD_THRESHOLD && dist > GRIP_POS_THRESHOLD) {
      st.WritePosEx(SERVO_ID, pos, SERVO_SPEED, SERVO_ACC);
      grasped = true;
      finalPos = pos;
      finalLoad = load;
      Serial.printf("[GRIP] detected! load=%d > %d\n", load, GRIP_LOAD_THRESHOLD);
      break;
    }

    // 到达目标
    if (moving == 0 && hasMoved && dist <= GRIP_POS_THRESHOLD) {
      grasped = false;
      finalPos = pos;
      finalLoad = maxLoad;
      break;
    }

    // 超时
    if (elapsed > GRIP_TIMEOUT_MS) {
      Serial.println("[GRIP] timeout!");
      finalPos = pos;
      finalLoad = maxLoad;
      break;
    }

    // 检查是否有中断命令 (open/close/estop)
    int pktSize = udpServer.parsePacket();
    if (pktSize > 0 && pktSize < 128) {
      char buf[128] = {0};
      int len = udpServer.read((uint8_t*)buf, 127);
      if (len > 0) {
        String cmd(buf);
        cmd.trim();
        if (cmd == "open" || cmd == "close" || cmd == "estop") {
          Serial.printf("[GRIP] interrupted by: %s\n", cmd.c_str());
          st.WritePosEx(SERVO_ID, pos, SERVO_SPEED, SERVO_ACC);
          grasped = false;
          finalPos = pos;
          finalLoad = maxLoad;
          if (cmd == "open") moveServo(POS_OPEN);
          else if (cmd == "close") moveServo(POS_CLOSE);
          break;
        }
      }
    }

    delay(GRIP_POLL_MS);
  }

  // 更新状态
  g_pos = finalPos;
  g_load = finalLoad;
  g_moving = 0;

  // 回包
  if (grasped) {
    g_grasped = true;  // 禁止close, 必须先open
    udpRespondF("GRASPED:%d,%.1f,%d\r\n", finalPos, posToMM(finalPos), finalLoad);
  } else {
    udpRespondF("OK: closed pos=%d load=%d\r\n", finalPos, finalLoad);
  }
}

// ===== 命令解析 / Command Parser =====
// 风格对齐 p4-controller.ino 的 processCmd()

void processCmd(String line) {
  line.trim();
  if (line.length() == 0) return;

  // --- open ---
  if (line == "open") {
    g_grasped = false;  // 清除夹取锁定
    moveServo(POS_OPEN);
    delay(50);
    updateStatus();
    udpRespondF("OK: open pos=%d\r\n", g_pos);
    return;
  }

  // --- close ---
  if (line == "close") {
    if (g_grasped) {
      udpRespond("ERR: grasped, open first\r\n");
      return;
    }
    moveServo(POS_CLOSE);
    delay(50);
    updateStatus();
    udpRespondF("OK: close pos=%d\r\n", g_pos);
    return;
  }

  // --- grip (夹取检测) ---
  if (line == "grip") {
    doGrip();
    return;
  }

  // --- heartbeat ---
  if (line == "heartbeat") {
    g_heartbeatActive = true;
    g_lastHeartbeat = millis();
    udpRespond("OK: heartbeat\r\n");
    return;
  }

  // --- status ---
  if (line == "status") {
    updateStatus();
    udpRespondF("GRIP:%d,%d,%d,%.1f,%d\r\n",
                g_moving ? 2 : (g_pos < 100 ? 0 : 1),
                g_pos, g_load, posToMM(g_pos), g_moving);
    return;
  }

  // --- pos <0-3800> ---
  if (line.startsWith("pos ")) {
    int target = line.substring(4).toInt();
    moveServo(target);
    delay(50);
    updateStatus();
    udpRespondF("OK: pos=%d\r\n", g_pos);
    return;
  }

  // --- help ---
  if (line == "help") {
    udpRespond("OK: open|close|grip|heartbeat|status|pos <0-3800>|help\r\n");
    return;
  }

  // Unknown
  udpRespondF("ERR: unknown [%s]\r\n", line.c_str());
}

// ===== 初始化 / Setup =====
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Thirdhand Gripper Controller ===");

  // 舵机串口
  Serial1.begin(SERVO_BAUDRATE, SERIAL_8N1, SERVO_RX_PIN, SERVO_TX_PIN);
  st.pSerial = &Serial1;
  delay(500);
  Serial.printf("[SERVO] ID:%d test read: %d\n", SERVO_ID, st.ReadPos(SERVO_ID));

  // 力矩限制 / Torque limit (address 34, 0-1000, 500=50%)
  st.writeWord(SERVO_ID, 34, TORQUE_LIMIT);
  Serial.printf("[SERVO] Torque limit set to %d\n", TORQUE_LIMIT);

  // WiFi 静态 IP
  WiFi.mode(WIFI_STA);
  IPAddress ip, gw, mask;
  ip.fromString(STATIC_IP);
  gw.fromString(STATIC_GW);
  mask.fromString(STATIC_MASK);
  WiFi.config(ip, gw, mask);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.printf("[WiFi] connecting to %s ...\n", WIFI_SSID);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] FAILED - will retry in loop");
  }
}

// ===== 主循环 / Loop =====
void loop() {
  // WiFi 断线重连
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 10000) {
      lastReconnect = millis();
      Serial.println("[WiFi] reconnecting...");
      WiFi.reconnect();
    }
    delay(100);
    return;
  }

  // UDP 绑定（WiFi 连上后一次性）
  if (!udpBound) {
    udpServer.begin(UDP_PORT);
    udpBound = true;
    Serial.printf("[UDP] listening on port %d\n", UDP_PORT);
  }

  // UDP 收包
  int pktSize = udpServer.parsePacket();
  if (pktSize > 0 && pktSize < UDP_BUF_SIZE) {
    IPAddress senderIP = udpServer.remoteIP();
    uint16_t senderPort = udpServer.remotePort();
    char buf[UDP_BUF_SIZE] = {0};
    int len = udpServer.read((uint8_t*)buf, UDP_BUF_SIZE - 1);
    if (len > 0) {
      String line(buf);
      line.trim();
      // 只处理已知命令，过滤网络噪声 / Filter network noise
      bool known = (line == "open" || line == "close" || line == "grip"
                 || line == "heartbeat" || line == "status" || line == "help"
                 || line.startsWith("pos "));
      if (known) {
        proxyIP = senderIP;
        proxyPort = senderPort;
        Serial.printf("[UDP←%s:%d] %s\n", proxyIP.toString().c_str(), proxyPort, line.c_str());
        processCmd(line);
      }
    }
  }

  // 心跳超时检测
  if (g_heartbeatActive && (millis() - g_lastHeartbeat > 3000)) {
    g_heartbeatActive = false;
    Serial.println("[HB] heartbeat lost");
  }

  // 定时状态广播（500ms）
  if (millis() - g_lastStatus > 500 && proxyPort > 0) {
    g_lastStatus = millis();
    updateStatus();

    // 过载保护: 静止时负载超阈值 → 自动张开
    if (g_moving == 0 && g_pos > 100 && g_load > OVERLOAD_THRESHOLD) {
      Serial.printf("[OVERLOAD] load=%d > %d, auto open!\n", g_load, OVERLOAD_THRESHOLD);
      moveServo(POS_OPEN);
      udpRespondF("OVERLOAD:%d\r\n", g_load);
      delay(100);
      updateStatus();
    }

    int st = g_moving ? 2 : (g_pos < 100 ? 0 : (g_pos > POS_CLOSE - 100 ? 1 : 2));
    char buf[128];
    snprintf(buf, sizeof(buf), "GRIP:%d,%d,%d,%.1f,%d\r\n", st, g_pos, g_load, posToMM(g_pos), g_moving);
    udpServer.beginPacket(proxyIP, proxyPort);
    udpServer.write((const uint8_t*)buf, strlen(buf));
    udpServer.endPacket();
  }

  delay(2);
}
