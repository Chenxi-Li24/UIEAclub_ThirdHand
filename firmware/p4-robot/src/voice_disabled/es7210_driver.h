// es7210_driver.h - ES7210 4-ch ADC microphone driver (I2S RX + I2C config)
// JC8012P4A1 onboard dual-MEMS mic capture @ 16kHz/16bit/mono
#pragma once
#include <stdint.h>
#include <stddef.h>

// ES7210 I2C address (7-bit)
#define ES7210_ADDR  0x40

// Initialize ES7210 via I2C + start I2S RX DMA
// sda/scl: I2C pins for codec control (shared with touch bus)
// mclk/bclk/lrck/data: I2S audio data pins
// sampleRate: 8000, 16000, 22050, 44100, 48000
bool es7210Init(int sda, int scl,
                int mclk, int bclk, int lrck, int data,
                uint32_t sampleRate = 16000);

// Start I2S capture (DMA begins filling ring buffer)
bool es7210Start();

// Stop I2S capture
void es7210Stop();

// Read captured PCM samples from ring buffer (non-blocking)
// Returns number of samples actually read (0 if buffer empty)
// Each sample is int16_t, mono
size_t es7210Read(int16_t* buf, size_t maxSamples);

// Number of samples currently available in ring buffer
size_t es7210Available();

// Check if ES7210 is detected on I2C bus
bool es7210Detected();

// De-initialize and free resources
void es7210Deinit();
