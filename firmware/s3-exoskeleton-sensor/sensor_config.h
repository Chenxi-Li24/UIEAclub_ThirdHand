#pragma once

// WiFi network shared by the S3 sensor and P4 controller.
#define WIFI_SSID           "ZTE-P5cS5Y"
#define WIFI_PASS           "12345678"

// Keep the sensor on a different address from the P4 (192.168.58.100).
#define USE_STATIC_IP       1
#define SENSOR_STATIC_IP    "192.168.58.101"
#define NETWORK_GATEWAY     "192.168.58.1"
#define NETWORK_MASK        "255.255.255.0"

#define P4_CONTROLLER_IP    "192.168.58.100"
#define P4_COMMAND_PORT     20008
#define SENSOR_UDP_PORT     20009

// PC running web-control/server/proxy.js. Update this if the PC LAN IP changes.
#define WEB_PROXY_IP        "192.168.58.38"
#define WEB_TELEMETRY_PORT  20010

// ESP32-S3 Dev Module onboard WS2812 RGB LED.
#define RGB_LED_PIN         48
#define RGB_LED_BRIGHTNESS  24
#define P4_ACK_TIMEOUT_MS   1000
#define WEB_ACK_TIMEOUT_MS  1000

#define SENSOR_COUNT        6
#define ADC_FULL_SCALE_MV   3300.0f
#define ADC_SAMPLE_COUNT    16
#define SEND_INTERVAL_MS    50
#define SERIAL_PRINT_INTERVAL_MS 200

// One Euro angle filter. Lower min cutoff removes more idle jitter; higher
// beta follows fast motion more closely.
#define FILTER_MIN_CUTOFF_HZ        0.8f
#define FILTER_BETA                 0.04f
#define FILTER_DERIVATIVE_CUTOFF_HZ 1.0f

// Defaults are used only before a channel has been calibrated in NVS.
// direction: +1 = counter-clockwise adds angle, -1 = clockwise adds angle.
static const float DEFAULT_ZERO_DEG[SENSOR_COUNT] = {0, 0, 0, 0, 0, 0};
static const int8_t DEFAULT_DIRECTION[SENSOR_COUNT] = {1, 1, 1, 1, 1, 1};
