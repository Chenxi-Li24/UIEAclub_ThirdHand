#pragma once
// ── WiFi ─────────────────────────────────────────────────────────────
#define WIFI_SSID       "Xiaomi_7D5E"
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

// ── LED ──────────────────────────────────────────────────────────────
#define PIN_WS2812      17
#define LED_BUILTIN      48
#define BOOT_BUTTON      0
