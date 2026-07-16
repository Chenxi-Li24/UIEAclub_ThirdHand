#pragma once
// Master-Arm Controller Configuration
// Hardware: ESP32-SEED + FEETECH Servo Driver Board
// Servos: 6x FEEIECH STS3215 (STS series)

// ── WiFi ──────────────────────────────────────────────
#define WIFI_SSID     "ZTE-P5cS5Y"
#define WIFI_PASS     "12345678"
#define STATIC_IP     "192.168.58.104"
#define STATIC_GW     "192.168.58.1"
#define STATIC_MASK   "255.255.255.0"

// ── Robot (Fairino) ───────────────────────────────────
#define ROBOT_IP      "192.168.58.2"
#define ROBOT_PORT    20007

// ── Servo (STS3215) ──────────────────────────────────
#define SERVO_BAUD      1000000      // STS series = 1M bps
#define SERVO_RX        44           // UART0 RX (XIAO D7)
#define SERVO_TX        43           // UART0 TX (XIAO D6)
#define SERVO_COUNT     6
#define SERVO_POS_MAX   4096.0f      // STS 12-bit position range

// ── Master-Arm Control ────────────────────────────────
// Match P4 SafeServoMotion: acc=0 vel=0 (velocity done in software),
// cmdT=0.016s (60Hz servo loop). Robot follows raw joint positions.
#define SEND_INTERVAL_MS  16         // ~60Hz (P4 uses ~16ms)
#define SERVOJ_ACC        0.0f       // 0 = robot uses raw angle, P4 style
#define SERVOJ_VEL        20.0f      // 20°/s — explicit velocity limit, robot won't use max speed
#define SERVOJ_CMDT       0.016f     // 16ms command period (P4 standard)

// ── Angle Limits ──────────────────────────────────────
#define JOINT_MIN_DEG    -170.0f
#define JOINT_MAX_DEG     170.0f

// ── Pins ──────────────────────────────────────────────
#define PIN_LED_BUILTIN   21         // XIAO ESP32S3 built-in Neopixel (WS2812)
#define PIN_CALIB_BUTTON  0          // BOOT button -> trigger calibration
#define PIN_ESTOP         2          // E-STOP button (active LOW, XIAO D1)

// ── Joint Mapping (adjustable) ────────────────────────
// sign: +1.0 = normal, -1.0 = invert
// scale: scaling factor (usually 1.0)
const float JOINT_SIGN[SERVO_COUNT]  = { 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f};
const float JOINT_SCALE[SERVO_COUNT] = { 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f};

// ── Fairino Zero Pose (robot target angles when master is at zero) ──
const float FAIRINO_ZERO_POSE[SERVO_COUNT] = { 0.0f, 0.0f, -150.0f, 0.0f, 90.0f, 0.0f };

// ── Master → Fairino Joint Mapping (master_index → fairino_joint_index) ──
// Master ID1→FJ1, ID2→FJ2, ID3→FJ3, ID4→FJ4, ID5→FJ6, ID6→FJ5
const int JOINT_MAP[SERVO_COUNT] = { 0, 1, 2, 3, 5, 4 };

// ── Pre-calibrated Zero Offsets (raw servo positions at zero pose) ──
// Recorded: 2026-07-16 on XIAO ESP32S3
// ID1=3716  ID2=4035  ID3=2048  ID4=2037  ID5=2404  ID6=2564
#define ZERO_POS_1  3716
#define ZERO_POS_2  4035
#define ZERO_POS_3  2048
#define ZERO_POS_4  2037
#define ZERO_POS_5  2404
#define ZERO_POS_6  2564
