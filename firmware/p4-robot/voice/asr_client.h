// asr_client.h - Cloud ASR WebSocket client (Aliyun NLS)
// ESP32-P4 streams 16kHz/16bit PCM to cloud, receives recognized text
// Token-based auth: auto-fetches & caches token via HMAC-SHA1 signature
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef void (*AsrCallback)(const char* text, bool isFinal);

// Initialize ASR client
// provider: "aliyun"
bool asrInit(const char* provider,
             const char* appKey,
             const char* apiKey,
             const char* secretKey);

void asrSetCallback(AsrCallback onResult, AsrCallback onError);

// Fetch a fresh token from Aliyun NLS meta server (needs NTP time)
// Returns true if token obtained; call before asrStartSession
// Token is cached and auto-refreshed when near expiry
bool asrGetToken();

// Start real-time recognition WebSocket session
bool asrStartSession();
bool asrSendAudio(const int16_t* pcm, size_t samples);
void asrEndSession();
bool asrIsConnected();
void asrTick();
const char* asrToken();        // cached NLS token (shared with TTS)
const char* asrLastError();
