// ESP32 Fairino Robot Client — UDP Command Server + Self-Test
//
// Hardware: ESP32-S3 + WS2812 LED + BOOT button
// Communicates with Fairino robot controller via UDP frame protocol (port 20007)
// Accepts commands from PC via UDP on port 20008
// BOOT button triggers self-test sequence (test1→test8→home)
//
// ServoJ parameters follow official SDK: acc=0, vel=0, cmdT=1.0s
// CNDE protocol (port 20005) for real-time state feedback

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "config.h"
#include "wifi_manager.h"
#include "fairino_udp.h"
#include "cnde_client.h"

// ── UDP Command Server ──────────────────────────────────────────────
#define CMD_SERVER_PORT  20008
#define CMD_BUF_SIZE     512

static WiFiUDP s_cmdServer;

// ── LED (WS2812) ────────────────────────────────────────────────────
#include <Adafruit_NeoPixel.h>
static Adafruit_NeoPixel s_led(1, PIN_WS2812, NEO_RGB + NEO_KHZ800);

// ── Fairino Client ──────────────────────────────────────────────────
static FairinoUDPClient s_fairino;
static CNDEClient s_cnde;

// ── Self-test state machine ─────────────────────────────────────────
enum SelfTestState { ST_IDLE, ST_MOVE, ST_SETTLE, ST_DONE, ST_ERROR };
static SelfTestState stState = ST_IDLE;
static unsigned long stStartTime = 0;
static unsigned long stSettleStart = 0;
static int stSegment = -1;

static const float SELF_TEST_POS[][6] = {
    { 60.485f, -69.577f, -91.012f, -84.252f, 100.514f,  -8.943f},  // test1 (home)
    { 18.048f,-125.631f, -65.685f, -72.588f,   2.137f,  47.036f},  // test2
    { 17.968f, -87.820f,  -0.081f, -91.700f,  94.012f,  47.037f},  // test3
    { 18.853f,-121.856f, -55.031f,-104.126f, -73.387f,  47.030f},  // test4
    { 19.604f,-122.555f,  92.010f, -78.909f, -78.479f,  47.036f},  // test5
    { 23.043f,-114.471f,  54.353f,-120.587f, -89.061f,  47.036f},  // test6
    { 22.383f,-112.778f,  26.096f, -61.089f, -88.293f,  47.037f},  // test7
    { 22.385f,-110.832f,  35.234f, -60.983f, -88.319f,  47.037f},  // test8
    { 60.485f, -69.577f, -91.012f, -84.252f, 100.514f,  -8.943f},  // home=test1
};
static const int SELF_TEST_COUNT = sizeof(SELF_TEST_POS) / sizeof(SELF_TEST_POS[0]);

// ── LED helpers ─────────────────────────────────────────────────────
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

// ── Send UDP response to PC ─────────────────────────────────────────
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

// ── Self-test: send ServoJ to current target ───────────────────────
static void selfTestSendTarget() {
    if (stSegment < 0 || stSegment >= SELF_TEST_COUNT) return;
    const float* t = SELF_TEST_POS[stSegment];
    s_fairino.servoJ(t[0], t[1], t[2], t[3], t[4], t[5],
                     SELF_TEST_ACC, SELF_TEST_VEL, SELF_TEST_CMDT, 0, 0);
}

// ── Self-test state machine (called from loop) ──────────────────────
static void selfTestTick() {
    unsigned long now = millis();

    switch (stState) {
    case ST_IDLE:
        return;

    case ST_MOVE:
        // Send ServoJ to target, then enter settle
        selfTestSendTarget();
        delay((int)(SELF_TEST_CMDT * 1000));
        stState = ST_SETTLE;
        stSettleStart = now;
        Serial.printf("[SELF-TEST] → seg %d target sent\n", stSegment);
        break;

    case ST_SETTLE:
        // Keep sending target during settle so robot arrives
        selfTestSendTarget();
        delay((int)(SELF_TEST_CMDT * 1000));
        if (now - stSettleStart >= (unsigned long)SELF_TEST_SETTLE_MS) {
            stSegment++;
            if (stSegment >= SELF_TEST_COUNT) {
                s_fairino.servoMoveEnd();
                stState = ST_DONE;
                Serial.println("[SELF-TEST] DONE — ServoMoveEnd sent");
                ledSet(0, 0, 0, 0);
                return;
            }
            stState = ST_MOVE;
        }
        break;

    case ST_DONE:
    case ST_ERROR:
        break;
    }

    // Overall timeout
    if (stState == ST_MOVE || stState == ST_SETTLE) {
        if (now - stStartTime > SELF_TEST_TIMEOUT) {
            s_fairino.servoMoveEnd();
            stState = ST_ERROR;
            Serial.println("[SELF-TEST] TIMEOUT");
            ledSet(255, 0, 0, 64);
        }
    }
}

// ── Start self-test ─────────────────────────────────────────────────
static void selfTestStart() {
    if (stState == ST_MOVE || stState == ST_SETTLE) {
        Serial.println("[SELF-TEST] Already running");
        return;
    }
    if (!wifiMgrConnected()) {
        Serial.println("[SELF-TEST] No WiFi — cannot start");
        return;
    }
    Serial.printf("[SELF-TEST] Starting %d positions\n", SELF_TEST_COUNT);

    s_fairino.servoMoveStart();
    delay(50);

    stSegment = 0;
    stState = ST_MOVE;
    stStartTime = millis();
    ledSet(255, 255, 0, 64);
    Serial.println("[SELF-TEST] ServoMoveStart sent");
}

// ── Command processor ───────────────────────────────────────────────
static void processCmd(const String& line) {
    if (line == "help") {
        cmdRespond("help | status | test | selftest | servo start | servo end | servo j1 <j1-j6> [delta] [dur]\r\n");
        return;
    }

    if (line == "status") {
        const RobotStateData& rs = s_cnde.getState();
        if (rs.valid) {
            cmdRespondF("J:%.1f,%.1f,%.1f,%.1f,%.1f,%.1f robot:%d prog:%d err:%d/%d\r\n",
                        rs.jointPos[0], rs.jointPos[1], rs.jointPos[2],
                        rs.jointPos[3], rs.jointPos[4], rs.jointPos[5],
                        rs.robotState, rs.programState, rs.mainCode, rs.subCode);
        } else {
            cmdRespondF("WiFi:%s CNDE:%s SelfTest:%d\r\n",
                        wifiMgrConnected() ? "OK" : "---",
                        s_cnde.isConnected() ? "OK" : "---",
                        stState);
        }
        return;
    }

    if (line == "test") {
        if (!wifiMgrConnected()) { cmdRespond("ERR: no WiFi\r\n"); return; }
        ledSet(255, 255, 0, 32);
        s_fairino.servoTimingTest();
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
        s_fairino.servoMoveStart();
        cmdRespond("OK: servo start\r\n");
        return;
    }

    if (line == "servo end") {
        s_fairino.servoMoveEnd();
        cmdRespond("OK: servo end\r\n");
        return;
    }

    // servo j1 <j1> <j2> <j3> <j4> <j5> <j6>
    if (line.startsWith("servo j1 ")) {
        if (!wifiMgrConnected()) { cmdRespond("ERR: no WiFi\r\n"); return; }
        float joints[6];
        int n = sscanf(line.c_str(), "servo j1 %f %f %f %f %f %f",
                       &joints[0], &joints[1], &joints[2], &joints[3], &joints[4], &joints[5]);
        if (n < 6) {
            cmdRespond("Usage: servo j1 <j1> <j2> <j3> <j4> <j5> <j6>\r\n");
            return;
        }

        int r = s_fairino.servoJ(joints[0], joints[1], joints[2], joints[3], joints[4], joints[5],
                                  SELF_TEST_ACC, SELF_TEST_VEL, SELF_TEST_CMDT, 0, 0);
        if (r != FR_OK) {
            cmdRespondF("ERR: servoJ failed %d\r\n", r);
        } else {
            cmdRespondF("OK: servo j1 → %.1f %.1f %.1f %.1f %.1f %.1f\r\n",
                        joints[0], joints[1], joints[2], joints[3], joints[4], joints[5]);
        }
        ledSet(0, 255, 0, 32);
        return;
    }

    cmdRespondF("Unknown: '%s'\r\n", line.c_str());
}

// ── Setup ───────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    uint32_t serStart = millis();
    while (!Serial && (millis() - serStart < 3000)) { delay(10); }
    delay(100);
    Serial.println();
    Serial.println("=== ESP32 Fairino Client ===");

    // LED
    s_led.begin();
    ledSet(255, 0, 0, 32);   // Red = booting

    // WiFi
    wifiMgrInit();
    wifiMgrConnectStatic(WIFI_SSID, WIFI_PASS, STATIC_IP, STATIC_GW, STATIC_MASK);
    Serial.println("[MAIN] WiFi connecting...");

    // Fairino UDP client
    s_fairino.begin();
    s_fairino.setTarget(ROBOT_IP, ROBOT_UDP_PORT);

    // CNDE state feedback client
    s_cnde.begin(ROBOT_IP, 20005);

    // Ready
    Serial.println("[MAIN] Setup done. UDP cmd server on port 20008.");
    Serial.println("[MAIN] Press BOOT button or send 'selftest' via UDP to start self-test.");
}

// ── Loop ────────────────────────────────────────────────────────────
void loop() {
    static uint32_t lastLed   = 0;
    static uint32_t lastBeat  = 0;
    static bool     wasConn   = false;
    static bool     cmdBound  = false;
    static bool     bootArmed = true;
    uint32_t now = millis();

    // 1. WiFi state machine
    wifiMgrTick();

    // 1b. CNDE state feedback
    if (wifiMgrConnected()) s_cnde.tick();

    // 2. Bind UDP cmd server once WiFi is up
    if (wifiMgrConnected() && !cmdBound) {
        s_cmdServer.begin(CMD_SERVER_PORT);
        cmdBound = true;
        Serial.printf("[MAIN] UDP cmd server listening on port %d\n", CMD_SERVER_PORT);
    }

    // 3. Process incoming UDP commands (only when self-test not running)
    if (cmdBound && stState != ST_MOVE) {
        int pktSize = s_cmdServer.parsePacket();
        if (pktSize > 0 && pktSize < CMD_BUF_SIZE) {
            char buf[CMD_BUF_SIZE] = {0};
            int len = s_cmdServer.read((uint8_t*)buf, CMD_BUF_SIZE - 1);
            if (len > 0) {
                String line(buf);
                line.trim();
                if (line.length() > 0) {
                    processCmd(line);
                }
            }
        }
    }

    // 4. BOOT button → start self-test
    if (digitalRead(BOOT_BUTTON) == LOW) {
        if (bootArmed) {
            bootArmed = false;
            selfTestStart();
        }
    } else {
        bootArmed = true;
    }

    // 5. Self-test tick
    selfTestTick();

    // 6. LED update (500ms)
    if (now - lastLed > 500) {
        lastLed = now;
        if (stState == ST_MOVE || stState == ST_SETTLE) {
            ledSet(255, 255, 0, 64);  // Yellow = self-test running
        } else if (stState == ST_DONE) {
            ledSet(0, 0, 0, 0);       // Off = done
        } else if (stState == ST_ERROR) {
            ledBreath(255, 0, 0);     // Red breath = error
        } else {
            // Normal operation — show WiFi status
            WifiMgrState st = wifiMgrState();
            if (!wifiMgrConnected()) {
                switch (st) {
                    case WM_IDLE:         ledBreath(0, 0, 255);  break;
                    case WM_AUTO_CONNECT:
                    case WM_CONNECTING:   ledSet(255, 255, 0, 32); break;
                    case WM_FAIL:         ledBreath(255, 0, 0);   break;
                    default: break;
                }
            } else {
                ledBreath(0, 255, 0);  // Green = WiFi OK, idle
            }
        }
    }

    // 7. WiFi state change logging
    bool nowConn = wifiMgrConnected();
    if (nowConn != wasConn) {
        if (nowConn) {
            Serial.printf("[MAIN] WiFi ONLINE — IP: %s\n", wifiMgrLocalIP().c_str());
        } else {
            Serial.println("[MAIN] WiFi OFFLINE");
        }
        wasConn = nowConn;
    }

    // 8. Heartbeat (10s)
    if (now - lastBeat >= 10000) {
        lastBeat = now;
        const RobotStateData& rs = s_cnde.getState();
        if (rs.valid) {
            Serial.printf("[MAIN] up %lus heap %u J:%.1f,%.1f,%.1f,%.1f,%.1f,%.1f st:%d\n",
                          now / 1000, ESP.getFreeHeap(),
                          rs.jointPos[0], rs.jointPos[1], rs.jointPos[2],
                          rs.jointPos[3], rs.jointPos[4], rs.jointPos[5],
                          stState);
        } else {
            Serial.printf("[MAIN] up %lus heap %u WiFi:%s CNDE:%s st:%d\n",
                          now / 1000, ESP.getFreeHeap(),
                          wifiMgrConnected() ? "OK" : "---",
                          s_cnde.isConnected() ? "OK" : "---",
                          stState);
        }
    }
}
