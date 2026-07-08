// agent_relay.cpp - PC Claude Agent WebSocket relay
// Bridges recognized text from ESP32-P4 to local PC running claude_agent.py
#include "voice/agent_relay.h"
#include <Arduino.h>
#include <WiFi.h>

static String s_pcIp;
static uint16_t s_pcPort = 9000;
static AgentCallback s_onResponse = nullptr;
static WiFiClient s_client;
static bool s_connected = false;
static String s_lastError;

static bool wsSendText(const char* text) {
    if (!s_client.connected()) return false;
    size_t len = strlen(text);
    uint8_t hdr[10]; size_t hdrLen = 0;
    hdr[hdrLen++] = 0x81;
    if (len <= 125) {
        hdr[hdrLen++] = (uint8_t)(len | 0x80);
    } else if (len <= 65535) {
        hdr[hdrLen++] = (uint8_t)(126 | 0x80);
        hdr[hdrLen++] = (len >> 8) & 0xFF;
        hdr[hdrLen++] = len & 0xFF;
    }
    uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
    uint8_t* masked = (uint8_t*)malloc(len);
    for (size_t i = 0; i < len; i++) masked[i] = text[i] ^ mask[i % 4];
    s_client.write(hdr, hdrLen);
    s_client.write(mask, 4);
    s_client.write(masked, len);
    s_client.flush();
    free(masked);
    return true;
}

bool agentRelayInit(const char* pcIp, uint16_t pcPort) {
    s_pcIp = pcIp;
    s_pcPort = pcPort;
    Serial.printf("[AgentRelay] Target: %s:%d\n", pcIp, pcPort);
    return true;
}

void agentRelaySetCallback(AgentCallback cb) { s_onResponse = cb; }

bool agentRelayConnect() {
    if (s_connected) return true;
    if (!s_client.connect(s_pcIp.c_str(), s_pcPort)) {
        s_lastError = "Agent connect failed";
        return false;
    }
    String upgrade = "GET / HTTP/1.1\r\n";
    upgrade += "Host: " + s_pcIp + ":" + String(s_pcPort) + "\r\n";
    upgrade += "Upgrade: websocket\r\nConnection: Upgrade\r\n";
    upgrade += "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n";
    upgrade += "Sec-WebSocket-Version: 13\r\n\r\n";
    s_client.print(upgrade);
    s_client.flush();
    unsigned long t = millis();
    while (millis() - t < 3000) {
        if (s_client.available()) {
            String line = s_client.readStringUntil('\n');
            if (line.startsWith("HTTP/1.1 101")) {
                while (s_client.available() && s_client.readStringUntil('\n').length() > 2) {}
                s_connected = true;
                Serial.println("[AgentRelay] Connected");
                return true;
            }
        }
        delay(10);
    }
    s_lastError = "Agent upgrade timeout";
    s_client.stop();
    return false;
}

bool agentRelaySend(const char* text) {
    if (!s_connected) return false;
    char json[512];
    snprintf(json, sizeof(json),
             "{\"type\":\"voice_command\",\"text\":\"%s\",\"timestamp\":%lu}",
             text, millis());
    return wsSendText(json);
}

bool agentRelayIsConnected() { return s_connected; }

void agentRelayTick() {
    if (!s_connected || !s_client.connected()) {
        if (s_connected) { s_connected = false; }
        return;
    }
    if (s_client.available() >= 2) {
        uint8_t b0 = s_client.read(), b1 = s_client.read();
        uint8_t opcode = b0 & 0x0F;
        if (opcode == 0x08) { s_connected = false; s_client.stop(); return; }
        if (opcode == 0x01 || opcode == 0x00) {
            uint64_t len = b1 & 0x7F;
            if (len == 126) {
                while (s_client.available() < 2) delay(1);
                len = (s_client.read() << 8) | s_client.read();
            }
            if (len > 4096) return;
            uint8_t* data = (uint8_t*)malloc(len + 1);
            size_t total = 0;
            unsigned long t = millis();
            while (total < len && millis() - t < 2000) {
                int avail = s_client.available();
                if (avail > 0) {
                    size_t chunk = (size_t)avail < (len - total) ? avail : (len - total);
                    s_client.read(data + total, chunk);
                    total += chunk;
                }
                delay(1);
            }
            data[len] = 0;
            if (s_onResponse) s_onResponse("text", data, len);
            free(data);
        }
    }
}

const char* agentRelayLastError() { return s_lastError.c_str(); }
