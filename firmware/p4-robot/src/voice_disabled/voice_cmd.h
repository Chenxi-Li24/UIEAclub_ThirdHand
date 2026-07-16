// voice_cmd.h - Voice command state machine (orchestration layer)
// Coordinates: ES7210 mic -> ASR client -> Agent relay -> Robot control
#pragma once
#include <stdint.h>

enum VoiceState {
    VOICE_IDLE = 0,
    VOICE_LISTENING,
    VOICE_ASR_PROCESSING,
    VOICE_AGENT_RELAY,
    VOICE_EXECUTING,
    VOICE_SPEAKING,
    VOICE_ERROR
};

// sda/scl: I2C pins for ES7210/ES8311
// micPins[4]: {mclk, bclk, lrck, sdin} for ES7210
// ampPins[4]: {mclk, bclk, lrck, sdout} for ES8311
bool voiceCmdInit(int sda, int scl,
                  const int micPins[4], const int ampPins[4],
                  const char* asrProvider,
                  const char* asrAppKey, const char* asrApiKey,
                  const char* asrSecretKey,
                  const char* agentIp, uint16_t agentPort);

void voiceCmdStart();     // PTT — push to talk
void voiceCmdStop();      // Cancel
void voiceCmdTick();      // Call in loop()

VoiceState voiceCmdState();
const char* voiceCmdLastText();
const char* voiceCmdLastError();
const char* voiceCmdLastResponse();
