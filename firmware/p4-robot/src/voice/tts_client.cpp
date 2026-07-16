// tts_client.cpp - Aliyun NLS TTS REST API client
// POST JSON to /stream/v1/tts, receive raw 16-bit PCM audio
// Uses PSRAM for PCM buffer (up to ~512KB for ~16s of speech)
#include "voice/tts_client.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

static String s_lastError;

static String jsonEscape(const String& src) {
    String out;
    out.reserve(src.length() + 16);
    for (size_t i = 0; i < src.length(); i++) {
        char c = src[i];
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

int16_t* ttsSynthesize(const char* text, const char* appKey, const char* token,
                       size_t* outSamples, const char* voice, uint32_t sampleRate) {
    *outSamples = 0;
    s_lastError = "";

    if (!text || strlen(text) == 0) { s_lastError = "Empty text"; return nullptr; }
    if (!token || strlen(token) == 0) { s_lastError = "No token"; return nullptr; }

    // Limit text length
    String textStr(text);
    if (textStr.length() > 400) {
        textStr = textStr.substring(0, 400);
        Serial.printf("[TTS] Text truncated to %d chars\n", textStr.length());
    }

    // Build JSON body
    String jsonBody = "{\"appkey\":\"" + String(appKey) + "\"";
    jsonBody += ",\"text\":\"" + jsonEscape(textStr) + "\"";
    jsonBody += ",\"format\":\"pcm\"";
    jsonBody += ",\"sample_rate\":" + String(sampleRate);
    jsonBody += ",\"voice\":\"" + String(voice) + "\"";
    jsonBody += ",\"volume\":50,\"speech_rate\":0,\"pitch_rate\":0}";

    Serial.printf("[TTS] Synthesizing %d chars voice=%s\n", textStr.length(), voice);

    // HTTPS POST to Aliyun NLS TTS endpoint
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15);

    const char* host = "nls-gateway.cn-shanghai.aliyuncs.com";
    if (!client.connect(host, 443)) {
        s_lastError = "TTS connect failed";
        Serial.println("[TTS] Connect FAILED");
        return nullptr;
    }

    String req = "POST /stream/v1/tts HTTP/1.1\r\n";
    req += "Host: " + String(host) + "\r\n";
    req += "Content-Type: application/json\r\n";
    req += "X-NLS-Token: " + String(token) + "\r\n";
    req += "Content-Length: " + String(jsonBody.length()) + "\r\n";
    req += "Connection: close\r\n\r\n";
    req += jsonBody;

    client.print(req);
    client.flush();
    Serial.println("[TTS] Request sent");

    // Read status line
    unsigned long tStart = millis();
    while (client.connected() && !client.available() && millis() - tStart < 5000)
        delay(10);

    String statusLine = client.readStringUntil('\n');
    statusLine.trim();
    Serial.printf("[TTS] Status: %s\n", statusLine.c_str());

    if (!statusLine.startsWith("HTTP/1.1 200") && !statusLine.startsWith("HTTP/1.0 200")) {
        s_lastError = "TTS HTTP " + statusLine;
        while (client.available()) {
            String line = client.readStringUntil('\n');
            if (line.length() > 0) Serial.printf("[TTS] Err: %s\n", line.c_str());
        }
        client.stop();
        return nullptr;
    }

    // Skip response headers, read Content-Length
    int contentLen = -1;
    while (client.connected() || client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) break;
        if (line.startsWith("Content-Length:"))
            contentLen = line.substring(16).toInt();
        if (millis() - tStart > 5000) break;
    }

    // Allocate PCM buffer — try PSRAM, fall back to SRAM progressively
    size_t bufCap = (contentLen > 0) ? (size_t)contentLen : (size_t)(256 * 1024);
    Serial.printf("[TTS] Alloc: contentLen=%d bufCap=%u\n", contentLen, bufCap);
    uint8_t* raw = (uint8_t*)heap_caps_malloc(bufCap, MALLOC_CAP_SPIRAM);
    if (!raw) {
        Serial.printf("[TTS] PSRAM alloc failed, trying SRAM...\n");
        // Try progressively smaller sizes in SRAM
        raw = (uint8_t*)malloc(128*1024);
        if (raw) { bufCap = 128*1024; Serial.printf("[TTS] SRAM 128K OK\n"); }
        else {
            raw = (uint8_t*)malloc(64*1024);
            if (raw) { bufCap = 64*1024; Serial.printf("[TTS] SRAM 64K OK\n"); }
            else {
                raw = (uint8_t*)malloc(32*1024);
                if (raw) { bufCap = 32*1024; Serial.printf("[TTS] SRAM 32K OK\n"); }
                else {
                    raw = (uint8_t*)malloc(16*1024);
                    if (raw) { bufCap = 16*1024; Serial.printf("[TTS] SRAM 16K OK\n"); }
                }
            }
        }
        if (!raw) { s_lastError = "TTS buffer alloc failed"; client.stop(); return nullptr; }
        Serial.println("[TTS] WARNING: SRAM fallback");
    }

    // Read PCM body
    size_t total = 0;
    unsigned long tRead = millis();
    while ((client.connected() || client.available()) && millis() - tRead < 15000) {
        int avail = client.available();
        if (avail > 0) {
            if (total + (size_t)avail > bufCap) {
                size_t newCap = (bufCap * 2 > 1024 * 1024) ? 1024 * 1024 : bufCap * 2;
                uint8_t* newBuf = (uint8_t*)heap_caps_realloc(raw, newCap, MALLOC_CAP_SPIRAM);
                if (!newBuf) newBuf = (uint8_t*)realloc(raw, newCap);
                if (!newBuf) { Serial.printf("[TTS] Realloc FAILED at %u\n", total); break; }
                raw = newBuf; bufCap = newCap;
            }
            size_t toRead = (size_t)avail;
            if (total + toRead > bufCap) toRead = bufCap - total;
            int rd = client.read(raw + total, toRead);
            if (rd > 0) total += rd;
        } else if (!client.connected()) {
            delay(50);
            if (!client.available()) break;
        }
        delay(1);
    }
    client.stop();

    if (total < 2) { free(raw); s_lastError = "TTS: empty response"; return nullptr; }
    if (total % 2) total--;  // align to 16-bit

    size_t samples = total / sizeof(int16_t);
    Serial.printf("[TTS] Done: %u samples %.1fs\n", samples,
                  (float)samples / (float)sampleRate);

    *outSamples = samples;
    return (int16_t*)raw;
}

const char* ttsLastError() { return s_lastError.c_str(); }
