// audio_i2c.h - Shared I2C bus for ES7210 + ES8311 audio codecs
// Uses legacy i2c.h API (I2C_NUM_1) to avoid conflict with touch (I2C_NUM_0)
#pragma once
#include <stdint.h>

// Initialize shared I2C bus for audio codecs
// Call once before es7210Init() / es8311Init()
// Returns true if I2C_NUM_1 is ready
bool audioI2cInit(int sda, int scl);

// Low-level I2C write: send [reg, val] to dev_addr
// Used by both ES7210 and ES8311 drivers
bool audioI2cWriteReg(uint8_t devAddr, uint8_t reg, uint8_t val);

// Probe: check if device at addr ACKs
bool audioI2cProbe(uint8_t devAddr);
