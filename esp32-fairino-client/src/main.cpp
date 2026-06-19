// Fairino Robot Controller — DeskPet HW + Robot Control
// Hardware: Waveshare ESP32-S3-Touch-LCD-1.69 V2.1
//   ST7789V 240x280 SPI display  +  CST816T touch  +  QMI8658 IMU
//   WS2812 LED  +  PWM buzzer  +  18650 battery ADC
// Comm: WiFi static IP + UDP robot commands (port 20007) + CNDE state (TCP 20005)
//       Web server (port 80)  +  UDP cmd server (port 20008)

#include "hw/audio.h"
#include "hw/display.h"
#include "hw/imu.h"
#include "hw/input.h"
#include "hw/pins.h"
#include "hw/power.h"
#include "stats.h"
#include "config.h"
#include "wifi_manager.h"
#include "fairino_udp.h"
#include "cnde_client.h"
#include "web_server.h"
#include "ui/ui_core.h"
#include "ui/ui_wifi.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_timer.h>
#include <lvgl.h>

// ═══════════════════════════════════════════════════════════════════════
//  Stubs for excluded deskpet modules (build_src_filter skips them,
//  but ui_core and others may reference these symbols at link time)
// ═══════════════════════════════════════════════════════════════════════

bool buddyMode = true;
bool gifAvailable = false;
uint32_t _clkLastRead = 0;

int bleAvailable() { return 0; }
int bleRead() { return -1; }
void bleWrite(const uint8_t*, int) {}
bool bleConnected() { return false; }
bool bleSecure() { return false; }
void bleClearBonds() {}

void buddyInit() {}
void buddyTick(uint8_t) {}
void buddySetSpeciesIdx(uint8_t) {}
bool characterInit(const char*) { return false; }
void characterTick() {}
void characterClose() {}

// ═══════════════════════════════════════════════════════════════════════
//  BLE (WiFi provisioning)
// ═══════════════════════════════════════════════════════════════════════
#define SERVICE_UUID    "12345678-1234-1234-1234-123456789abc"
#define CHAR_INFO       "abcd0001-1234-1234-1234-123456789abc"
#define CHAR_WIFI_CFG   "abcd0002-1234-1234-1234-123456789abc"

static bool s_bleConnected = false;

class ConnCallback : public BLEServerCallbacks {
  void onConnect(BLEServer*) { s_bleConnected = true; }
  void onDisconnect(BLEServer*) { s_bleConnected = false; BLEDevice::startAdvertising(); }
};

// ═══════════════════════════════════════════════════════════════════════
//  LED (WS2812)
// ═══════════════════════════════════════════════════════════════════════
static Adafruit_NeoPixel s_led(WS2812_NUM, PIN_WS2812, NEO_RGB + NEO_KHZ800);

static void ledSet(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 32) {
  s_led.setBrightness(brightness);
  s_led.fill(s_led.Color(r, g, b));
  s_led.show();
}

static void ledBreath(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t now = millis();
  float phase = (now % 2000) / 2000.0f;
  uint8_t bri = 4 + (uint8_t)((sin(phase * 2 * PI) + 1) * 10);
  ledSet(r, g, b, bri);
}

// ═══════════════════════════════════════════════════════════════════════
//  Robot Globals (instantiated here, externed in fairino_udp.h / cnde_client.h)
// ═══════════════════════════════════════════════════════════════════════
FairinoUDPClient g_fairino;
CNDEClient g_cnde;

// ═══════════════════════════════════════════════════════════════════════
//  UDP Command Server (port 20008)
// ═══════════════════════════════════════════════════════════════════════
#define CMD_SERVER_PORT  20008
#define CMD_BUF_SIZE     512
static WiFiUDP s_cmdServer;

static void cmdRespond(const char* msg) {
  s_cmdServer.beginPacket();
  s_cmdServer.write((const uint8_t*)msg, strlen(msg));
  s_cmdServer.endPacket();
}

static void cmdRespondF(const char* fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  cmdRespond(buf);
}

// ═══════════════════════════════════════════════════════════════════════
//  Self-test State Machine
// ═══════════════════════════════════════════════════════════════════════
enum SelfTestState { ST_IDLE, ST_MOVE, ST_SETTLE, ST_DONE, ST_ERROR };
static SelfTestState stState = ST_IDLE;
static unsigned long stStartTime = 0;
static unsigned long stSettleStart = 0;
static int stSegment = -1;

static const float SELF_TEST_POS[][6] = {
  { 60.485f, -69.577f, -91.012f, -84.252f, 100.514f,  -8.943f},
  { 18.048f,-125.631f, -65.685f, -72.588f,   2.137f,  47.036f},
  { 17.968f, -87.820f,  -0.081f, -91.700f,  94.012f,  47.037f},
  { 18.853f,-121.856f, -55.031f,-104.126f, -73.387f,  47.030f},
  { 19.604f,-122.555f,  92.010f, -78.909f, -78.479f,  47.036f},
  { 23.043f,-114.471f,  54.353f,-120.587f, -89.061f,  47.036f},
  { 22.383f,-112.778f,  26.096f, -61.089f, -88.293f,  47.037f},
  { 22.385f,-110.832f,  35.234f, -60.983f, -88.319f,  47.037f},
  { 60.485f, -69.577f, -91.012f, -84.252f, 100.514f,  -8.943f},
};
static const int SELF_TEST_COUNT = sizeof(SELF_TEST_POS) / sizeof(SELF_TEST_POS[0]);

static void selfTestSendTarget() {
  if (stSegment < 0 || stSegment >= SELF_TEST_COUNT) return;
  const float* t = SELF_TEST_POS[stSegment];
  g_fairino.servoJ(t[0], t[1], t[2], t[3], t[4], t[5],
                   SELF_TEST_ACC, SELF_TEST_VEL, SELF_TEST_CMDT, 0, 0);
}

static void selfTestTick() {
  unsigned long now = millis();
  switch (stState) {
  case ST_IDLE: return;
  case ST_MOVE:
    selfTestSendTarget();
    delay((int)(SELF_TEST_CMDT * 1000));
    stState = ST_SETTLE;
    stSettleStart = now;
    break;
  case ST_SETTLE:
    selfTestSendTarget();
    delay((int)(SELF_TEST_CMDT * 1000));
    if (now - stSettleStart >= (unsigned long)SELF_TEST_SETTLE_MS) {
      stSegment++;
      if (stSegment >= SELF_TEST_COUNT) {
        g_fairino.servoMoveEnd();
        stState = ST_DONE;
        ledSet(0, 0, 0, 0);
        Serial.println("[SELF-TEST] DONE");
        webLog("SELF-TEST: DONE");
        return;
      }
      stState = ST_MOVE;
    }
    break;
  case ST_DONE: case ST_ERROR: break;
  }
  if (stState == ST_MOVE || stState == ST_SETTLE) {
    if (now - stStartTime > SELF_TEST_TIMEOUT) {
      g_fairino.servoMoveEnd();
      stState = ST_ERROR;
      Serial.println("[SELF-TEST] TIMEOUT");
      webLog("SELF-TEST: TIMEOUT");
      ledSet(255, 0, 0, 64);
    }
  }
}

// ── public entry point (called from ui_robot.cpp via extern) ──
void selfTestStart() {
  if (stState == ST_MOVE || stState == ST_SETTLE) {
    Serial.println("[SELF-TEST] Already running");
    return;
  }
  if (!wifiMgrConnected()) {
    Serial.println("[SELF-TEST] No WiFi");
    return;
  }
  Serial.printf("[SELF-TEST] Starting %d positions\n", SELF_TEST_COUNT);
  webLog("SELF-TEST: starting %d positions", SELF_TEST_COUNT);
  g_fairino.servoMoveStart();
  delay(50);
  stSegment = 0;
  stState = ST_MOVE;
  stStartTime = millis();
  ledSet(255, 255, 0, 64);
}

// ═══════════════════════════════════════════════════════════════════════
//  UDP Command Processor
// ═══════════════════════════════════════════════════════════════════════
static void processCmd(const String& line) {
  if (line == "help") {
    cmdRespond("help | status | test | selftest | servo start | servo end | servo j1 <j1-j6>\r\n");
    return;
  }
  if (line == "status") {
    const RobotStateData& rs = g_cnde.getState();
    if (rs.valid) {
      cmdRespondF("J1:%.1f J2:%.1f J3:%.1f J4:%.1f J5:%.1f J6:%.1f robot:%d prog:%d err:%d/%d\r\n",
                  rs.jointPos[0], rs.jointPos[1], rs.jointPos[2],
                  rs.jointPos[3], rs.jointPos[4], rs.jointPos[5],
                  rs.robotState, rs.programState, rs.mainCode, rs.subCode);
    } else {
      cmdRespondF("WiFi:%s CNDE:%s SelfTest:%d\r\n",
                  wifiMgrConnected() ? "OK" : "---",
                  g_cnde.isConnected() ? "OK" : "---", stState);
    }
    return;
  }
  if (line == "test") {
    if (!wifiMgrConnected()) { cmdRespond("ERR: no WiFi\r\n"); return; }
    ledSet(255, 255, 0, 32);
    g_fairino.servoTimingTest();
    cmdRespond("OK: timing test sent\r\n");
    return;
  }
  if (line == "selftest") {
    selfTestStart();
    cmdRespond("OK: self-test started\r\n");
    return;
  }
  if (line == "servo start") {
    if (!wifiMgrConnected()) { cmdRespond("ERR: no WiFi\r\n"); return; }
    g_fairino.servoMoveStart();
    cmdRespond("OK: servo start\r\n");
    return;
  }
  if (line == "servo end") {
    g_fairino.servoMoveEnd();
    cmdRespond("OK: servo end\r\n");
    return;
  }
  // "servo j1 <j1> <j2> <j3> <j4> <j5> <j6>"
  if (line.startsWith("servo j1 ")) {
    if (!wifiMgrConnected()) { cmdRespond("ERR: no WiFi\r\n"); return; }
    float joints[6];
    int n = sscanf(line.c_str(), "servo j1 %f %f %f %f %f %f",
                   &joints[0], &joints[1], &joints[2], &joints[3], &joints[4], &joints[5]);
    if (n < 6) { cmdRespond("Usage: servo j1 <j1> <j2> <j3> <j4> <j5> <j6>\r\n"); return; }
    int r = g_fairino.servoJ(joints[0], joints[1], joints[2], joints[3], joints[4], joints[5],
                              SELF_TEST_ACC, SELF_TEST_VEL, SELF_TEST_CMDT, 0, 0);
    if (r != FR_OK) { cmdRespondF("ERR: servoJ failed %d\r\n", r); }
    else { cmdRespondF("OK: servo j1 -> %.1f %.1f %.1f %.1f %.1f %.1f\r\n",
                        joints[0], joints[1], joints[2], joints[3], joints[4], joints[5]); }
    ledSet(0, 255, 0, 32);
    return;
  }
  cmdRespondF("Unknown: '%s'\r\n", line.c_str());
}

// ═══════════════════════════════════════════════════════════════════════
//  LVGL Tick (esp_timer, 2ms period → 500 Hz)
// ═══════════════════════════════════════════════════════════════════════
static void lvglTick(void* arg) { lv_tick_inc(2); }

// ═══════════════════════════════════════════════════════════════════════
//  LVGL Periodic Timers
// ═══════════════════════════════════════════════════════════════════════

// LED status refresh (500ms)
static void ledTimerCb(lv_timer_t* timer) {
  if (stState == ST_MOVE || stState == ST_SETTLE) {
    ledSet(255, 255, 0, 64);                     // self-test running: yellow
  } else if (stState == ST_DONE) {
    ledSet(0, 0, 0, 0);                           // self-test done: off
  } else if (stState == ST_ERROR) {
    ledBreath(255, 0, 0);                         // self-test error: red breath
  } else {
    if (!wifiMgrConnected()) {
      switch (wifiMgrState()) {
        case WM_IDLE:     ledBreath(0, 0, 255); break;   // blue breath: idle
        case WM_CONNECTING:
        case WM_AUTO_CONNECT: ledSet(255, 255, 0, 32); break;  // yellow: connecting
        case WM_FAIL:     ledBreath(255, 0, 0); break;   // red breath: fail
        default: break;
      }
    } else {
      ledBreath(0, 255, 0);                        // green breath: connected
    }
  }
}

// UI data refresh (200ms)
static void statusTimerCb(lv_timer_t* timer) { ui_refresh_all(); }

// CNDE debug serial print (500ms)
static void cndeDebugTimerCb(lv_timer_t* timer) {
  const RobotStateData& rs = g_cnde.getState();
  if (rs.valid) {
    Serial.printf("J1:%.1f J2:%.1f J3:%.1f J4:%.1f J5:%.1f J6:%.1f st:%d/%d\n",
                  rs.jointPos[0], rs.jointPos[1], rs.jointPos[2],
                  rs.jointPos[3], rs.jointPos[4], rs.jointPos[5],
                  rs.robotState, rs.programState);
  }
}

// ═══════════════════════════════════════════════════════════════════════
//  setup()
// ═══════════════════════════════════════════════════════════════════════
void setup() {
  // 1. Serial
  Serial.begin(115200);
  uint32_t serStart = millis();
  while (!Serial && (millis() - serStart < 3000)) { delay(10); }
  delay(100);
  Serial.println("\n=== ESP32 Fairino Robot Controller v2.0 ===");

  // 2. LED — show boot
  s_led.begin();
  ledSet(255, 0, 0, 32);  // red = booting

  // 3. Display + LVGL
  if (!hwDisplayInit()) {
    Serial.println("[BOOT] Display init FAILED");
    ledSet(255, 0, 0, 128);
    delay(2000);
  }
  if (!hwDisplayInitLVGL()) {
    Serial.println("[BOOT] LVGL init FAILED");
    ledSet(255, 0, 0, 128);
    delay(2000);
  }

  // 4. LVGL 2ms tick timer
  esp_timer_handle_t tickH = NULL;
  esp_timer_create_args_t targs = {};
  targs.callback = lvglTick;
  targs.name = "lvgl";
  esp_timer_create(&targs, &tickH);
  esp_timer_start_periodic(tickH, 2000);

  // 5. Touch
  hwInputInit();

  // 6. IMU
  if (hwImuInit()) { Serial.println("[BOOT] IMU: OK"); }
  else             { Serial.println("[BOOT] IMU: NOT FOUND"); }

  // 7. Buzzer
  if (hwAudioInit()) { hwBeep(880, 80); }
  else               { Serial.println("[BOOT] Audio: FAIL"); }

  // 8. Battery ADC
  hwPowerInit();

  // 9. NVS settings
  settingsLoad();
  wifiCredLoad();

  // 10. WiFi — static IP
  wifiMgrInit();
  if (!wifiCredHas()) {
    wifiCredAddTop(WIFI_SSID, WIFI_PASS);
  }
  wifiMgrReconnectStatic(STATIC_IP, STATIC_GW, STATIC_MASK);

  // 11. Fairino UDP + CNDE (non-blocking begin, will connect once WiFi is up)
  g_fairino.begin();
  g_fairino.setTarget(ROBOT_IP, ROBOT_UDP_PORT);
  Serial.printf("[BOOT] Fairino UDP target: %s:%d\n", ROBOT_IP, ROBOT_UDP_PORT);

  g_cnde.begin(ROBOT_IP, 20005);
  Serial.printf("[BOOT] CNDE TCP target: %s:20005\n", ROBOT_IP);

  // 12. BLE (WiFi provisioning)
  BLEDevice::init("Fairino-Mini");
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ConnCallback());
  BLEService* pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic* pInfoChar = pService->createCharacteristic(
      CHAR_INFO, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pInfoChar->setValue("{\"type\":\"fairino\",\"ver\":\"2.0\"}");
  BLECharacteristic* pWifiChar = pService->createCharacteristic(
      CHAR_WIFI_CFG, BLECharacteristic::PROPERTY_WRITE);
  BLE2902* pDesc = new BLE2902();
  pInfoChar->addDescriptor(pDesc);
  pService->start();
  BLEAdvertising* pAdv = BLEDevice::getAdvertising();
  pAdv->addServiceUUID(SERVICE_UUID);
  pAdv->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("[BOOT] BLE: Fairino-Mini advertising");

  // 13. Web server
  webServerInit();
  Serial.println("[BOOT] Web server on port 80");

  // 14. UI (5 swipeable screens: Home / Robot / Settings / WiFi / Melody)
  ui_core_init();
  ui_wifi_create();

  // 15. LVGL periodic timers
  lv_timer_create(ledTimerCb, 500, NULL);          // LED status
  lv_timer_create(statusTimerCb, 200, NULL);       // UI data refresh
  lv_timer_create(cndeDebugTimerCb, 500, NULL);    // CNDE debug print

  Serial.println("[BOOT] === Ready ===");
  ledSet(0, 0, 255, 32);  // blue = idle/ready
}

// ═══════════════════════════════════════════════════════════════════════
//  loop()
// ═══════════════════════════════════════════════════════════════════════
void loop() {
  static uint32_t lastImu = 0;
  static bool cmdBound = false;
  static bool bootArmed = true;
  static bool wasConn = false;
  uint32_t now = millis();

  // 1. LVGL heartbeat
  lv_timer_handler();

  // 2. WiFi state machine tick
  wifiMgrTick();

  // 3. Touch input
  hwInputUpdate();

  // 4. WiFi scan poll (async result delivery)
  ui_wifi_poll_scan();

  // 5. CNDE state feedback (only when WiFi is up)
  if (wifiMgrConnected()) g_cnde.tick();

  // 6. Bind UDP command server once WiFi connects
  if (wifiMgrConnected() && !cmdBound) {
    s_cmdServer.begin(CMD_SERVER_PORT);
    cmdBound = true;
    Serial.printf("[MAIN] UDP cmd server on port %d\n", CMD_SERVER_PORT);
  }

  // 7. Process incoming UDP commands (skip during self-test MOVEs to avoid conflicts)
  if (cmdBound && stState != ST_MOVE) {
    int pktSize = s_cmdServer.parsePacket();
    if (pktSize > 0 && pktSize < CMD_BUF_SIZE) {
      char buf[CMD_BUF_SIZE] = {0};
      int len = s_cmdServer.read((uint8_t*)buf, CMD_BUF_SIZE - 1);
      if (len > 0) {
        String line(buf);
        line.trim();
        if (line.length() > 0) processCmd(line);
      }
    }
  }

  // 8. BOOT button (GPIO0) — trigger self-test on press
  if (digitalRead(BOOT_BUTTON) == LOW) {
    if (bootArmed) { bootArmed = false; selfTestStart(); }
  } else { bootArmed = true; }

  // 9. Self-test state machine advance
  selfTestTick();

  // 10. IMU read (1 Hz)
  if (now - lastImu >= 1000) {
    lastImu = now;
    hwImuRead();
  }

  // 11. WiFi state change logging
  bool nowConn = wifiMgrConnected();
  if (nowConn != wasConn) {
    if (nowConn) {
      Serial.printf("[MAIN] WiFi ONLINE — IP: %s\n", wifiMgrLocalIP().c_str());
      webLog("WiFi online: %s", wifiMgrLocalIP().c_str());
    } else {
      Serial.println("[MAIN] WiFi OFFLINE");
      webLog("WiFi offline");
    }
    wasConn = nowConn;
  }

  delay(5);
}
