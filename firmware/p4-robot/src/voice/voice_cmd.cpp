// voice_cmd.cpp - Voice command state machine (orchestration)
// Coordinates: ES7210 -> ASR -> Agent relay -> robot execution
#include "voice/voice_cmd.h"
#include "voice/audio_i2c.h"
#include "voice/es7210_driver.h"
#include "voice/es8311_driver.h"
#include "voice/asr_client.h"
#include "voice/agent_relay.h"
#include "voice/tts_client.h"
#include "config.h"
#include <Arduino.h>

static VoiceState s_state = VOICE_IDLE;
static String s_lastText;
static String s_lastError;
static String s_lastResponse;
static unsigned long s_stateStart = 0;
static bool s_initialized = false;
static String s_appKey;                     // cached for TTS
static int16_t* s_ttsPcm = nullptr;        // TTS PCM buffer (PSRAM)
static size_t s_ttsSamples = 0;

#define LISTEN_TIMEOUT_MS  10000
#define ASR_TIMEOUT_MS      5000
#define AGENT_TIMEOUT_MS    8000
#define EXEC_TIMEOUT_MS    15000

#define CHUNK_SAMPLES  1600   // 100ms @ 16kHz
static int16_t s_pcmBuf[CHUNK_SAMPLES];

// ── ASR callbacks ─────────────────────────────────────────────────────
static void onAsrResult(const char* text, bool isFinal) {
    if (s_state != VOICE_ASR_PROCESSING && s_state != VOICE_LISTENING) return;
    s_lastText = text;
    if (isFinal && s_lastText.length() > 0) {
        es7210Stop();
        asrEndSession();
        s_state = VOICE_AGENT_RELAY;
        s_stateStart = millis();
        if (agentRelayConnect()) {
            agentRelaySend(s_lastText.c_str());
        } else {
            s_lastError = "Agent unreachable";
            s_state = VOICE_ERROR;
        }
    }
}

static void onAsrError(const char* err, bool) {
    s_lastError = err;
    s_state = VOICE_ERROR;
    es7210Stop();
    asrEndSession();
}

// ── Agent callback ────────────────────────────────────────────────────
static void onAgentResponse(const char* type, const void* data, size_t len) {
    if (type && strcmp(type, "text") == 0) {
        s_lastResponse = (const char*)data;
        // Route to TTS → speaker instead of going idle
        if (es8311Detected() && s_appKey.length() > 0) {
            s_state = VOICE_TTS_SYNTH;
            s_stateStart = millis();
        } else {
            s_state = VOICE_IDLE;
        }
    }
}

// ── Public API ────────────────────────────────────────────────────────
bool voiceCmdInit(const char* asrProvider,
                  const char* asrAppKey, const char* asrApiKey,
                  const char* asrSecretKey,
                  const char* agentIp, uint16_t agentPort) {
    Serial.println("[Voice] Initializing...");
    s_appKey = asrAppKey;  // cache for TTS
    bool micOk = false;

#if ENABLE_MIC
    micOk = es7210Init(16000);
    if (!micOk)
        Serial.println("[Voice] Mic init FAILED — continuing without mic (TTS still works)");
#else
    Serial.println("[Voice] MIC disabled (ENABLE_MIC=0, conflicts with GSL3680 @ 0x40)");
#endif

#if ENABLE_SPEAKER
    if (!es8311Init(16000))
        Serial.println("[Voice] Amp init FAILED — continuing without speaker");
#else
    Serial.println("[Voice] SPEAKER disabled (ENABLE_SPEAKER=0)");
#endif

    // Only init ASR + agent relay if we have at least speaker (for TTS feedback)
    // ASR needs NTP time (already synced in setup) and mic
    asrInit(asrProvider, asrAppKey, asrApiKey, asrSecretKey);
    asrSetCallback(onAsrResult, onAsrError);

    // Token needed for both ASR and TTS (NTP must be synced)
    if (!asrGetToken())
        Serial.println("[Voice] Token fetch FAILED — voice services will not work");
    else
        Serial.println("[Voice] NLS token obtained OK");

    agentRelayInit(agentIp, agentPort);
    agentRelaySetCallback(onAgentResponse);

    s_initialized = true;
    Serial.println("[Voice] Init complete");
    return true;
}

void voiceCmdStart() {
    Serial.printf("[Voice] cmdStart: initialized=%d state=%d\n", s_initialized, s_state);
    if (!s_initialized || s_state != VOICE_IDLE) { voiceCmdStop(); }

    s_state = VOICE_LISTENING;
    s_stateStart = millis();
    s_lastText = ""; s_lastError = ""; s_lastResponse = "";

    Serial.println("[Voice] Starting mic...");
    if (!es7210Start()) {
        s_lastError = "Mic start failed";
        s_state = VOICE_ERROR;
        Serial.println("[Voice] Mic start FAILED");
        return;
    }
    Serial.println("[Voice] Starting ASR session...");
    if (!asrStartSession()) {
        s_lastError = "ASR connect failed";
        es7210Stop();
        s_state = VOICE_ERROR;
        Serial.println("[Voice] ASR session FAILED");
        return;
    }
    Serial.println("[Voice] Listening...");
}

void voiceCmdTts(const char* text) {
    if (!es8311Detected() || s_appKey.length() == 0) {
        Serial.println("[Voice] TTS: no speaker or no appKey");
        return;
    }
    const char* token = asrToken();
    if (!token || strlen(token) == 0) {
        Serial.println("[Voice] TTS: no NLS token — try /asr first");
        return;
    }
    s_lastResponse = text;
    s_state = VOICE_TTS_SYNTH;
    s_stateStart = millis();
    Serial.printf("[Voice] TTS: \"%s\"\n", text);
}

void voiceCmdStop() {
    if (s_state == VOICE_IDLE) return;
    es7210Stop();
    asrEndSession();
    es8311Stop();
    s_state = VOICE_IDLE;
}

void voiceCmdTick() {
    if (s_state == VOICE_IDLE || s_state == VOICE_ERROR) return;
    unsigned long now = millis();

    switch (s_state) {
    case VOICE_LISTENING:
        if (now - s_stateStart > LISTEN_TIMEOUT_MS) {
            s_state = VOICE_ASR_PROCESSING;
            s_stateStart = now;
            asrEndSession();
            break;
        }
        {
            size_t samples = es7210Read(s_pcmBuf, CHUNK_SAMPLES);
            if (samples > 0) {
                // Compute peak amplitude for mic debugging
                int16_t peak = 0;
                for (size_t i = 0; i < samples; i++) {
                    int16_t absVal = s_pcmBuf[i] >= 0 ? s_pcmBuf[i] : -s_pcmBuf[i];
                    if (absVal > peak) peak = absVal;
                }
                static unsigned long lastDbg = 0;
                if (now - lastDbg >= 2000) {
                    // Dump first 8 raw PCM samples
                    Serial.printf("[Voice] Audio: %u samples, peak=%d, raw=[", samples, peak);
                    for (size_t i = 0; i < (samples < 8 ? samples : 8); i++)
                        Serial.printf("%s%d", i ? "," : "", s_pcmBuf[i]);
                    Serial.println("]");
                    lastDbg = now;
                }
                asrSendAudio(s_pcmBuf, samples);
            }
        }
        asrTick();
        break;

    case VOICE_ASR_PROCESSING:
        asrTick();
        if (now - s_stateStart > ASR_TIMEOUT_MS) {
            s_lastError = "ASR timeout";
            s_state = VOICE_ERROR;
            es7210Stop();
        }
        break;

    case VOICE_AGENT_RELAY:
        agentRelayTick();
        if (now - s_stateStart > AGENT_TIMEOUT_MS) {
            s_state = VOICE_EXECUTING;
            s_stateStart = now;
        }
        break;

    case VOICE_TTS_SYNTH:
        // Fetch TTS audio from cloud (blocking ~2-3s, acceptable during voice interaction)
        {
            const char* token = asrToken();
            if (token && strlen(token) > 0) {
                s_ttsPcm = ttsSynthesize(s_lastResponse.c_str(), s_appKey.c_str(),
                                         token, &s_ttsSamples);
            }
            if (s_ttsPcm && s_ttsSamples > 0) {
                s_state = VOICE_SPEAKING;
            } else {
                Serial.printf("[Voice] TTS failed: %s\n", ttsLastError());
                s_lastError = ttsLastError();
                s_state = VOICE_IDLE;
            }
        }
        break;

    case VOICE_EXECUTING:
        agentRelayTick();
        if (now - s_stateStart > EXEC_TIMEOUT_MS)
            s_state = VOICE_IDLE;
        break;

    case VOICE_SPEAKING:
        // Play TTS PCM through ES8311 amp (blocking for audio duration)
        if (s_ttsPcm && s_ttsSamples > 0) {
            Serial.printf("[Voice] Speaking %u samples...\n", s_ttsSamples);
            es8311SetVolume(70);
            es8311Play(s_ttsPcm, s_ttsSamples);
            free(s_ttsPcm);
            s_ttsPcm = nullptr;
            s_ttsSamples = 0;
        }
        s_state = VOICE_IDLE;
        break;

    default: break;
    }
}

VoiceState voiceCmdState()       { return s_state; }
const char* voiceCmdLastText()   { return s_lastText.c_str(); }
const char* voiceCmdLastError()  { return s_lastError.c_str(); }
const char* voiceCmdLastResponse(){ return s_lastResponse.c_str(); }
