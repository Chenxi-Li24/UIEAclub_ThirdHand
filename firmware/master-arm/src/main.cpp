// Master-Arm Controller — main entry
// Reads 6x STS3215 servos -> applies calibration -> sends ServoJ to Fairino via WiFi UDP
//
// Serial commands:
//   c = calibrate zero pose at current position
//   r = RobotEnable(1)
//   d = RobotEnable(0)
//   x = ServoMoveStart
//   a = ResetAlarm XML-RPC
//   g = GroupReset XML-RPC
//   m = DragSwitch(0)
//   e = E-STOP
//   o = dump calibrator offsets
//   O = force defaults from config.h (discard NVS)

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "servo_reader.h"
#include "calibrator.h"
#include "fairino_udp.h"
#include "cnde_client.h"
#include "wifi_manager.h"
#include "stats.h"

// ── Globals ──────────────────────────────────────────────
static ServoReader       g_reader;
static Calibrator        g_calib;
static FairinoUDPClient  g_fairino;
static CNDEClient        g_cnde;

// ── State ────────────────────────────────────────────────
enum State { ST_WAIT_WIFI, ST_WAIT_CALIB, ST_RUNNING, ST_ESTOP };
static State       g_state    = ST_WAIT_WIFI;
static bool        g_estop    = false;
static float       g_joint[SERVO_COUNT];
static unsigned long g_lastSend = 0;
static unsigned long g_btnDown  = 0;

// P4-style interpolator state (must be declared before runServoJ and handleSerialCmd)
static float        g_cmdPos[SERVO_COUNT];
static bool         g_cmdPosInit = false;
static const float  P4_HARD_LIMIT_DEG = 0.32f;  // max per-cycle change (20°/s × 0.016s)

// ── LED ──────────────────────────────────────────────────
static unsigned long g_ledLast = 0;
static uint8_t       g_ledBri  = 0;
static bool          g_ledUp   = true;

static void ledUpdate(State st) {
    if (millis() - g_ledLast < 20) return;
    g_ledLast = millis();
    switch (st) {
    case ST_WAIT_WIFI:
        if (g_ledUp) { g_ledBri++; if (g_ledBri >= 64) g_ledUp = false; }
        else         { g_ledBri--; if (g_ledBri <= 4)  g_ledUp = true;  }
        analogWrite(PIN_LED_BUILTIN, g_ledBri);
        break;
    case ST_WAIT_CALIB:
        analogWrite(PIN_LED_BUILTIN, 40);
        break;
    case ST_RUNNING:
        analogWrite(PIN_LED_BUILTIN, 40);
        break;
    case ST_ESTOP:
        analogWrite(PIN_LED_BUILTIN, (millis() / 100) % 2 ? 128 : 0);
        break;
    }
}

// ── Debug: full computation trace ─────────────────────────
static void printTrace(const float* rawAngles, bool servoOk) {
    // Print full trace: raw pos → raw deg → calibrated → fairino target
    float calibAngles[SERVO_COUNT];
    memcpy(calibAngles, rawAngles, sizeof(calibAngles));
    g_calib.apply(calibAngles);

    float fj[SERVO_COUNT];
    for (int i = 0; i < SERVO_COUNT; i++) {
        int t = JOINT_MAP[i];
        fj[t] = calibAngles[i] + FAIRINO_ZERO_POSE[t];
    }

    // Target
    Serial.print("[Target] ");
    for (int i = 0; i < SERVO_COUNT; i++) Serial.printf("J%d=%6.1f ", i + 1, fj[i]);
    Serial.println();

    // Actual (CNDE)
    const RobotStateData& rs = g_cnde.getState();
    if (rs.valid) {
        Serial.print("[Actual] ");
        for (int i = 0; i < 6; i++) Serial.printf("J%d=%6.1f ", i + 1, rs.jointPos[i]);
        Serial.printf(" rs:%d ps:%d err:%d/%d\n",
                      rs.robotState, rs.programState, (int)rs.mainCode, (int)rs.subCode);
    } else {
        Serial.print("[Actual] CNDE: no data");
        Serial.println(g_cnde.isConnected() ? " (connected)" : " (disconnected)");
    }

    // Servo raw
    Serial.printf("[Servo ] %s  ID:" , servoOk ? "OK" : "ERR");
    for (int i = 0; i < SERVO_COUNT; i++)
        Serial.printf(" %d=%d", i + 1, g_reader.readPos(i + 1));
    Serial.println();
}

// ── Serial command handler ───────────────────────────────
static void handleSerialCmd(char cmd) {
    while (Serial.available()) Serial.read(); // flush rest

    switch (cmd) {
    case 'c': case 'C': {
        float raw[SERVO_COUNT];
        g_reader.readAngles(raw);
        g_calib.confirmZero(raw);
        g_state = ST_RUNNING;
        g_cmdPosInit = false;  // re-sync interpolator from CNDE actual
        Serial.println("[Main] Calibrated!");
        break;
    }
    case 'r': case 'R':
        Serial.println("[Cmd] Sending RobotEnable(1)...");
        g_fairino.robotEnable(1);
        break;
    case 'd': case 'D':
        Serial.println("[Cmd] Sending RobotEnable(0)...");
        g_fairino.robotEnable(0);
        break;
    case 'x': case 'X':
        Serial.println("[Cmd] Sending ServoMoveStart...");
        g_fairino.servoMoveStart();
        delay(50);
        break;
    case 'a': case 'A':
        Serial.println("[Cmd] Sending ResetAlarm...");
        g_fairino.sendXmlRpc("ResetAlarm", "<i4>1</i4>");
        break;
    case 'g': case 'G':
        Serial.println("[Cmd] Sending GroupReset...");
        g_fairino.sendXmlRpc("GroupReset", "<i4>1</i4>");
        break;
    case 'm': case 'M':
        Serial.println("[Cmd] Sending DragSwitch(0)...");
        g_fairino.sendXmlRpc("DragSwitch", "<i4>0</i4>");
        break;
    case 'e': case 'E':
        Serial.println("[Cmd] E-STOP!");
        g_fairino.stopMotion();
        g_fairino.servoMoveEnd();
        g_state = ST_ESTOP;
        g_estop = true;
        break;
    case 'o':
        g_calib.dump();
        break;
    case 'O':
        Serial.println("[Cmd] Forcing calibrator defaults (erasing NVS)...");
        g_calib.forceDefaults();
        g_calib.dump();
        break;
    default:
        Serial.printf("[Cmd] Unknown: '%c' (0x%02X)\n", cmd, cmd);
        Serial.println("  c=cal r=enb d=dis x=start a=alarm g=grpReset m=drag0 e=stop o=dump O=defs");
        break;
    }
}

// ── P4-style ServoJ interpolation ─────────────────────────
static void runServoJ(const float* rawAngles, bool servoOk) {
    if (millis() - g_lastSend < SEND_INTERVAL_MS) return;
    g_lastSend = millis();

    // Wait for stable servo data + CNDE
    static int stableCount = 0;
    if (!servoOk || !g_cnde.getState().valid) {
        stableCount = 0;
        return;
    }
    if (stableCount < 50) { stableCount++; return; }  // 50 × 16ms = 800ms warmup

    // ── Init interpolator from CNDE actual position (P4 style) ──
    if (!g_cmdPosInit) {
        const RobotStateData& rs = g_cnde.getState();
        for (int i = 0; i < SERVO_COUNT; i++) {
            g_cmdPos[i] = rs.jointPos[i];  // start from actual robot position
        }
        g_cmdPosInit = true;
        Serial.print("[Motion] Interpolator init from CNDE actual: ");
        for (int i = 0; i < SERVO_COUNT; i++) Serial.printf("J%d=%.1f ", i+1, g_cmdPos[i]);
        Serial.println();

        // Deferred ServoMoveStart — send right before first ServoJ, not in setup()
        Serial.println("[Motion] Sending ServoMoveStart...");
        int smRet = g_fairino.servoMoveStart();
        Serial.printf("[Motion] ServoMoveStart returned %d\n", smRet);
        delay(10);  // brief pause for robot to enter servo mode
    }

    // ── Compute target from master arm ──
    float angles[SERVO_COUNT];
    memcpy(angles, rawAngles, sizeof(angles));
    g_calib.apply(angles);  // calibrates + clamps

    float target[SERVO_COUNT];
    for (int i = 0; i < SERVO_COUNT; i++) {
        int t = JOINT_MAP[i];
        target[t] = angles[i] + FAIRINO_ZERO_POSE[t];
    }
    // Clamp targets
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (target[i] < -170.0f) target[i] = -170.0f;
        if (target[i] >  170.0f) target[i] =  170.0f;
    }

    // ── P4-style interpolation: limit velocity ──
    for (int i = 0; i < SERVO_COUNT; i++) {
        float delta = target[i] - g_cmdPos[i];
        float maxStep = P4_HARD_LIMIT_DEG;  // 0.32°/cycle
        if (delta >  maxStep) delta =  maxStep;
        if (delta < -maxStep) delta = -maxStep;
        g_cmdPos[i] += delta;

        // Clamp
        if (g_cmdPos[i] < -170.0f) g_cmdPos[i] = -170.0f;
        if (g_cmdPos[i] >  170.0f) g_cmdPos[i] =  170.0f;
    }

    // Sanity check
    bool bogus = false;
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (isnan(g_cmdPos[i]) || isinf(g_cmdPos[i])) { bogus = true; break; }
    }
    if (bogus) { g_cmdPosInit = false; return; }

    // Send ServoJ with P4 params: acc=0, vel=0, cmdT=0.016
    g_fairino.servoJ(g_cmdPos[0], g_cmdPos[1], g_cmdPos[2],
                     g_cmdPos[3], g_cmdPos[4], g_cmdPos[5],
                     SERVOJ_ACC, SERVOJ_VEL, SERVOJ_CMDT);

    // Debug: show command vs target vs actual (once per second)
    static unsigned long lastPosDbg = 0;
    if (millis() - lastPosDbg > 1000) {
        lastPosDbg = millis();
        const RobotStateData& rs = g_cnde.getState();
        Serial.print("[P4-CMD] ");
        for (int i = 0; i < SERVO_COUNT; i++) Serial.printf("J%d=%6.1f ", i+1, g_cmdPos[i]);
        Serial.print(" | delta: ");
        for (int i = 0; i < SERVO_COUNT; i++) Serial.printf("%5.1f ", target[i] - g_cmdPos[i]);
        if (rs.valid) {
            Serial.print(" | actual: ");
            for (int i = 0; i < SERVO_COUNT; i++) Serial.printf("%5.1f ", rs.jointPos[i]);
            Serial.printf(" err:%d/%d", (int)rs.mainCode, (int)rs.subCode);
        }
        Serial.println();
    }

    // Capture first response
    static bool firstRespDone = false;
    if (!firstRespDone) {
        firstRespDone = true;
        String resp;
        g_fairino.recvResponse(resp, 200);
    }
}

// ── Setup ────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(3000);  // wait for serial monitor to connect
    Serial.println("\n\n=== Master-Arm Controller v3 ===");

    pinMode(PIN_LED_BUILTIN, OUTPUT);
    pinMode(PIN_CALIB_BUTTON, INPUT_PULLUP);
    pinMode(PIN_ESTOP, INPUT_PULLUP);

    g_reader.begin(Serial0);
    g_calib.begin();

    // Force NVS defaults on first boot to eliminate corrupted calibration
    g_calib.forceDefaults();
    Serial.println("[Main] NVS calibration reset to config.h defaults");

    // ── WiFi Scan ──────────────────────────────────────
    Serial.println("[WiFi] Scanning nearby networks...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();
    Serial.printf("[WiFi] Found %d networks:\n", n);
    for (int i = 0; i < n; i++) {
        Serial.printf("  %d: %s  RSSI=%d  Ch=%d  %s\n",
                      i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i),
                      WiFi.channel(i),
                      WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "(open)" : "");
    }
    WiFi.scanDelete();

    wifiMgrInit();
    g_fairino.begin();
    g_fairino.setTarget(ROBOT_IP, ROBOT_PORT);
    g_cnde.begin(ROBOT_IP, 20005);

    Serial.printf("\n[WiFi] Trying DHCP on %s...\n", WIFI_SSID);
    wifiMgrConnect(WIFI_SSID, WIFI_PASS);

    unsigned long wifiStart = millis();
    while (!wifiMgrConnected() && millis() - wifiStart < 10000) {
        wifiMgrTick();
        delay(10);
    }

    if (!wifiMgrConnected()) {
        Serial.println("[WiFi] DHCP failed, trying static IP...");
        wifiMgrDisconnect();
        delay(500);
        wifiMgrConnectStatic(WIFI_SSID, WIFI_PASS, STATIC_IP, STATIC_GW, STATIC_MASK);
        wifiStart = millis();
        while (!wifiMgrConnected() && millis() - wifiStart < 10000) {
            wifiMgrTick();
            delay(10);
        }
    }

    if (wifiMgrConnected()) {
        Serial.printf("[WiFi] OK! IP: %s\n", wifiMgrLocalIP().c_str());
        Serial.printf("[Robot] Target: %s:%d\n", ROBOT_IP, ROBOT_PORT);

        Serial.println("[FR-XML] Sending RobotEnable(1)...");
        int reRet = g_fairino.robotEnable(1);
        Serial.printf("[FR-XML] RobotEnable(1) returned %d, enabled=%d\n",
                      reRet, g_fairino.isRobotEnabled());

        Serial.println("[Servo] Scanning IDs 1-6...");
        g_reader.allAlive();

        if (g_calib.isCalibrated()) {
            // ServoMoveStart is deferred — sent in runServoJ()
            // when interpolator is ready with CNDE actual positions
            g_state = ST_RUNNING;
            Serial.println("[Main] Running (ServoMoveStart deferred)");
        } else {
            g_state = ST_WAIT_CALIB;
            Serial.println("[Main] WAIT - send 'c' to calibrate zero");
        }
    } else {
        Serial.println("[WiFi] FAILED");
        g_state = ST_WAIT_WIFI;
    }
    Serial.println("[Main] Cmds: c=cal r=en d=dis x=start a=alarm m=drag0 e=stop o=dump O=defs");
}

// ── Loop ─────────────────────────────────────────────────
void loop() {
    wifiMgrTick();
    if (wifiMgrConnected()) g_cnde.tick();

    // ── Serial command ──────────────────────────────────
    if (Serial.available()) {
        handleSerialCmd(Serial.read());
    }

    // ── E-STOP check ──────────────────────────────────
    bool estopNow = (digitalRead(PIN_ESTOP) == LOW);
    if (estopNow && !g_estop) {
        Serial.println("[E-STOP] ACTIVATED!");
        if (wifiMgrConnected()) g_fairino.stopMotion();
        g_state = ST_ESTOP;
        g_estop = true;
    }
    if (!estopNow && g_estop) {
        Serial.println("[E-STOP] Released - send 'c' to calibrate");
        g_estop = false;
        g_state = ST_WAIT_CALIB;
    }
    if (g_state == ST_ESTOP) {
        ledUpdate(ST_ESTOP);
        delay(50);
        return;
    }

    // ── Read servos ───────────────────────────────────
    float rawAngles[SERVO_COUNT];
    bool servoOk = g_reader.readAngles(rawAngles);

    // ── Debug print every 1s ──────────────────────────
    static unsigned long lastDbg = 0;
    if (millis() - lastDbg > 1000) {
        lastDbg = millis();
        printTrace(rawAngles, servoOk);
    }

    // ── WiFi check ─────────────────────────────────────
    if (!wifiMgrConnected()) {
        ledUpdate(ST_WAIT_WIFI);
        return;
    }

    // Retry RobotEnable if failed
    if (!g_fairino.isRobotEnabled()) {
        static unsigned long lastEnableRetry = 0;
        if (millis() - lastEnableRetry > 5000) {
            lastEnableRetry = millis();
            Serial.println("[FR-XML] Retrying RobotEnable(1)...");
            g_fairino.robotEnable(1);
        }
    }

    // ── Calibration button ─────────────────────────────
    if (digitalRead(PIN_CALIB_BUTTON) == LOW) {
        if (g_btnDown == 0) g_btnDown = millis();
        if (millis() - g_btnDown > 500) {
            g_btnDown = 0;
            g_calib.confirmZero(rawAngles);
            g_state = ST_RUNNING;
            g_cmdPosInit = false;  // re-sync interpolator from CNDE actual
            Serial.println("[Main] Button calibrate -> Running!");
            delay(500);
        }
    } else {
        g_btnDown = 0;
    }

    if (g_state == ST_WAIT_CALIB) {
        ledUpdate(ST_WAIT_CALIB);
        return;
    }

    // ── P4-style ServoJ interpolation ───────────────────
    if (g_state == ST_RUNNING) {
        runServoJ(rawAngles, servoOk);
    }

    ledUpdate(g_state);
}
