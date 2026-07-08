// es7210_driver.cpp - ES7210 mic ADC: I2C config + I2S RX DMA
// I2S_NUM_0 RX MASTER — drives shared MCLK(13)/BCLK(12)/LRCK(11)
#include "voice/es7210_driver.h"
#include "voice/audio_i2c.h"
#include "pins_config.h"
#include <Arduino.h>
#include <driver/i2c.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static i2s_chan_handle_t s_rxChan = nullptr;
static i2s_chan_handle_t s_txChan = nullptr;  // Shared TX for ES8311

// ES8311 can get the TX handle after I2S init
i2s_chan_handle_t es7210GetTxHandle() { return s_txChan; }

#define RING_BUF_SAMPLES  32768
static int16_t* s_ringBuf = nullptr;
static volatile size_t s_ringRead = 0;
static volatile size_t s_ringWrite = 0;
static volatile bool s_running = false;

size_t es7210Available() {
    if (!s_ringBuf || !s_running) return 0;
    if (s_ringWrite >= s_ringRead) return s_ringWrite - s_ringRead;
    return (RING_BUF_SAMPLES - s_ringRead) + s_ringWrite;
}

// Read a register byte from the device at devAddr via I2C
static bool audioI2cReadReg(uint8_t devAddr, uint8_t reg, uint8_t* val) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, val, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK;
}

#define ES7210_CHIP_ID_H     0xFD   // should read 0x71
#define ES7210_RESET         0x00
#define ES7210_MAIN_CLK      0x02
#define ES7210_MODE_CFG      0x01
#define ES7210_ADC_CTL       0x06
#define ES7210_ADC_CH1_CTL   0x20
#define ES7210_ADC_CH2_CTL   0x21
#define ES7210_SDP_CTL1      0x10
#define ES7210_SDP_CTL2      0x11
#define ES7210_SDP_MCLK      0x0C

// Auto-detect ES7210 address (may not be at 0x40 — could be 0x32 or 0x36)
static uint8_t s_es7210Addr = 0;

static bool es7210ConfigI2C() {
    // ── Auto-detect ES7210 by write-verify on RESET register ──────────
    // Known I2C: 0x18=ES8311, 0x40=GSL3680, 0x32 and 0x36 are candidates
    // ES7210 RESET reg (0x00): default 0x00, accepts 0xFF→0x00 sequence
    const uint8_t candidates[] = { 0x32, 0x36 };
    s_es7210Addr = 0;
    for (int i = 0; i < 2; i++) {
        uint8_t val = 0xFF;
        if (!audioI2cReadReg(candidates[i], ES7210_RESET, &val)) {
            Serial.printf("[ES7210] 0x%02X read FAILED — skipping\n", candidates[i]);
            continue;
        }
        Serial.printf("[ES7210] 0x%02X RESET default=0x%02X\n", candidates[i], val);
        audioI2cWriteReg(candidates[i], ES7210_RESET, 0xFF);
        delay(10);
        audioI2cWriteReg(candidates[i], ES7210_RESET, 0x00);
        delay(10);
        val = 0xFF;
        if (audioI2cReadReg(candidates[i], ES7210_RESET, &val) && val == 0x00) {
            s_es7210Addr = candidates[i];
            Serial.printf("[ES7210] Found at 0x%02X (write-verify OK)\n", s_es7210Addr);
            break;
        }
    }
    if (!s_es7210Addr) {
        Serial.println("[ES7210] NOT FOUND: no responsive device at 0x32 or 0x36");
        return false;
    }
    // ── ES7210 init: enable analog path ────────────────────────────────
    audioI2cWriteReg(s_es7210Addr, ES7210_RESET, 0xFF); delay(50);
    audioI2cWriteReg(s_es7210Addr, ES7210_RESET, 0x00); delay(100);

    // Print key register defaults before config
    uint8_t rval;
    Serial.println("[ES7210] Pre-config regs:");
    for (int r : {0x00,0x01,0x02,0x06,0x07,0x08,0x09,0x0A,0x0C,0x10,0x11,0x20,0x21}) {
        if (audioI2cReadReg(s_es7210Addr, r, &rval))
            Serial.printf("  [0x%02X]=0x%02X\n", r, rval);
    }

    // Try to set analog power via MODE_CFG (0x01) bit7
    audioI2cWriteReg(s_es7210Addr, 0x01, 0x82);
    delay(5);
    // Try MICBIAS at 0x07
    audioI2cWriteReg(s_es7210Addr, 0x07, 0x20);
    delay(5);
    // PGA gain mic1+mic2 (30dB)
    audioI2cWriteReg(s_es7210Addr, 0x09, 0x30);
    audioI2cWriteReg(s_es7210Addr, 0x0A, 0x30);
    // Enable ADC ch1+ch2
    audioI2cWriteReg(s_es7210Addr, 0x06, 0x03);
    delay(5);

    // Print after config
    Serial.println("[ES7210] Post-config regs:");
    for (int r : {0x00,0x01,0x02,0x06,0x07,0x08,0x09,0x0A,0x0C,0x10,0x11,0x20,0x21}) {
        if (audioI2cReadReg(s_es7210Addr, r, &rval))
            Serial.printf("  [0x%02X]=0x%02X\n", r, rval);
    }

    // ── Full register scan 0x00-0xFF — identify all readable registers ──
    uint8_t v;
    Serial.println("[ES7210] === Full reg scan (0x00-0xFF) ===");
    for (int addr = 0; addr <= 0xFF; addr++) {
        if (audioI2cReadReg(s_es7210Addr, addr, &v)) {
            if (v != 0x00 && v != 0xFF) {  // Skip empty/unimplemented
                Serial.printf("[ES7210] [0x%02X]=0x%02X\n", addr, v);
            }
        }
    }
    Serial.println("[ES7210] === Scan complete ===");
    return true;
}

static bool es7210InitI2S(uint32_t sampleRate) {
    i2s_chan_config_t chanCfg = {};
    chanCfg.id = I2S_NUM_0;
    chanCfg.role = I2S_ROLE_MASTER;
    chanCfg.dma_desc_num = 8;
    chanCfg.dma_frame_num = 480;
    chanCfg.auto_clear = true;
    // Create FULL-DUPLEX: TX (ES8311) + RX (ES7210) on I2S_NUM_0
    if (i2s_new_channel(&chanCfg, &s_txChan, &s_rxChan) != ESP_OK) {
        Serial.println("[ES7210] I2S full-duplex channel alloc FAILED");
        return false;
    }
    // ── RX config (ES7210 mic input) ──────────────────────────
    i2s_std_config_t stdCfg = {};
    stdCfg.clk_cfg.sample_rate_hz = sampleRate;
    stdCfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    stdCfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    stdCfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
    stdCfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT;
    stdCfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    stdCfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    stdCfg.gpio_cfg.mclk = (gpio_num_t)PIN_I2S_MCLK;
    stdCfg.gpio_cfg.bclk = (gpio_num_t)PIN_I2S_BCLK;
    stdCfg.gpio_cfg.ws   = (gpio_num_t)PIN_I2S_LRCK;
    stdCfg.gpio_cfg.din  = (gpio_num_t)PIN_I2S_MIC_DIN;
    stdCfg.gpio_cfg.dout = I2S_GPIO_UNUSED;  // TX configured separately below
    if (i2s_channel_init_std_mode(s_rxChan, &stdCfg) != ESP_OK) {
        Serial.println("[ES7210] I2S RX std mode init FAILED");
        return false;
    }
    // ── TX config (ES8311 speaker out) — same clocks, DOUT=GPIO9 ──
    stdCfg.gpio_cfg.din  = I2S_GPIO_UNUSED;
    stdCfg.gpio_cfg.dout = (gpio_num_t)PIN_I2S_AMP_DOUT;
    if (i2s_channel_init_std_mode(s_txChan, &stdCfg) != ESP_OK) {
        Serial.println("[ES7210] I2S TX std mode init FAILED");
        return false;
    }
    Serial.printf("[ES7210] I2S full-duplex %luHz MCLK=%d BCLK=%d LRCK=%d DIN=%d DOUT=%d\n",
                  sampleRate, PIN_I2S_MCLK, PIN_I2S_BCLK, PIN_I2S_LRCK,
                  PIN_I2S_MIC_DIN, PIN_I2S_AMP_DOUT);
    return true;
}

bool es7210Detected() { return s_es7210Addr > 0 && audioI2cProbe(s_es7210Addr); }

bool es7210Init(uint32_t sampleRate) {
    s_ringBuf = (int16_t*)heap_caps_malloc(RING_BUF_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!s_ringBuf) s_ringBuf = (int16_t*)malloc(RING_BUF_SAMPLES * sizeof(int16_t));
    if (!s_ringBuf) { Serial.println("[ES7210] Ring buffer malloc FAILED"); return false; }
    if (!es7210ConfigI2C()) {
        Serial.println("[ES7210] I2C config FAILED — mic disabled");
        free(s_ringBuf); s_ringBuf = nullptr;
        return false;
    }
    if (!es7210InitI2S(sampleRate)) { free(s_ringBuf); s_ringBuf = nullptr; return false; }
    Serial.println("[ES7210] Init complete");
    return true;
}

bool es7210Start() {
    if (!s_rxChan || !s_ringBuf) {
        Serial.printf("[ES7210] Start FAIL: rxChan=%p ringBuf=%p\n", s_rxChan, s_ringBuf);
        return false;
    }
    if (s_running) return true;
    s_ringRead = s_ringWrite = 0;
    s_running = true;
    if (i2s_channel_enable(s_rxChan) != ESP_OK) { s_running = false; return false; }
    Serial.println("[ES7210] Capture started");
    return true;
}

void es7210Stop() {
    if (!s_running) return;
    s_running = false;
    if (s_rxChan) i2s_channel_disable(s_rxChan);
}

size_t es7210Read(int16_t* buf, size_t maxSamples) {
    if (!s_rxChan || !s_running) return 0;
    size_t bytes = 0;
    esp_err_t err = i2s_channel_read(s_rxChan, buf, maxSamples * sizeof(int16_t), &bytes, 0);
    return (err == ESP_OK && bytes > 0) ? bytes / sizeof(int16_t) : 0;
}

void es7210Deinit() {
    es7210Stop();
    if (s_rxChan) { i2s_del_channel(s_rxChan); s_rxChan = nullptr; }
    free(s_ringBuf); s_ringBuf = nullptr;
}
