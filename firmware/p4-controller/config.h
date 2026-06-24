#pragma once
// ── WiFi ─────────────────────────────────────────────────────────────
#define WIFI_SSID       "ZTE-P5cS5Y"
#define WIFI_PASS       "12345678"
#define STATIC_IP       "192.168.58.100"
#define STATIC_GW       "192.168.58.1"
#define STATIC_MASK     "255.255.255.0"

// ── Robot ────────────────────────────────────────────────────────────
#define ROBOT_IP        "192.168.58.2"
#define ROBOT_UDP_PORT  20007

// ── Self-test motion parameters ─────────────────────────────────────
// vel/acc: 暂不开放，传0；cmdT: ServoJ指令周期
#define SELF_TEST_ACC       0.0f
#define SELF_TEST_VEL       0.0f
#define SELF_TEST_CMDT      2.0f    // 2s 指令周期 (50% speed)
#define SELF_TEST_SETTLE_MS 5000    // 每个位置停留时间(ms)
#define SELF_TEST_TIMEOUT   300000  // 5min overall timeout

// ── Web/UDP servo command parameters ────────────────────────────────
// Web 端做轨迹插补，每 16ms 发一条 ServoJ，cmdT 需匹配插补间隔
// SDK 建议范围: 0.001~0.016s
#define WEB_SERVO_ACC       0.0f
#define WEB_SERVO_VEL       0.0f
#define WEB_SERVO_CMDT      0.016f  // 16ms 指令周期（匹配 Web 插补间隔）

// ── Display & Touch ──────────────────────────────────────────────────
#define ENABLE_DISPLAY  1   // JD9365DA MIPI DSI 800×1280
#define ENABLE_TOUCH    1   // GSL3680 I2C capacitive touch

// ── LED ──────────────────────────────────────────────────────────────
#define PIN_WS2812      26
#define LED_BUILTIN      48
#define BOOT_BUTTON      35
