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

// ── Display & Touch ──────────────────────────────────────────────────
#define ENABLE_DISPLAY  1   // JD9365DA MIPI DSI 800×1280
#define ENABLE_TOUCH    1   // GSL3680 I2C capacitive touch

// ── Voice Control ──────────────────────────────────────────────────
#define VOICE_ENABLED       1   // 1 = voice enabled (I2C shared with touch on I2C_NUM_0 GPIO7/8)
#define ENABLE_MIC          1   // 1 = ES7210 mic onboard (⚠ shares 0x40 with GSL3680)
#define ENABLE_SPEAKER      1   // 1 = ES8311 amp present
#define VOICE_ASR_PROVIDER  "aliyun"
#define VOICE_ASR_APPKEY    "YOUR_ALIYUN_NLS_APPKEY"
#define VOICE_ASR_APIKEY    "YOUR_ALIYUN_ACCESSKEY_ID"
#define VOICE_ASR_SECRET    "YOUR_ALIYUN_ACCESSKEY_SECRET"
#define VOICE_AGENT_IP      "192.168.58.11"   // PC running claude_agent.py
#define VOICE_AGENT_PORT    9000
#define VOICE_TTS_VOICE     "zhitian_emo"     // TTS voice: zhitian_emo(f) aixia(m) siyue(f)

// ── LED ──────────────────────────────────────────────────────────────
#define PIN_WS2812      26
#define LED_BUILTIN      48
#define BOOT_BUTTON      35
