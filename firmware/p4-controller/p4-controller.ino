// ESP32-P4 Fairino Robot Client — UDP Command Server + Self-Test
//
// Hardware: ESP32-P4 (JC8012P4A1) + WS2812 LED + BOOT button
// Communicates with Fairino robot controller via UDP frame protocol (port 20007)
// Accepts commands from PC via UDP on port 20008
// BOOT button triggers self-test sequence (test1→test8→home)
//
// ServoJ follows the official UDP example: 16 ms streaming, small joint increments.
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
#include "safe_motion.h"
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
static uint32_t s_exoLastAppliedSequence = UINT32_MAX;
static float s_exoAcceptedTarget[6];
static bool s_exoTargetInitialized = false;
static uint32_t s_exoZeroCaptureStartedMs = 0;
static ExoskeletonTelemetry s_exoTelemetry;
static bool s_lvglReady = false;

// ── LED (WS2812) ────────────────────────────────────────────────────
#include <Adafruit_NeoPixel.h>
static Adafruit_NeoPixel s_led(1, PIN_WS2812, NEO_RGB + NEO_KHZ800);

// ── Fairino Client ──────────────────────────────────────────────────
FairinoUDPClient g_fairino;
SafeServoMotion g_safeMotion;
CNDEClient g_cnde;
static TaskHandle_t s_cndeTaskHandle = nullptr;
static TaskHandle_t s_motionTaskHandle = nullptr;
static volatile int s_motionTaskError = FR_OK;

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

static void safeMotionNetworkTask(void* parameter) {
    (void)parameter;
    Serial.printf("[SAFE-MOTION] network task started on core %d\n", xPortGetCoreID());
    for (;;) {
        if (g_safeMotion.active()) {
            const int result = g_safeMotion.tick(true);
            if (result != FR_OK) s_motionTaskError = result;
            // The official UDP example sleeps 15 ms after every ServoJ packet.
            vTaskDelay(pdMS_TO_TICKS(15));
        } else {
            vTaskDelay(pdMS_TO_TICKS(2));
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

static bool robotFeedbackFresh(const RobotStateData& state) {
    return state.valid && millis() - state.lastUpdate <= 500;
}

static void updateExoskeletonRobotTargets() {
    memset(s_exoTelemetry.robotTargets, 0, sizeof(s_exoTelemetry.robotTargets));
    if (s_exoTelemetry.calibrating) return;

    for (int channel = 0; channel < 6; ++channel) {
        const float relativeAngle = s_exoTelemetry.calibrated
            ? normalizeJointAngle(s_exoTelemetry.angles[channel] -
                                  s_exoTelemetry.zeroOffsets[channel])
            : s_exoTelemetry.angles[channel];
        const uint8_t robotJoint = EXOSKELETON_TO_ROBOT_JOINT[channel];
        s_exoTelemetry.robotTargets[robotJoint] =
            relativeAngle * s_exoTelemetry.directions[channel];
    }
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
        if (s_exoServoActive) g_safeMotion.stop();
        s_exoServoActive = false;
        s_exoTargetInitialized = false;
        s_exoLastAppliedSequence = UINT32_MAX;
        if (g_state.state() == RSTATE_MOVING) g_state.transition(RSTATE_IDLE);
        Serial.println("[EXO] latched follow disabled; ServoJ ended");
    }
    return false;
}

bool exoskeletonCalibrateZero() {
    if (!s_exoTelemetry.valid ||
        millis() - s_exoTelemetry.lastUpdate > EXO_PACKET_TIMEOUT_MS) return false;
    if (s_exoControlEnabled) exoskeletonSetControlEnabled(false);
    if (g_safeMotion.active()) {
        Serial.println("[EXO] relative zero rejected: other motion is active");
        return false;
    }

    s_exoZeroCaptureStartedMs = millis();
    s_exoTelemetry.calibrating = true;
    for (int i = 0; i < 6; ++i) {
        s_exoTelemetry.zeroOffsets[i] = s_exoTelemetry.angles[i];
        s_exoTelemetry.robotTargets[i] = 0.0f;
    }
    s_exoTelemetry.calibrated = true;
    Serial.printf("[EXO] relative zero capture started for %u ms; follow disabled\n",
                  (unsigned)EXO_ZERO_CAPTURE_MS);
    return true;
}

static void saveExoskeletonCalibration() {
    Preferences preferences;
    if (preferences.begin("exo-cal", false)) {
        preferences.putBytes("zero", s_exoTelemetry.zeroOffsets,
                             sizeof(s_exoTelemetry.zeroOffsets));
        preferences.putBool("valid", true);
        preferences.end();
    }
    Serial.println("[EXO] relative zero stabilized and saved; robot calibration unchanged");
}

static void saveExoskeletonDirections() {
    Preferences preferences;
    if (preferences.begin("exo-cal", false)) {
        preferences.putBytes("dir", s_exoTelemetry.directions,
                             sizeof(s_exoTelemetry.directions));
        preferences.end();
    }
}

bool exoskeletonSetDirection(uint8_t channel, int8_t direction) {
    if (channel >= 6 || (direction != 1 && direction != -1)) return false;
    if (s_exoControlEnabled || g_safeMotion.active()) {
        Serial.println("[EXO] direction change rejected: stop robot control first");
        return false;
    }

    s_exoTelemetry.directions[channel] = direction;
    updateExoskeletonRobotTargets();
    saveExoskeletonDirections();
    Serial.printf("[EXO] H%u -> J%u direction set to %+d\n",
                  channel + 1, EXOSKELETON_TO_ROBOT_JOINT[channel] + 1, direction);
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
    if (preferences.getBytesLength("dir") == sizeof(s_exoTelemetry.directions)) {
        int8_t savedDirections[6];
        preferences.getBytes("dir", savedDirections, sizeof(savedDirections));
        for (int i = 0; i < 6; ++i) {
            if (savedDirections[i] == 1 || savedDirections[i] == -1) {
                s_exoTelemetry.directions[i] = savedDirections[i];
            }
        }
        Serial.println("[EXO] saved channel directions loaded");
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
    const RobotStateData robotState = g_cnde.getState();
    if (!robotFeedbackFresh(robotState)) return;
    int result = g_safeMotion.setTarget(t, robotState.jointPos, false);
    if (result != FR_OK) {
        Serial.printf("[SELF-TEST] safe motion start failed: %d\n", result);
        stState = ST_ERROR;
    }
}

// ── Self-test state machine (non-blocking, called from loop) ────────
static void selfTestTick() {
    // Abort self-test on E-STOP
    if (stState == ST_MOVE || stState == ST_SETTLE) {
        if (g_state.state() == RSTATE_ESTOP) {
            g_safeMotion.stop();
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
        selfTestSendTarget();
        if (stState != ST_ERROR && g_safeMotion.active()) {
            g_state.transition(RSTATE_MOVING);
            stState = ST_SETTLE;
            stSettleStart = 0;
        }
        break;

    case ST_SETTLE:
        if (g_safeMotion.active()) break;
        if (stSettleStart == 0) stSettleStart = now;
        if (now - stSettleStart >= (unsigned long)SELF_TEST_SETTLE_MS) {
            stSegment++;
            if (stSegment >= SELF_TEST_COUNT) {
                stState = ST_DONE;
                if (g_state.state() == RSTATE_MOVING) g_state.transition(RSTATE_IDLE);
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
        g_safeMotion.stop();
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

    stSegment = 0;
    stState = ST_MOVE;
    stStartTime = millis();
    ledSet(255, 255, 0, 64);
    Serial.println("[SELF-TEST] safe 16 ms interpolation armed");
}

// ── Command processor ───────────────────────────────────────────────
static void processCmd(const String& line) {
    // ── Immediate commands (not queued) ──────────────────────────
    if (line == "help") {
        cmdRespond("help | status | test | selftest | servo start | servo end | servo j1 <j1-j6> | exo enable | exo disable | exo direction <H1-H6> <-1|1> | estop | reset | heartbeat\r\n");
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

    if (line.startsWith("exo direction ")) {
        int channel = 0;
        int direction = 0;
        if (sscanf(line.c_str(), "exo direction %d %d", &channel, &direction) != 2 ||
            channel < 1 || channel > 6 || (direction != 1 && direction != -1)) {
            cmdRespond("Usage: exo direction <1-6> <-1|1>\r\n");
            return;
        }
        if (exoskeletonSetDirection(channel - 1, direction)) {
            cmdRespondF("OK: H%d -> J%d direction %+d saved\r\n", channel,
                        EXOSKELETON_TO_ROBOT_JOINT[channel - 1] + 1, direction);
        } else {
            cmdRespond("ERR: stop exoskeleton control and other motion first\r\n");
        }
        return;
    }

    // ── E-STOP: IMMEDIATE — bypass queue, official SDK StopMotion ─
    if (line == "estop") {
        g_cmdQueue.clear();                   // Discard all pending commands
        g_safeMotion.reset();                 // Stop the motion task before sharing UDP
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
        if (g_safeMotion.active()) { cmdRespond("ERR: motion active\r\n"); return; }
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
    for (int channel = 0; channel < 6; ++channel) {
        s_exoTelemetry.angles[channel] = values[channel + 1];
        s_exoTelemetry.millivolts[channel] = values[channel + 7];
        if (s_exoTelemetry.calibrating) {
            // Follow the settling S3 filter so one button press captures the stable pose.
            s_exoTelemetry.zeroOffsets[channel] = values[channel + 1];
        }
    }
    updateExoskeletonRobotTargets();
    if (s_exoTelemetry.calibrating &&
        s_exoLastPacketMs - s_exoZeroCaptureStartedMs >= EXO_ZERO_CAPTURE_MS) {
        s_exoTelemetry.calibrating = false;
        saveExoskeletonCalibration();
    }

    char ack[48];
    snprintf(ack, sizeof(ack), "EXO_ACK:%lu,%d", (unsigned long)values[0],
             s_exoControlEnabled ? 1 : 0);
    s_cmdServer.beginPacket(s_exoIP, s_exoPort);
    s_cmdServer.write((const uint8_t*)ack, strlen(ack));
    s_cmdServer.endPacket();

    // Forward telemetry to the Node proxy without mixing it with CNDE feedback.
    if (s_proxyPort > 0) {
        char json[640];
        snprintf(json, sizeof(json),
                 "{\"type\":\"exoskeleton_state\",\"seq\":%lu,"
                 "\"angles\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],"
                 "\"robotTargets\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],"
                 "\"millivolts\":[%.0f,%.0f,%.0f,%.0f,%.0f,%.0f],"
                 "\"mapping\":[1,2,4,3,6,5],"
                 "\"directions\":[%d,%d,%d,%d,%d,%d],"
                 "\"calibrated\":%s,\"controlEnabled\":%s,"
                 "\"source\":\"p4\",\"ts\":%lu}",
                 (unsigned long)values[0],
                 values[1], values[2], values[3], values[4], values[5], values[6],
                 s_exoTelemetry.robotTargets[0], s_exoTelemetry.robotTargets[1],
                 s_exoTelemetry.robotTargets[2], s_exoTelemetry.robotTargets[3],
                 s_exoTelemetry.robotTargets[4], s_exoTelemetry.robotTargets[5],
                 values[7], values[8], values[9], values[10], values[11], values[12],
                 s_exoTelemetry.directions[0], s_exoTelemetry.directions[1],
                 s_exoTelemetry.directions[2], s_exoTelemetry.directions[3],
                 s_exoTelemetry.directions[4], s_exoTelemetry.directions[5],
                 s_exoTelemetry.calibrated ? "true" : "false",
                 s_exoControlEnabled ? "true" : "false", (unsigned long)millis());
        s_cmdServer.beginPacket(s_proxyIP, s_proxyPort);
        s_cmdServer.write((const uint8_t*)json, strlen(json));
        s_cmdServer.endPacket();
    }

    if (!s_exoControlEnabled || !g_state.canMove() || !wifiMgrConnected()) return;

    if (!s_exoServoActive) {
        const RobotStateData robotState = g_cnde.getState();
        if (!robotFeedbackFresh(robotState)) {
            Serial.println("[EXO] waiting for valid robot joint feedback");
            return;
        }
        s_exoTargetInitialized = true;
        s_exoLastAppliedSequence = UINT32_MAX;
        for (int i = 0; i < 6; ++i) {
            s_exoAcceptedTarget[i] = robotState.jointPos[i];
        }
        Serial.println("[EXO] safe 16 ms ServoJ follow ready");
    }
}

static void exoskeletonServoTick(uint32_t now) {
    if (!s_exoControlEnabled || !s_exoTargetInitialized ||
        !wifiMgrConnected() || !g_state.canMove()) return;
    if (now - s_exoLastPacketMs > EXO_PACKET_TIMEOUT_MS) return;
    if (s_exoTelemetry.sequence == s_exoLastAppliedSequence) return;
    s_exoLastAppliedSequence = s_exoTelemetry.sequence;

    for (int i = 0; i < 6; ++i) {
        s_exoAcceptedTarget[i] = s_exoTelemetry.robotTargets[i];
    }

    const RobotStateData robotState = g_cnde.getState();
    if (!robotFeedbackFresh(robotState)) return;
    int result = g_safeMotion.setTarget(
        s_exoAcceptedTarget, robotState.jointPos, true);
    if (result != FR_OK) {
        Serial.printf("[EXO] safe ServoJ start/update failed: %d\n", result);
        g_safeMotion.stop();
        s_exoServoActive = false;
        s_exoTargetInitialized = false;
        g_state.force(RSTATE_ERROR);
        return;
    }
    s_exoServoActive = true;
    g_state.transition(RSTATE_MOVING);
}

static void handleFairinoError(const RobotStateData& robotState) {
    static int32_t lastMainCode = 0;
    static int32_t lastSubCode = 0;
    if (!robotState.valid ||
        (robotState.mainCode == lastMainCode && robotState.subCode == lastSubCode)) return;

    lastMainCode = robotState.mainCode;
    lastSubCode = robotState.subCode;
    if (robotState.mainCode == 0 && robotState.subCode == 0) {
        Serial.println("[FAIRINO-ERROR] cleared");
        return;
    }

    Serial.printf("[FAIRINO-ERROR] main=%ld sub=%ld; motion stopped\n",
                  (long)robotState.mainCode, (long)robotState.subCode);
    s_exoControlEnabled = false;
    g_cmdQueue.clear();
    if (g_safeMotion.active()) g_safeMotion.stop();
    s_exoServoActive = false;
    s_exoTargetInitialized = false;
    s_exoLastAppliedSequence = UINT32_MAX;
    g_state.force(RSTATE_ERROR);
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
    g_safeMotion.begin(&g_fairino);
    BaseType_t motionTaskResult = xTaskCreatePinnedToCore(
        safeMotionNetworkTask, "servo-net", 6144, nullptr, 4,
        &s_motionTaskHandle, 0);
    if (motionTaskResult != pdPASS) {
        Serial.println("[SAFE-MOTION] ERROR: failed to create network task");
    }

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

    // UDP parsing above can update lastPacketMs after the loop's cached `now`.
    // Use a fresh timestamp here to avoid unsigned underflow and false timeouts.
    const uint32_t exoNow = millis();

    // Pause stale motion but keep follow mode latched until the user turns it off.
    if (s_exoControlEnabled && s_exoLastPacketMs > 0 &&
        exoNow - s_exoLastPacketMs > EXO_PACKET_TIMEOUT_MS) {
        if (s_exoServoActive) {
            g_cmdQueue.clear();
            g_safeMotion.stop();
            s_exoServoActive = false;
            s_exoTargetInitialized = false;
            s_exoLastAppliedSequence = UINT32_MAX;
            if (g_state.state() == RSTATE_MOVING) g_state.transition(RSTATE_IDLE);
            Serial.println("[EXO] packet timeout; follow paused, switch remains on");
        }
    }

    handleFairinoError(g_cnde.getState());
    exoskeletonServoTick(exoNow);

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
                g_safeMotion.reset();                 // Stop the motion task before sharing UDP
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
                    const RobotStateData robotState = g_cnde.getState();
                    int r = robotFeedbackFresh(robotState)
                        ? g_safeMotion.setTarget(e.joints, robotState.jointPos,
                                                 e.type == CMD_EXO_MOVE)
                        : FR_ERR_NOT_CONN;
                    if (r != FR_OK) {
                        cmdRespondF("ERR: safe servo motion failed %d\r\n", r);
                        g_state.force(RSTATE_ERROR);
                    } else {
                        g_state.transition(RSTATE_MOVING);
                    }
                    if (r == FR_OK && e.type == CMD_SERVO_MOVE) {
                        cmdRespondF("OK: servo j1 → %.1f %.1f %.1f %.1f %.1f %.1f\r\n",
                                    e.joints[0], e.joints[1], e.joints[2],
                                    e.joints[3], e.joints[4], e.joints[5]);
                    }
                }
                break;
            }
            case CMD_SERVO_START: {
                if (g_state.canMove()) {
                    const RobotStateData robotState = g_cnde.getState();
                    int r = robotFeedbackFresh(robotState)
                        ? g_safeMotion.setTarget(
                            robotState.jointPos, robotState.jointPos, true)
                        : FR_ERR_NOT_CONN;
                    if (r == FR_OK) {
                        g_state.transition(RSTATE_MOVING);
                        cmdRespond("OK: safe servo hold started\r\n");
                    } else {
                        cmdRespondF("ERR: safe servo start failed %d\r\n", r);
                    }
                }
                break;
            }
            case CMD_SERVO_END: {
                g_safeMotion.stop();
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
        g_safeMotion.reset();                 // Stop the motion task before sharing UDP
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
    const int motionResult = s_motionTaskError;
    if (motionResult != FR_OK) {
        s_motionTaskError = FR_OK;
        Serial.printf("[SAFE-MOTION] ServoJ stream failed: %d\n", motionResult);
        s_exoServoActive = false;
        s_exoTargetInitialized = false;
        g_state.force(RSTATE_ERROR);
    } else if (!g_safeMotion.active() && !s_exoControlEnabled &&
               stState != ST_MOVE && stState != ST_SETTLE &&
               g_state.state() == RSTATE_MOVING) {
        g_state.transition(RSTATE_IDLE);
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
            Serial.printf("J1:%.1f J2:%.1f J3:%.1f J4:%.1f J5:%.1f J6:%.1f st:%s hb:%lu err:%ld/%ld\n",
                          rs.jointPos[0], rs.jointPos[1], rs.jointPos[2],
                          rs.jointPos[3], rs.jointPos[4], rs.jointPos[5],
                          g_state.stateName(), g_heartbeat.age(),
                          (long)rs.mainCode, (long)rs.subCode);
            // UDP broadcast to web proxy, including Fairino controller status/error fields.
            if (wifiMgrConnected() && cmdBound && s_proxyPort > 0) {
                char buf[192];
                snprintf(buf, sizeof(buf),
                         "JOINTS:%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%lu,%u,%u,%ld,%ld\n",
                         rs.jointPos[0], rs.jointPos[1], rs.jointPos[2],
                         rs.jointPos[3], rs.jointPos[4], rs.jointPos[5],
                         (int)g_state.state(), g_heartbeat.age(),
                         (unsigned)rs.robotState, (unsigned)rs.programState,
                         (long)rs.mainCode, (long)rs.subCode);
                s_cmdServer.beginPacket(s_proxyIP, s_proxyPort);
                s_cmdServer.write((const uint8_t*)buf, strlen(buf));
                s_cmdServer.endPacket();
            }
        }
    }
}
