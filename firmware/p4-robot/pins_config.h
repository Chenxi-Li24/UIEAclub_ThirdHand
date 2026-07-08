// pins_config.h — JC8012P4A1 ESP32-P4 pin definitions
// 10.1" IPS 800x1280 MIPI DSI (JD9365DA) + GSL3680 touch + WS2812
// Audio pins VERIFIED against JC8012P4A1 schematic GPIO mapping table
#pragma once
#define LCD_H_RES   800
#define LCD_V_RES   1280
#define LCD_RST     27
#define LCD_LED     23
#define TP_I2C_SDA  7
#define TP_I2C_SCL  8
#define TP_RST      22
#define TP_INT      21
#define PIN_WS2812  26
#define WS2812_NUM  1
#define BOOT_BUTTON 35
#define LED_BUILTIN 48

// ── I2S Audio — SHARED bus (VERIFIED from schematic) ──────────────────
// Both ES7210 and ES8311 share the same MCLK/SCLK/LRCK
// Only data pins are separate: GPIO10 (mic in), GPIO09 (amp out)
#define PIN_I2S_MCLK      13   // ES7210 + ES8311 shared master clock
#define PIN_I2S_BCLK      12   // ES7210 + ES8311 shared bit clock
#define PIN_I2S_LRCK      10   // ES7210 + ES8311 shared frame sync (corrected per schematic)
#define PIN_I2S_MIC_DIN   11   // ES7210 SDOUT → ESP32-P4 I2S RX  (corrected per schematic)
#define PIN_I2S_AMP_DOUT   9   // ESP32-P4 → ES8311 DSDIN I2S TX

// ── Audio I2C bus — SHARED with touch on GPIO7/8 ─────────────────────
// Both touch (GSL3680) and audio codecs share the SAME physical I2C bus
// Touch driver already initializes I2C_NUM_0 (old i2c.h API); audio reuses it
// ⚠ ES7210 (0x40) may conflict with GSL3680 (0x40) — I2C scan needed
#define AUDIO_I2C_SDA   TP_I2C_SDA   // GPIO7
#define AUDIO_I2C_SCL   TP_I2C_SCL   // GPIO8
#define AUDIO_I2C_PORT  I2C_NUM_0    // shared with touch
