// ESP32 Fairino Robot Client — WiFi UDP Command Server
//
// Hardware: ESP32-S3 + WS2812 LED
// Communicates with Fairino robot controller via UDP frame protocol (port 20007)
// Accepts commands from PC via UDP on port 20008 — no serial needed!

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "wifi_manager.h"
#include "stats.h"
#include "fairino_udp.h"

// ── Hardcoded WiFi ──────────────────────────────────────────────────
#define DEFAULT_WIFI_SSID     "Xiaomi_7D5E"
#define DEFAULT_WIFI_PASS     "12345678"
#define DEFAULT_STATIC_IP     "192.168.58.100"
#define DEFAULT_STATIC_GW     "192.168.58.1"
#define DEFAULT_STATIC_MASK   "255.255.255.0"

// ── UDP Command Server ──────────────────────────────────────────────
#define CMD_SERVER_PORT  20008
#define CMD_BUF_SIZE     512

static WiFiUDP s_cmdServer;

// ── LED (WS2812) ────────────────────────────────────────────────────
#ifndef PIN_WS2812
#define PIN_WS2812  17
#endif
#define WS2812_NUM  1
#include <Adafruit_NeoPixel.h>
static Adafruit_NeoPixel s_led(WS2812_NUM, PIN_WS2812, NEO_RGB + NEO_KHZ800);

// ── Fairino Client ──────────────────────────────────────────────────
static FairinoUDPClient s_fairino;

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

// ── Send UDP response ───────────────────────────────────────────────
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

// ── Command processor ───────────────────────────────────────────────
static void processCmd(const String& line) {
    if (line == "help") {
        cmdRespond("help status test 'servo start' 'servo end' 'servo j1 ...' 'servo j1wave [cycles] [deg/s]'\r\n");
        return;
    }

    if (line == "status") {
        cmdRespondF("WiFi:%s IP:%s RSSI:%d Robot:%s\r\n",
                    wifiMgrConnected() ? "OK" : "---",
                    wifiMgrLocalIP().c_str(),
                    wifiMgrRssi(),
                    s_fairino.isReady() ? "ready" : "offline");
        return;
    }

    if (line == "test") {
        if (!wifiMgrConnected()) { cmdRespond("ERR: no WiFi\r\n"); return; }
        ledSet(255, 255, 0, 32);
        s_fairino.servoTimingTest();
        cmdRespond("OK: timing test sent\r\n");
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

    // servo j1 <j1> <j2> <j3> <j4> <j5> <j6> [delta_deg] [duration_s]
    if (line.startsWith("servo j1 ")) {
        if (!wifiMgrConnected()) { cmdRespond("ERR: no WiFi\r\n"); return; }
        float joints[6];
        float delta = 2.0f;
        float duration = 3.0f;
        int n = sscanf(line.c_str(), "servo j1 %f %f %f %f %f %f %f %f",
                       &joints[0], &joints[1], &joints[2], &joints[3], &joints[4], &joints[5],
                       &delta, &duration);
        if (n < 6) {
            cmdRespond("Usage: servo j1 <j1> <j2> <j3> <j4> <j5> <j6> [delta=2] [duration_s=3]\r\n");
            return;
        }

        float durationS = (n >= 8) ? duration : 3.0f;
        float stepDeg = (n >= 7) ? delta : 2.0f;
        int steps = (int)(durationS / 0.020f);
        if (steps < 5) steps = 5;

        cmdRespondF("ROTATING J1 by %.1f deg over %.1fs (%d steps)\r\n",
                    stepDeg, durationS, steps);

        ledSet(255, 255, 0, 64);

        // ServoJ does NOT need ServoMoveStart/End — bare ServoJ works
        float startJ1 = joints[0];
        for (int i = 1; i <= steps; i++) {
            float t = (float)i / (float)steps;
            float curJ1 = startJ1 + stepDeg * t;
            int r = s_fairino.servoJ(curJ1, joints[1], joints[2], joints[3], joints[4], joints[5],
                                      0.1, 0.1, 0.05, 0, 0);
            if (r != FR_OK) {
                cmdRespondF("ERR at step %d/%d\r\n", i, steps);
                return;
            }
            delay(20);
        }

        ledSet(0, 255, 0, 32);
        cmdRespond("OK: J1 rotation complete\r\n");
        return;
    }

    // servo j1wave [cycles=3] [deg_per_sec=10]
    //   Reciprocates J1 between -60° and 0°, holding other joints constant
    if (line.startsWith("servo j1wave")) {
        if (!wifiMgrConnected()) { cmdRespond("ERR: no WiFi\r\n"); return; }

        // User's current joint angles (fixed reference)
        const float fixedJoints[6] = {
            -59.143f, -57.948f, -102.472f, -106.385f, 117.306f, -12.648f
        };

        const float J1_LOW  = -60.0f;   // target: sweep low
        const float J1_HIGH =   0.0f;   // target: sweep high
        const float RANGE   = J1_HIGH - J1_LOW;  // 60°

        int   cycles     = 3;
        float degPerSec  = 10.0f;

        int n = sscanf(line.c_str(), "servo j1wave %d %f", &cycles, &degPerSec);
        if (n >= 2 && cycles < 1)  cycles = 1;
        if (cycles > 100) cycles = 100;  // safety cap
        if (n >= 3 && degPerSec < 1.0f) degPerSec = 1.0f;
        if (degPerSec > 100.0f) degPerSec = 100.0f;

        float stepDeg   = 2.0f;
        int   intervalMs = (int)(stepDeg / degPerSec * 1000.0f);
        if (intervalMs < 20) intervalMs = 20;

        int stepsPerHalf = (int)(RANGE / stepDeg);
        int totalSteps   = cycles * 2 * stepsPerHalf;

        cmdRespondF("J1WAVE: %d cycles, %.0f°/s (step=%.0f° every %dms, %d steps half, %d total)\r\n",
                    cycles, degPerSec, stepDeg, intervalMs, stepsPerHalf, totalSteps);

        ledSet(255, 255, 0, 64);

        // ServoJ standalone — no ServoMoveStart/End needed
        float j1Cur = J1_LOW;  // start from low end
        int stepCount = 0;
        bool aborted = false;

        for (int cyc = 0; cyc < cycles && !aborted; cyc++) {
            // ── Forward: -60 → 0 ──
            for (int s = 0; s < stepsPerHalf; s++) {
                float t = (float)(s + 1) / (float)stepsPerHalf;
                j1Cur = J1_LOW + RANGE * t;
                int r = s_fairino.servoJ(j1Cur,
                    fixedJoints[1], fixedJoints[2], fixedJoints[3],
                    fixedJoints[4], fixedJoints[5],
                    0.1, 0.1, 0.05, 0, 0);
                if (r != FR_OK) {
                    cmdRespondF("ERR: forward step %d (cyc %d) J1=%.1f\r\n", s, cyc+1, j1Cur);
                    aborted = true;
                    break;
                }
                stepCount++;
                delay(intervalMs);
            }
            if (aborted) break;

            // ── Reverse: 0 → -60 ──
            for (int s = 0; s < stepsPerHalf; s++) {
                float t = (float)(s + 1) / (float)stepsPerHalf;
                j1Cur = J1_HIGH - RANGE * t;
                int r = s_fairino.servoJ(j1Cur,
                    fixedJoints[1], fixedJoints[2], fixedJoints[3],
                    fixedJoints[4], fixedJoints[5],
                    0.1, 0.1, 0.05, 0, 0);
                if (r != FR_OK) {
                    cmdRespondF("ERR: reverse step %d (cyc %d) J1=%.1f\r\n", s, cyc+1, j1Cur);
                    aborted = true;
                    break;
                }
                stepCount++;
                delay(intervalMs);
            }

            // Brief pause between cycles
            if (!aborted && cyc < cycles - 1) {
                delay(200);
            }
        }

        ledSet(0, 255, 0, 32);
        cmdRespondF("OK: J1 wave done — %d steps sent, J1 end=%.1f\r\n", stepCount, j1Cur);
        return;
    }

    cmdRespondF("Unknown: '%s'\r\n", line.c_str());
}

// ── Setup ───────────────────────────────────────────────────────────
void setup() {
    // 1. Serial (best-effort, may not be visible)
    Serial.begin(115200);
    uint32_t serStart = millis();
    while (!Serial && (millis() - serStart < 3000)) { delay(10); }
    delay(100);
    Serial.println();
    Serial.println("=== ESP32 Fairino Client ===");

    // 2. LED
    s_led.begin();
    ledSet(255, 0, 0, 32);   // Red = booting

    // 3. WiFi
    wifiMgrInit();
    wifiCredClear();
    wifiMgrConnectStatic(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS,
                         DEFAULT_STATIC_IP, DEFAULT_STATIC_GW, DEFAULT_STATIC_MASK);
    Serial.println("[MAIN] WiFi connecting...");

    // 4. Fairino UDP client
    s_fairino.begin();
    s_fairino.setTarget(FR_ROBOT_DEFAULT_IP, FR_ROBOT_UDP_PORT);

    // 5. Ready
    Serial.println("[MAIN] Setup done. UDP cmd server on port 20008.");
}

// ── Loop ────────────────────────────────────────────────────────────
void loop() {
    static uint32_t lastLed  = 0;
    static bool     wasConn  = false;
    static bool     cmdBound  = false;
    uint32_t now = millis();

    // 1. WiFi state machine
    wifiMgrTick();

    // 2. Bind UDP cmd server once WiFi is up
    if (wifiMgrConnected() && !cmdBound) {
        s_cmdServer.begin(CMD_SERVER_PORT);
        cmdBound = true;
        Serial.printf("[MAIN] UDP cmd server listening on port %d\n", CMD_SERVER_PORT);
    }

    // 3. Process incoming UDP commands
    if (cmdBound) {
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

    // 4. LED update (500ms)
    if (now - lastLed > 500) {
        lastLed = now;
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
            ledBreath(0, 255, 0);  // Green = WiFi OK
        }
    }

    // 5. WiFi state change logging
    bool nowConn = wifiMgrConnected();
    if (nowConn != wasConn) {
        if (nowConn) {
            Serial.printf("[MAIN] WiFi ONLINE — IP: %s\n", wifiMgrLocalIP().c_str());
        } else {
            Serial.println("[MAIN] WiFi OFFLINE");
        }
        wasConn = nowConn;
    }

    delay(10);
}
