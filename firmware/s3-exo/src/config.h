#pragma once
// Robot Controller Configuration
// WiFi defaults (fallback if NVS has no stored credentials)
#define WIFI_SSID       "Xiaomi_7D5E"
#define WIFI_PASS       "12345678"
#define STATIC_IP       "192.168.58.100"
#define STATIC_GW       "192.168.58.1"
#define STATIC_MASK     "255.255.255.0"

// Robot
#define ROBOT_IP        "192.168.58.2"
#define ROBOT_UDP_PORT  20007

// Motion is interpolated at 16 ms with a 20 deg/s hard limit.
#define SELF_TEST_SETTLE_MS 5000
#define SELF_TEST_TIMEOUT   300000

// Button (from hw/pins.h: PIN_PWR_KEY = GPIO0)
#define BOOT_BUTTON      0

// Self-test entry point (defined in main.cpp, called from ui_robot.cpp)
extern void selfTestStart();
extern bool safeMotionSetTarget(const float joints[6], bool continuous = false);
extern bool safeMotionStartHold();
extern void safeMotionStop();
