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

// Self-test motion parameters
#define SELF_TEST_ACC       0.0f
#define SELF_TEST_VEL       0.0f
#define SELF_TEST_CMDT      2.0f
#define SELF_TEST_SETTLE_MS 5000
#define SELF_TEST_TIMEOUT   300000

// Button (from hw/pins.h: PIN_PWR_KEY = GPIO0)
#define BOOT_BUTTON      0

// Self-test entry point (defined in main.cpp, called from ui_robot.cpp)
extern void selfTestStart();
