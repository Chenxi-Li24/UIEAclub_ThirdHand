#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println();
    Serial.println("ESP32-S3 Serial Test Start");
}

void loop() {
    static int count = 0;
    Serial.print("count: ");
    Serial.println(count++);
    delay(1000);
}
