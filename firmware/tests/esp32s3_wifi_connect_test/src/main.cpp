#include <Arduino.h>
#include <WiFi.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
// Keep real credentials in a local secrets.h file. The placeholders allow
// this example to compile safely when secrets.h has not been created yet.
constexpr char WIFI_TEST_SSID[] = "YOUR_2G_WIFI_SSID";
constexpr char WIFI_TEST_PASSWORD[] = "YOUR_WIFI_PASSWORD";
#endif

void printWiFiState() {
    Serial.print("WiFi status: ");
    Serial.println(static_cast<int>(WiFi.status()));

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        Serial.print("RSSI: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
    }
}

void setup() {
    Serial.begin(115200);
    delay(3000);

    Serial.println();
    Serial.println("ESP32-S3 WiFi Connect Test Start");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_TEST_SSID, WIFI_TEST_PASSWORD);

    const unsigned long timeoutMs = 20000;
    const unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < timeoutMs) {
        Serial.print(".");
        delay(500);
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected");
        Serial.print("SSID: ");
        Serial.println(WiFi.SSID());
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        Serial.print("RSSI: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
    } else {
        Serial.println("WiFi connect failed");
        Serial.print("WiFi.status(): ");
        Serial.println(static_cast<int>(WiFi.status()));
    }
}

void loop() {
    printWiFiState();
    delay(5000);
}
