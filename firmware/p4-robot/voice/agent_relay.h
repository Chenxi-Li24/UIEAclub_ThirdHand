// agent_relay.h - PC Claude Agent WebSocket relay
// Forwards recognized ASR text to local PC Claude agent
// Receives TTS audio/text responses back
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef void (*AgentCallback)(const char* type, const void* data, size_t len);

bool agentRelayInit(const char* pcIp, uint16_t pcPort = 9000);
void agentRelaySetCallback(AgentCallback cb);
bool agentRelayConnect();
bool agentRelaySend(const char* text);
bool agentRelayIsConnected();
void agentRelayTick();
const char* agentRelayLastError();
