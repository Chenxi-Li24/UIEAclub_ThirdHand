#include <Arduino.h>
#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    delay(3000);

    Serial.println();
    Serial.println("ESP32-S3 WiFi Scan Test Start");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(1000);
}

void loop() {
    Serial.println();
    Serial.println("Scanning WiFi...");

    int n = WiFi.scanNetworks();

    Serial.print("Networks found: ");
    Serial.println(n);

    if (n == 0) {
        Serial.println("No WiFi networks found");
    } else {
        for (int i = 0; i < n; ++i) {
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" | RSSI: ");
            Serial.print(WiFi.RSSI(i));
            Serial.print(" dBm | CH: ");
            Serial.print(WiFi.channel(i));
            Serial.print(" | Encryption: ");
            Serial.println(WiFi.encryptionType(i));
        }
    }

    WiFi.scanDelete();
    delay(10000);
}
