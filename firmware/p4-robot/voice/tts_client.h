// tts_client.h - Aliyun NLS TTS REST API client
// Converts text to 16-bit PCM audio via HTTPS POST
// Same token/auth system as ASR client (share asrGetToken())
#pragma once
#include <stdint.h>
#include <stddef.h>

// Synthesize text to speech (blocking HTTPS request, ~1-3s)
// Returns PSRAM-allocated PCM buffer (caller must free()), or nullptr on error
// outSamples: receives number of int16_t samples
// text: max ~300 chars recommended (longer text = larger PCM)
// appKey: Aliyun NLS project appKey (same as ASR)
// token: Aliyun NLS token from asrGetToken()
// voice: "zhitian_emo" (female), "aixia" (male), "siyue" (female)
int16_t* ttsSynthesize(const char* text, const char* appKey, const char* token,
                       size_t* outSamples,
                       const char* voice = "zhitian_emo",
                       uint32_t sampleRate = 16000);

const char* ttsLastError();
