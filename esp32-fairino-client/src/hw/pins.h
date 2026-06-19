// src/hw/pins.h — 物理桌宠引脚定义
// 支持两块板子，通过 #define BOARD_WAVESHARE 切换
//
//   P169H002-CTP      1.69" 240x280 ST7789V + CST816T (自研 ESP32-S3-Pico)
//   Waveshare         1.69" 240x280 ST7789V2 + CST816T (ESP32-S3-Touch-LCD-1.69 V2.1)
//
// 默认 Waveshare；切回自研板注释掉 #define BOARD_WAVESHARE

#pragma once

#define BOARD_WAVESHARE  // <-- 注释此行切回 P169H002-CTP

// ══ SPI 屏幕 (ST7789, 4-wire SPI Mode 3) ══
#if defined(BOARD_WAVESHARE)
// Waveshare ESP32-S3-Touch-LCD-1.69 V2.1 (verified against official pin_config.h)
#  define PIN_LCD_SCLK 6
#  define PIN_LCD_MOSI 7
#  define PIN_LCD_CS 5
#  define PIN_LCD_DC 4
#  define PIN_LCD_RST 8
#  define PIN_LCD_BL 15
#else
// P169H002-CTP 自研板
#  define PIN_LCD_SCLK 12
#  define PIN_LCD_MOSI 11
#  define PIN_LCD_CS 2
#  define PIN_LCD_DC 13
#  define PIN_LCD_RST 10
#  define PIN_LCD_BL 9
#endif

// ── WS2812 ──
#define PIN_WS2812 21
#define WS2812_NUM 1

// ══ I2C 总线 ══
#if defined(BOARD_WAVESHARE)
#  define PIN_I2C_SDA 11
#  define PIN_I2C_SCL 10
#else
#  define PIN_I2C_SDA 4
#  define PIN_I2C_SCL 5
#endif

// ══ 触摸 CST816T I2C 0x15 ══
#if defined(BOARD_WAVESHARE)
#  define PIN_TP_RST 13
#  define PIN_TP_INT 14
#else
#  define PIN_TP_RST 2
#  define PIN_TP_INT 3
#endif

// ── WS2812 LED ──
#define PIN_LED 18

// ── PWM 蜂鸣器 ──
// Waveshare V2.1: GPIO42 per HARDWARE_REFERENCE.md
#define PIN_BEEP 42

// ── 电池 ADC (18650 分压 1:1) ──
#define PIN_VBAT_ADC 1

// ── 电源按键 ──
#define PIN_PWR_KEY 0

// ── AI 语音 UART 预留 ──
#define PIN_VOICE_RX 20
#define PIN_VOICE_TX 19

// ── 屏幕参数 ──
// 1.69" ST7789V: 240x280 physical, row offset 20 (under bezel)
// Visible area: 240x260, Canvas fills it completely
#define LCD_PHYS_W 240
#define LCD_PHYS_H 280
#define HW_W 240
#define HW_H 250
