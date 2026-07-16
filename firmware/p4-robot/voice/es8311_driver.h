// es8311_driver.h - ES8311 low-power audio DAC driver (I2S TX + I2C config)
// JC8012P4A1 onboard speaker amplifier output
// I2C bus must be pre-initialized via audioI2cInit() before calling es8311Init()
#pragma once
#include <stdint.h>
#include <stddef.h>

// ES8311 I2C address (7-bit)
#define ES8311_ADDR  0x18

// Initialize ES8311 via I2C + configure I2S TX
// I2S pins from pins_config.h: shared clocks from I2S_NUM_0, DOUT=GPIO9
// Calls audioI2cInit() internally (no-op if touch already owns I2C_NUM_0)
// sampleRate: should match ES7210 (16000 for Aliyun NLS)
bool es8311Init(uint32_t sampleRate = 16000);

// Play PCM audio data (blocks until queued to DMA)
// Returns true if successfully queued
bool es8311Play(const int16_t* data, size_t samples);

// Stop playback immediately
void es8311Stop();

// Check if audio is currently playing
bool es8311IsPlaying();

// Set output volume (0-100)
void es8311SetVolume(uint8_t vol);

// Check if ES8311 is detected on I2C bus
bool es8311Detected();

// De-initialize
void es8311Deinit();
