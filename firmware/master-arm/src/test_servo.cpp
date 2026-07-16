// Simple servo read — UART0, GPIO43 TX, GPIO44 RX, 1M baud, 8N1
#include <Arduino.h>
#include <SCServo.h>

SMS_STS servo;

void setup() {
    Serial.begin(115200);
    Serial0.begin(1000000, SERIAL_8N1, 44, 43);  // RX=44, TX=43
    servo.pSerial = &Serial0;
    Serial.println("[READY] UART0 1M 8N1 RX=44 TX=43");
}

void loop() {
    static bool started = false;
    if (!started && Serial.available()) {
        while (Serial.available()) Serial.read();
        started = true;
        Serial.println("[START]");
    }
    if (!started) { delay(500); return; }

    for (int id = 1; id <= 6; id++) {
        int err = servo.getLastError();
        int pos = servo.ReadPos(id);
        int ea  = servo.getLastError();
        Serial.printf("ID%d: pos=%d(0x%04X) err=%d->%d\n", id, pos, (unsigned)pos, err, ea);
        delay(10);
    }
    Serial.println("---");
    delay(500);
}
