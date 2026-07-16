// es8311_driver.cpp - ES8311 DAC amp: I2C config + I2S TX DMA
// I2S_NUM_0 full-duplex — TX drives DOUT(GPIO9) + shared clocks (MCLK/BCLK/LRCK)
// ES7210 RX also on I2S_NUM_0 — clocks shared, separate data pins
#include "voice/es8311_driver.h"
#include "voice/audio_i2c.h"
#include "pins_config.h"
#include <Arduino.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static i2s_chan_handle_t s_txChan = nullptr;
static bool s_playing = false;
static uint8_t s_volume = 70;

#define ES8311_I2S_PORT  I2S_NUM_0   // shared with ES7210 RX

#define ES8311_RESET         0x00
#define ES8311_CLK_MGR_01    0x01
#define ES8311_CLK_MGR_02    0x02
#define ES8311_CLK_MGR_03    0x03
#define ES8311_CLK_MGR_04    0x04
#define ES8311_CLK_MGR_05    0x05
#define ES8311_SDP_CTL       0x10
#define ES8311_SDP_MGR       0x11
#define ES8311_DAC_CTL1      0x20
#define ES8311_DAC_CTL2      0x21
#define ES8311_DAC_CTL3      0x22
#define ES8311_DAC_VOL       0x2A
#define ES8311_SYS_PDN       0x0D
#define ES8311_GPIO_SEL      0x42
#define ES8311_GP_EN         0x43

static bool es8311ConfigI2C() {
    if (!audioI2cProbe(ES8311_ADDR)) {
        Serial.printf("[ES8311] I2C probe FAIL at 0x%02X\n", ES8311_ADDR);
        return false;
    }
    Serial.printf("[ES8311] I2C probe OK at 0x%02X\n", ES8311_ADDR);
    audioI2cWriteReg(ES8311_ADDR, ES8311_RESET, 0x3F); delay(10);
    audioI2cWriteReg(ES8311_ADDR, ES8311_RESET, 0x00); delay(10);
    audioI2cWriteReg(ES8311_ADDR, ES8311_SYS_PDN, 0x00);
    audioI2cWriteReg(ES8311_ADDR, ES8311_CLK_MGR_01, 0x0F);
    audioI2cWriteReg(ES8311_ADDR, ES8311_CLK_MGR_02, 0x00);
    audioI2cWriteReg(ES8311_ADDR, ES8311_CLK_MGR_03, 0x10);
    audioI2cWriteReg(ES8311_ADDR, ES8311_CLK_MGR_04, 0x00);
    audioI2cWriteReg(ES8311_ADDR, ES8311_CLK_MGR_05, 0x40);
    audioI2cWriteReg(ES8311_ADDR, ES8311_SDP_CTL, 0x00);
    audioI2cWriteReg(ES8311_ADDR, ES8311_SDP_MGR, 0x02);
    audioI2cWriteReg(ES8311_ADDR, ES8311_DAC_CTL1, 0x18);
    audioI2cWriteReg(ES8311_ADDR, ES8311_DAC_CTL2, 0x02);
    audioI2cWriteReg(ES8311_ADDR, ES8311_DAC_CTL3, 0x02);
    audioI2cWriteReg(ES8311_ADDR, ES8311_DAC_VOL, 0x00);
    audioI2cWriteReg(ES8311_ADDR, ES8311_GPIO_SEL, 0x08);
    audioI2cWriteReg(ES8311_ADDR, ES8311_GP_EN, 0x08);
    Serial.println("[ES8311] Registers configured");
    return true;
}

static bool es8311InitI2S(uint32_t sampleRate) {
    // Use the TX channel already created by ES7210 on I2S_NUM_0 (full-duplex)
    extern i2s_chan_handle_t es7210GetTxHandle();
    s_txChan = es7210GetTxHandle();
    if (!s_txChan) {
        Serial.println("[ES8311] No TX handle from ES7210 — init ES7210 first");
        return false;
    }
    Serial.printf("[ES8311] I2S TX %luHz (shared full-duplex) DOUT=%d\n",
                  sampleRate, PIN_I2S_AMP_DOUT);
    return true;
}

bool es8311Detected() { return audioI2cProbe(ES8311_ADDR); }

bool es8311Init(uint32_t sampleRate) {
    if (!es8311ConfigI2C())
        Serial.println("[ES8311] I2C config FAILED — continuing anyway");
    if (!es8311InitI2S(sampleRate)) return false;
    Serial.println("[ES8311] Init complete");
    return true;
}

bool es8311Play(const int16_t* data, size_t samples) {
    if (!s_txChan) return false;
    // Enable TX channel (OK if already enabled — shared full-duplex with ES7210 RX)
    esp_err_t err = i2s_channel_enable(s_txChan);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return false;
    s_playing = true;
    size_t bw = 0;
    // Timeout: audio duration + 3s buffer (don't block forever if clock isn't running)
    TickType_t timeout = pdMS_TO_TICKS((samples * 1000) / 16000 + 3000);
    esp_err_t writeErr = i2s_channel_write(s_txChan, data, samples * sizeof(int16_t), &bw, timeout);
    if (writeErr != ESP_OK) {
        Serial.printf("[ES8311] I2S write timeout/error: %d (wrote %u/%u)\n", writeErr, bw, samples * sizeof(int16_t));
    }
    vTaskDelay(pdMS_TO_TICKS((samples * 1000) / 16000 + 50));
    // NOTE: NEVER disable TX — it shares full-duplex I2S_NUM_0 clocks with ES7210 RX
    s_playing = false;
    return true;
}

void es8311Stop() {
    // Shared full-duplex: don't touch I2S channel — let ES7210 manage lifecycle
    s_playing = false;
}

bool es8311IsPlaying() { return s_playing; }

void es8311SetVolume(uint8_t vol) {
    if (vol > 100) vol = 100;
    s_volume = vol;
    uint8_t r = (uint8_t)((100 - vol) * 0x60 / 100);
    audioI2cWriteReg(ES8311_ADDR, ES8311_DAC_VOL, r);
}

void es8311Deinit() {
    es8311Stop();
    if (s_txChan) { i2s_del_channel(s_txChan); s_txChan = nullptr; }
}
