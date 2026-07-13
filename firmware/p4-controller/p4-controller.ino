// ESP32-P4 Fairino Robot Client — UDP Command Server + Self-Test
//
// Hardware: ESP32-P4 (JC8012P4A1) + WS2812 LED + BOOT button
// Communicates with Fairino robot controller via UDP frame protocol (port 20007)
// Accepts commands from PC via UDP on port 20008
// BOOT button triggers self-test sequence (test1→test8→home)
//
// ServoJ parameters follow official SDK: acc=0, vel=0, cmdT=1.0s
// CNDE protocol (port 20005) for real-time state feedback

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <lvgl.h>
#include <Preferences.h>

#include "pins_config.h"
#include "config.h"
#include "wifi_manager.h"
#include "fairino_udp.h"
#include "cnde_client.h"
#include "cmd_queue.h"
#include "exoskeleton_state.h"

#if ENABLE_DISPLAY
#include "hw/display.h"
#endif
#if ENABLE_TOUCH
#include "hw/input.h"
#endif

#include "ui/ui_core.h"
#include "ui/ui_dashboard.h"
#include "ui/ui_control.h"
#include "ui/ui_exoskeleton.h"
#include "ui/ui_system.h"
#include "ui/ui_wifi.h"

// ── UDP Command Server ──────────────────────────────────────────────
#define CMD_SERVER_PORT  20008
#define CMD_BUF_SIZE     512

static WiFiUDP s_cmdServer;
static IPAddress s_proxyIP;
static uint16_t s_proxyPort = 0;
static IPAddress s_exoIP;
static uint16_t s_exoPort = 0;
static bool s_exoControlEnabled = false;
static bool s_exoServoActive = false;
static uint32_t s_exoLastPacketMs = 0;
static uint32_t s_exoLastServoMs = 0;
static uint32_t s_exoLastTargetUpdateMs = 0;
static float s_exoAcceptedTarget[6];
static bool s_exoCommandInitialized = false;
static ExoskeletonTelemetry s_exoTelemetry;
static bool s_lvglReady = false;

// ── LED (WS2812) ────────────────────────────────────────────────────
#include <Adafruit_NeoPixel.h>
static Adafruit_NeoPixel s_led(1, PIN_WS2812, NEO_RGB + NEO_KHZ800);

// ── Fairino Client ──────────────────────────────────────────────────
FairinoUDPClient g_fairino;
CNDEClient g_cnde;
static TaskHandle_t s_cndeTaskHandle = nullptr;

static void cndeNetworkTask(void* parameter) {
    (void)parameter;
    Serial.printf("[CNDE] network task started on core %d\n", xPortGetCoreID());
    for (;;) {
        if (wifiMgrConnected()) {
            g_cnde.tick();
            vTaskDelay(pdMS_TO_TICKS(2));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// ── Command Queue + Heartbeat + State Machine ────────────────────────
CmdQueue          g_cmdQueue;
HeartbeatMonitor  g_heartbeat(2000);  // 2s timeout
RobotStateMachine g_state;

static float normalizeJointAngle(float angle) {
    while (angle > 180.0f) angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}

ExoskeletonTelemetry exoskeletonGetTelemetry() {
    return s_exoTelemetry;
}

bool exoskeletonControlEnabled() {
    return s_exoControlEnabled;
}

bool exoskeletonSetControlEnabled(bool enabled) {
    if (enabled) {
        if (!wifiMgrConnected() || !g_state.canMove()) {
            Serial.println("[EXO] control enable rejected: WiFi or robot not ready");
            return false;
        }
        s_exoControlEnabled = true;
        Serial.println("[EXO] latched follow enabled; waiting for sensor data");
        return true;
    }

    bool wasEnabled = s_exoControlEnabled;
    s_exoControlEnabled = false;
    if (wasEnabled) {
        g_cmdQueue.clear();
        if (s_exoServoActive) g_fairino.servoMoveEnd();
        s_exoServoActive = false;
        s_exoCommandInitialized = false;
        s_exoLastServoMs = 0;
        s_exoLastTargetUpdateMs = 0;
        if (g_state.state() == RSTATE_MOVING) g_state.transition(RSTATE_IDLE);
        Serial.println("[EXO] latched follow disabled; ServoJ ended");
    }
    return false;
}

bool exoskeletonCalibrateZero() {
    if (!s_exoTelemetry.valid ||
        millis() - s_exoTelemetry.lastUpdate > EXO_PACKET_TIMEOUT_MS) return false;

    for (int i = 0; i < 6; ++i) {
        s_exoTelemetry.zeroOffsets[i] = s_exoTelemetry.angles[i];
        s_exoTelemetry.robotTargets[i] = 0.0f;
    }
    s_exoTelemetry.calibrated = true;

    Preferences preferences;
    if (preferences.begin("exo-cal", false)) {
        preferences.putBytes("zero", s_exoTelemetry.zeroOffsets,
                             sizeof(s_exoTelemetry.zeroOffsets));
        preferences.putBool("valid", true);
        preferences.end();
    }
    Serial.println("[EXO] current pose calibrated as robot 0 degrees");
    return true;
}

static void loadExoskeletonCalibration() {
    Preferences preferences;
    if (!preferences.begin("exo-cal", true)) return;
    bool valid = preferences.getBool("valid", false);
    if (valid && preferences.getBytesLength("zero") == sizeof(s_exoTelemetry.zeroOffsets)) {
        preferences.getBytes("zero", s_exoTelemetry.zeroOffsets,
                             sizeof(s_exoTelemetry.zeroOffsets));
        s_exoTelemetry.calibrated = true;
        Serial.println("[EXO] saved zero calibration loaded");
    }
    preferences.end();
}

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

// ── Command source tracking ────────────────────────────────────────
static bool cmdFromSerial = false;

// ── Send response to PC (UDP + Serial) ─────────────────────────────
static void cmdRespond(const char* msg) {
    // Always echo to Serial (proxy reads this)
    Serial.print(msg);
    // Also send via UDP
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
    g_fairino.servoJ(t[0], t[1], t[2], t[3], t[4], t[5],
                     SELF_TEST_ACC, SELF_TEST_VEL, SELF_TEST_CMDT, 0, 0);
}

// ── Self-test state machine (non-blocking, called from loop) ────────
static unsigned long stLastSend = 0;

static void selfTestTick() {
    // Abort self-test on E-STOP
    if (stState == ST_MOVE || stState == ST_SETTLE) {
        if (g_state.state() == RSTATE_ESTOP) {
            g_fairino.servoMoveEnd();
            stState = ST_ERROR;
            Serial.println("[SELF-TEST] Aborted by E-STOP");
            ledSet(255, 0, 0, 64);
            return;
        }
    }

    unsigned long now = millis();

    switch (stState) {
    case ST_IDLE:
    case ST_DONE:
    case ST_ERROR:
        return;

    case ST_MOVE:
        // Send ServoJ at interval, then switch to settle after one cycle
        if (now - stLastSend >= (unsigned long)(SELF_TEST_CMDT * 1000)) {
            selfTestSendTarget();
            stLastSend = now;
            stState = ST_SETTLE;
            stSettleStart = now;
        }
        break;

    case ST_SETTLE:
        // Keep sending target periodically during settle so robot arrives
        if (now - stLastSend >= (unsigned long)(SELF_TEST_CMDT * 1000)) {
            selfTestSendTarget();
            stLastSend = now;
        }
        if (now - stSettleStart >= (unsigned long)SELF_TEST_SETTLE_MS) {
            stSegment++;
            if (stSegment >= SELF_TEST_COUNT) {
                g_fairino.servoMoveEnd();
                stState = ST_DONE;
                ledSet(0, 0, 0, 0);
                Serial.println("[SELF-TEST] Done");
                return;
            }
            stState = ST_MOVE;
        }
        break;
    }

    // Overall timeout
    if (now - stStartTime > SELF_TEST_TIMEOUT) {
        g_fairino.servoMoveEnd();
        stState = ST_ERROR;
        Serial.println("[SELF-TEST] TIMEOUT");
        ledSet(255, 0, 0, 64);
    }
}

// ── Start self-test ─────────────────────────────────────────────────
void selfTestStart() {
    if (stState == ST_MOVE || stState == ST_SETTLE) {
        Serial.println("[SELF-TEST] Already running");
        return;
    }
    if (!wifiMgrConnected()) {
        Serial.println("[SELF-TEST] No WiFi — cannot start");
        return;
    }
    Serial.printf("[SELF-TEST] Starting %d positions\n", SELF_TEST_COUNT);

    g_fairino.servoMoveStart();
    delay(50);

    stSegment = 0;
    stState = ST_MOVE;
    stStartTime = millis();
    stLastSend = 0;  // send first target immediately
    ledSet(255, 255, 0, 64);
    Serial.println("[SELF-TEST] ServoMoveStart sent");
}

// ── Command processor ───────────────────────────────────────────────
static void processCmd(const String& line) {
    // ── Immediate commands (not queued) ──────────────────────────
    if (line == "help") {
        cmdRespond("help | status | test | selftest | servo start | servo end | servo j1 <j1-j6> | exo enable | exo disable | estop | reset | heartbeat\r\n");
        return;
    }

    if (line == "status") {
        const RobotStateData rs = g_cnde.getState();
        cmdRespondF("state:%s hb:%lu ",
                    g_state.stateName(), g_heartbeat.age());
        if (rs.valid) {
            cmdRespondF("J1:%.1f J2:%.1f J3:%.1f J4:%.1f J5:%.1f J6:%.1f robot:%d prog:%d err:%d/%d\r\n",
                        rs.jointPos[0], rs.jointPos[1], rs.jointPos[2],
                        rs.jointPos[3], rs.jointPos[4], rs.jointPos[5],
                        rs.robotState, rs.programState, rs.mainCode, rs.subCode);
        } else {
            cmdRespondF("WiFi:%s CNDE:%s SelfTest:%d\r\n",
                        wifiMgrConnected() ? "OK" : "---",
                        g_cnde.isConnected() ? "OK" : "---",
                        stState);
        }
        return;
    }

    // heartbeat — feed heartbeat monitor
    if (line == "heartbeat") {
        g_heartbeat.feed();
        cmdRespond("OK: heartbeat\r\n");
        return;
    }

    if (line == "exo enable") {
        if (exoskeletonSetControlEnabled(true)) {
            cmdRespond("OK: exoskeleton control enabled\r\n");
        } else {
            cmdRespond("ERR: exoskeleton sensor, WiFi, or robot not ready\r\n");
        }
        return;
    }

    if (line == "exo disable") {
        exoskeletonSetControlEnabled(false);
        cmdRespond("OK: exoskeleton control disabled\r\n");
        return;
    }

    // ── E-STOP: IMMEDIATE — bypass queue, official SDK StopMotion ─
    if (line == "estop") {
        g_cmdQueue.clear();                   // Discard all pending commands
        g_fairino.stopMotion();               // Official SDK: cmdID=102 "STOP" — immediate brake
        g_fairino.servoMoveEnd();             // Also end servo streaming (cmdID=690)
        s_exoServoActive = false;
        s_exoControlEnabled = false;
        g_state.force(RSTATE_ESTOP);          // State → E-STOP NOW
        ledSet(255, 0, 0, 64);               // Red LED
        cmdRespond("OK: E-STOP active (immediate, queue cleared)\r\n");
        Serial.println("[MAIN] E-STOP triggered — queue cleared, stopMotion+servoEnd sent");
        return;
    }

    // ── Queued commands ──────────────────────────────────────────

    // reset — reset ESTOP/ERROR/LOCKED → IDLE
    if (line == "reset") {
        if (g_state.state() == RSTATE_ESTOP || g_state.state() == RSTATE_ERROR || g_state.state() == RSTATE_LOCKED) {
            g_state.transition(RSTATE_IDLE);
            cmdRespondF("OK: reset → %s\r\n", g_state.stateName());
        } else {
            cmdRespondF("ERR: can't reset from %s\r\n", g_state.stateName());
        }
        return;
    }

    // test (timing test)
    if (line == "test") {
        if (!wifiMgrConnected()) { cmdRespond("ERR: no WiFi\r\n"); return; }
        ledSet(255, 255, 0, 32);
        g_fairino.servoTimingTest();
        cmdRespond("OK: timing test sent\r\n");
        return;
    }

    // selftest
    if (line == "selftest") {
        selfTestStart();
        cmdRespond("OK: self-test started\r\n");
        return;
    }

    // servo start — enqueue
    if (line == "servo start") {
        if (!g_state.canMove()) {
            cmdRespondF("ERR: can't move in state %s\r\n", g_state.stateName());
            return;
        }
        if (!wifiMgrConnected()) { cmdRespond("ERR: no WiFi\r\n"); return; }
        CmdEntry e; e.type = CMD_SERVO_START; e.ts = millis();
        g_cmdQueue.enqueue(e);
        cmdRespond("OK: servo start queued\r\n");
        return;
    }

    // servo end — enqueue
    if (line == "servo end") {
        CmdEntry e; e.type = CMD_SERVO_END; e.ts = millis();
        g_cmdQueue.enqueue(e);
        cmdRespond("OK: servo end queued\r\n");
        return;
    }

    // servo j1 <j1> <j2> <j3> <j4> <j5> <j6> — enqueue
    if (line.startsWith("servo j1 ")) {
        if (!g_state.canMove()) {
            cmdRespondF("ERR: can't move in state %s\r\n", g_state.stateName());
            return;
        }
        if (!wifiMgrConnected()) { cmdRespond("ERR: no WiFi\r\n"); return; }
        float joints[6];
        int n = sscanf(line.c_str(), "servo j1 %f %f %f %f %f %f",
                       &joints[0], &joints[1], &joints[2], &joints[3], &joints[4], &joints[5]);
        if (n < 6) {
            cmdRespond("Usage: servo j1 <j1> <j2> <j3> <j4> <j5> <j6>\r\n");
            return;
        }
        CmdEntry e; e.type = CMD_SERVO_MOVE; e.ts = millis();
        memcpy(e.joints, joints, sizeof(joints));
        g_cmdQueue.enqueue(e);
        ledSet(0, 255, 0, 32);
        return;  // response sent after execution
    }

    cmdRespondF("Unknown: [%s]\r\n", line.c_str());
}

// EXO:sequence,a1..a6,mv1..mv6
static void processExoskeletonPacket(const String& line) {
    char data[CMD_BUF_SIZE];
    line.substring(4).toCharArray(data, sizeof(data));

    float values[13];
    int count = 0;
    char* save = nullptr;
    for (char* token = strtok_r(data, ",", &save); token && count < 13;
         token = strtok_r(nullptr, ",", &save)) {
        char* end = nullptr;
        values[count] = strtof(token, &end);
        if (end == token || *end != '\0' || !isfinite(values[count])) return;
        ++count;
    }
    if (count != 13) return;

    for (int i = 0; i < 6; ++i) {
        if (values[i + 1] < -180.0f || values[i + 1] > 180.0f) return;
        if (values[i + 7] < 0.0f || values[i + 7] > 3600.0f) return;
    }

    s_exoLastPacketMs = millis();
    s_exoTelemetry.sequence = (uint32_t)values[0];
    s_exoTelemetry.lastUpdate = s_exoLastPacketMs;
    s_exoTelemetry.valid = true;
    for (int i = 0; i < 6; ++i) {
        s_exoTelemetry.angles[i] = values[i + 1];
        s_exoTelemetry.millivolts[i] = values[i + 7];
        s_exoTelemetry.robotTargets[i] = s_exoTelemetry.calibrated
            ? normalizeJointAngle(values[i + 1] - s_exoTelemetry.zeroOffsets[i])
            : values[i + 1];
    }

    char ack[48];
    snprintf(ack, sizeof(ack), "EXO_ACK:%lu,%d", (unsigned long)values[0],
             s_exoControlEnabled ? 1 : 0);
    s_cmdServer.beginPacket(s_exoIP, s_exoPort);
    s_cmdServer.write((const uint8_t*)ack, strlen(ack));
    s_cmdServer.endPacket();

    // Forward telemetry to the Node proxy without mixing it with CNDE feedback.
    if (s_proxyPort > 0) {
        char json[384];
        snprintf(json, sizeof(json),
                 "{\"type\":\"exoskeleton_state\",\"seq\":%lu,"
                 "\"angles\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],"
                 "\"millivolts\":[%.0f,%.0f,%.0f,%.0f,%.0f,%.0f],"
                 "\"controlEnabled\":%s,\"source\":\"p4\",\"ts\":%lu}",
                 (unsigned long)values[0],
                 values[1], values[2], values[3], values[4], values[5], values[6],
                 values[7], values[8], values[9], values[10], values[11], values[12],
                 s_exoControlEnabled ? "true" : "false", (unsigned long)millis());
        s_cmdServer.beginPacket(s_proxyIP, s_proxyPort);
        s_cmdServer.write((const uint8_t*)json, strlen(json));
        s_cmdServer.endPacket();
    }

    if (!s_exoControlEnabled || !g_state.canMove() || !wifiMgrConnected()) return;

    if (!s_exoServoActive) {
        const RobotStateData robotState = g_cnde.getState();
        if (!robotState.valid) {
            Serial.println("[EXO] waiting for valid robot joint feedback");
            return;
        }
        s_exoServoActive = true;
        s_exoCommandInitialized = true;
        s_exoLastServoMs = millis() - EXO_COMMAND_INTERVAL_MS;
        s_exoLastTargetUpdateMs = millis();
        for (int i = 0; i < 6; ++i) {
            s_exoAcceptedTarget[i] = robotState.jointPos[i];
        }
        g_state.transition(RSTATE_MOVING);
        Serial.println("[EXO] standalone 2-second ServoJ follow enabled");
    }
}

static void exoskeletonServoTick(uint32_t now) {
    if (!s_exoControlEnabled || !s_exoServoActive || !s_exoCommandInitialized ||
        !wifiMgrConnected() || !g_state.canMove()) return;
    if (now - s_exoLastPacketMs > EXO_PACKET_TIMEOUT_MS) return;
    if (now - s_exoLastTargetUpdateMs >= EXO_TARGET_UPDATE_INTERVAL_MS) {
        s_exoLastTargetUpdateMs = now;
        for (int i = 0; i < 6; ++i) {
            const float targetChange =
                s_exoTelemetry.robotTargets[i] - s_exoAcceptedTarget[i];
            if (fabsf(targetChange) >= EXO_TARGET_DEADBAND_DEG) {
                s_exoAcceptedTarget[i] = s_exoTelemetry.robotTargets[i];
            }
        }
    }

    if (now - s_exoLastServoMs < EXO_COMMAND_INTERVAL_MS) return;
    s_exoLastServoMs = now;

    int result = g_fairino.servoJ(
        s_exoAcceptedTarget[0], s_exoAcceptedTarget[1], s_exoAcceptedTarget[2],
        s_exoAcceptedTarget[3], s_exoAcceptedTarget[4], s_exoAcceptedTarget[5],
        SELF_TEST_ACC, SELF_TEST_VEL, EXO_SERVO_CMDT, 0, 0);
    if (result != FR_OK) {
        Serial.printf("[EXO] smooth ServoJ failed: %d\n", result);
        g_fairino.servoMoveEnd();
        s_exoServoActive = false;
        s_exoCommandInitialized = false;
        g_state.transition(RSTATE_ERROR);
    }
}


// --- LVGL periodic UI refresh timer callback -------------------------
static void uiRefreshTimer(lv_timer_t *timer) {
    ui_refresh_all();
}

// --- LVGL WiFi scan poll timer callback ------------------------------
static void uiWifiScanTimer(lv_timer_t *timer) {
    ui_wifi_setup_poll_scan();
}
// ── Setup ───────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    uint32_t serStart = millis();
    while (!Serial && (millis() - serStart < 3000)) { delay(10); }
    delay(100);
    Serial.println("\n=== ESP32 Fairino Client ===");
    loadExoskeletonCalibration();

    // Button
    pinMode(BOOT_BUTTON, INPUT_PULLUP);

    // LED
    s_led.begin();
    ledSet(255, 0, 0, 32);   // Red = booting

    // --- LVGL Display & Touch ---
#if ENABLE_DISPLAY
    if (hwDisplayInit()) {
        s_lvglReady = hwDisplayInitLVGL();
        if (s_lvglReady) {
            Serial.println("[MAIN] LVGL display OK");
#if ENABLE_TOUCH
            hwInputInit();
#endif
            ui_core_init();
            lv_timer_create(uiRefreshTimer, 200, NULL);
            lv_timer_create(uiWifiScanTimer, 500, NULL);
            Serial.println("[MAIN] LVGL UI initialized");
        }
    } else {
        Serial.println("[MAIN] Display init FAILED - running headless");
    }
#else
    Serial.println("[MAIN] DISPLAY disabled - running headless");
#endif

    // WiFi
    wifiMgrInit();
    wifiMgrConnectStatic(WIFI_SSID, WIFI_PASS, STATIC_IP, STATIC_GW, STATIC_MASK);

    // Fairino UDP client
    g_fairino.begin();
    g_fairino.setTarget(ROBOT_IP, ROBOT_UDP_PORT);

    // CNDE state feedback client
    g_cnde.begin(ROBOT_IP, 20005);
    BaseType_t cndeTaskResult = xTaskCreatePinnedToCore(
        cndeNetworkTask, "cnde-net", 8192, nullptr, 2, &s_cndeTaskHandle, 0);
    if (cndeTaskResult != pdPASS) {
        Serial.println("[CNDE] ERROR: failed to create network task");
    }
}

// ── Loop ────────────────────────────────────────────────────────────
void loop() {
    static uint32_t lastLed   = 0;

    // --- 0. LVGL tick handler ---
    if (s_lvglReady) {
#if ENABLE_TOUCH
        hwInputUpdate();
#endif
        lv_timer_handler();
    }
    static uint32_t lastBeat  = 0;
    static bool     wasConn   = false;
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
            IPAddress remoteIP = s_cmdServer.remoteIP();
            uint16_t remotePort = s_cmdServer.remotePort();
            char buf[CMD_BUF_SIZE] = {0};
            int len = s_cmdServer.read((uint8_t*)buf, CMD_BUF_SIZE - 1);
            if (len > 0) {
                String line(buf);
                line.trim();
                if (line.length() > 0) {
                    if (line.startsWith("EXO:")) {
                        s_exoIP = remoteIP;
                        s_exoPort = remotePort;
                        processExoskeletonPacket(line);
                    } else {
                        s_proxyIP = remoteIP;
                        s_proxyPort = remotePort;
                        processCmd(line);
                    }
                }
            }
        }
    }

    // Pause stale motion but keep follow mode latched until the user turns it off.
    if (s_exoControlEnabled && s_exoLastPacketMs > 0 &&
        now - s_exoLastPacketMs > EXO_PACKET_TIMEOUT_MS) {
        if (s_exoServoActive) {
            g_cmdQueue.clear();
            g_fairino.servoMoveEnd();
            s_exoServoActive = false;
            s_exoCommandInitialized = false;
            s_exoLastServoMs = 0;
            s_exoLastTargetUpdateMs = 0;
            if (g_state.state() == RSTATE_MOVING) g_state.transition(RSTATE_IDLE);
            Serial.println("[EXO] packet timeout; follow paused, switch remains on");
        }
    }

    exoskeletonServoTick(now);

    // 3b. Process incoming Serial commands (from web proxy)
    if (Serial.available() > 0) {
        String serLine = Serial.readStringUntil(0x0A);
        serLine.trim();
        if (serLine.length() > 0) {
            Serial.printf("[SERIAL] cmd: %s\n", serLine.c_str());
            processCmd(serLine);
        }
    }

    // 4. Process queued commands (estop priority first)
    {
        CmdEntry e;
        if (g_cmdQueue.dequeue(e)) {
            switch (e.type) {
            case CMD_ESTOP: {
                g_cmdQueue.clear();                   // Discard remaining queued commands
                g_fairino.stopMotion();               // Official SDK: cmdID=102 "STOP"
                g_fairino.servoMoveEnd();             // Also end servo streaming
                s_exoServoActive = false;
                s_exoControlEnabled = false;
                g_state.force(RSTATE_ESTOP);
                cmdRespondF("OK: E-STOP active (state %s)\r\n", g_state.stateName());
                ledSet(255, 0, 0, 64);
                break;
            }
            case CMD_SERVO_MOVE:
            case CMD_EXO_MOVE: {
                if (g_state.canMove()) {
                    g_state.transition(RSTATE_MOVING);
                    float commandPeriod = e.type == CMD_EXO_MOVE
                        ? EXO_SERVO_CMDT : SELF_TEST_CMDT;
                    int r = g_fairino.servoJ(e.joints[0], e.joints[1], e.joints[2],
                                              e.joints[3], e.joints[4], e.joints[5],
                                              SELF_TEST_ACC, SELF_TEST_VEL, commandPeriod, 0, 0);
                    if (r != FR_OK) {
                        cmdRespondF("ERR: servoJ failed %d\r\n", r);
                        g_state.transition(RSTATE_ERROR);
                    } else if (e.type == CMD_SERVO_MOVE) {
                        cmdRespondF("OK: servo j1 → %.1f %.1f %.1f %.1f %.1f %.1f\r\n",
                                    e.joints[0], e.joints[1], e.joints[2],
                                    e.joints[3], e.joints[4], e.joints[5]);
                    }
                }
                break;
            }
            case CMD_SERVO_START: {
                if (g_state.canMove()) {
                    g_state.transition(RSTATE_MOVING);
                    g_fairino.servoMoveStart();
                    cmdRespond("OK: servo start\r\n");
                }
                break;
            }
            case CMD_SERVO_END: {
                g_fairino.servoMoveEnd();
                s_exoServoActive = false;
                if (g_state.state() == RSTATE_MOVING) g_state.transition(RSTATE_IDLE);
                cmdRespond("OK: servo end\r\n");
                break;
            }
            default: break;
            }
        }
    }

    // 4b. Heartbeat timeout → auto-estop (one-shot)
    static bool hbTimedOut = false;
    if (!s_exoControlEnabled && g_heartbeat.isTimeout() && !hbTimedOut) {
        hbTimedOut = true;
        g_cmdQueue.clear();                   // Discard pending commands
        g_fairino.stopMotion();               // Official SDK: cmdID=102 "STOP"
        g_fairino.servoMoveEnd();             // End servo streaming
        s_exoServoActive = false;
        s_exoControlEnabled = false;
        g_state.force(RSTATE_ESTOP);
        cmdRespond("ERR: heartbeat timeout → E-STOP\r\n");
        ledSet(255, 0, 0, 64);
        Serial.println("[MAIN] HEARTBEAT TIMEOUT — E-STOP!");
    }
    if ((!g_heartbeat.isTimeout() || s_exoControlEnabled) && hbTimedOut) {
        hbTimedOut = false;  // reset on next heartbeat
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

    // 8. CNDE data print + UDP broadcast (500ms)
    if (now - lastBeat >= 500) {
        lastBeat = now;
        const RobotStateData rs = g_cnde.getState();
        if (rs.valid) {
            Serial.printf("J1:%.1f J2:%.1f J3:%.1f J4:%.1f J5:%.1f J6:%.1f st:%s hb:%lu\n",
                          rs.jointPos[0], rs.jointPos[1], rs.jointPos[2],
                          rs.jointPos[3], rs.jointPos[4], rs.jointPos[5],
                          g_state.stateName(), g_heartbeat.age());
            // UDP broadcast to web proxy (include state + heartbeat age)
            if (wifiMgrConnected() && cmdBound && s_proxyPort > 0) {
                char buf[192];
                snprintf(buf, sizeof(buf), "JOINTS:%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%lu\n",
                         rs.jointPos[0], rs.jointPos[1], rs.jointPos[2],
                         rs.jointPos[3], rs.jointPos[4], rs.jointPos[5],
                         (int)g_state.state(), g_heartbeat.age());
                s_cmdServer.beginPacket(s_proxyIP, s_proxyPort);
                s_cmdServer.write((const uint8_t*)buf, strlen(buf));
                s_cmdServer.endPacket();
            }
        }
    }
}
