// voice_cmd.h - Voice command state machine (orchestration layer)
// Coordinates: ES7210 mic -> ASR client -> Agent relay -> Robot control
// I2C audio bus must be initialized via audioI2cInit() before calling voiceCmdInit()
#pragma once
#include <stdint.h>

enum VoiceState {
    VOICE_IDLE = 0,
    VOICE_LISTENING,
    VOICE_ASR_PROCESSING,
    VOICE_AGENT_RELAY,
    VOICE_TTS_SYNTH,     // fetching TTS audio from cloud
    VOICE_EXECUTING,
    VOICE_SPEAKING,      // playing TTS audio through ES8311
    VOICE_ERROR
};

// Initialize voice subsystem (I2C bus must already be set up via audioI2cInit)
// I2S pins are read from pins_config.h — no pin parameters needed
bool voiceCmdInit(const char* asrProvider,
                  const char* asrAppKey, const char* asrApiKey,
                  const char* asrSecretKey,
                  const char* agentIp, uint16_t agentPort);

void voiceCmdStart();     // PTT — push to talk
void voiceCmdStop();      // Cancel
void voiceCmdTick();      // Call in loop()
void voiceCmdTts(const char* text);  // Direct TTS test (bypasses mic/ASR/agent)

VoiceState voiceCmdState();
const char* voiceCmdLastText();
const char* voiceCmdLastError();
const char* voiceCmdLastResponse();
