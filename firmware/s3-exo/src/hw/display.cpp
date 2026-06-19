// src/hw/display.cpp — ST7789V 240x280 + LVGL framebuffer
// Verified against Waveshare ESP32-S3-Touch-LCD-1.69 V2.1 example

#include "hw/display.h"

#include <Arduino_GFX_Library.h>

static Arduino_DataBus *s_bus = nullptr;
static Arduino_GFX *s_gfx = nullptr;

// LVGL draw buffers (DMA-capable)
static lv_disp_draw_buf_t s_drawBuf;
static lv_color_t *s_buf1 = nullptr;
static lv_color_t *s_buf2 = nullptr;

Arduino_GFX *hwGfx() {
  return s_gfx;
}

// ── LVGL flush callback ─────────────────────────────────────────────────
static void dispFlush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
#if (LV_COLOR_16_SWAP != 0)
  s_gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  s_gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif
  lv_disp_flush_ready(disp);
}

// ── ST7789 + backlight init ─────────────────────────────────────────────
bool hwDisplayInit() {
  s_bus = new Arduino_HWSPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_MOSI);

  // ST7789(bus, RST, rotation, IPS, w, h, col1, row1, col2, row2)
  s_gfx = new Arduino_ST7789(s_bus, PIN_LCD_RST, 0, true, LCD_PHYS_W, LCD_PHYS_H, 0, 20, 0, 0);

  if (!s_gfx->begin()) {
    Serial.println("hwDisplay: ST7789 gfx->begin() FAILED");
    return false;
  }
  s_gfx->fillScreen(BLACK);

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);

  Serial.printf("hwDisplay: ST7789V %dx%d OK\n", LCD_PHYS_W, LCD_PHYS_H);
  return true;
}

// ── LVGL init ───────────────────────────────────────────────────────────
bool hwDisplayInitLVGL() {
  lv_init();

  // Allocate DMA-capable draw buffers (1/4 screen each = ~33KB x2)
  size_t bufPixels = LCD_PHYS_W * LCD_PHYS_H / 4;
  s_buf1 = (lv_color_t *)heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
  s_buf2 = (lv_color_t *)heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
  if (!s_buf1 || !s_buf2) {
    Serial.println("hwDisplay: LVGL DMA buffer alloc FAILED");
    return false;
  }
  lv_disp_draw_buf_init(&s_drawBuf, s_buf1, s_buf2, bufPixels);

  // Display driver
  static lv_disp_drv_t dispDrv;
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = LCD_PHYS_W;
  dispDrv.ver_res = LCD_PHYS_H;
  dispDrv.flush_cb = dispFlush;
  dispDrv.draw_buf = &s_drawBuf;
  lv_disp_drv_register(&dispDrv);

  // Log callback
#if LV_USE_LOG != 0
  lv_log_register_print_cb([](const char *buf) { Serial.print(buf); });
#endif

  Serial.println("hwDisplay: LVGL init OK");
  return true;
}

// ── Brightness / Sleep ──────────────────────────────────────────────────
void hwDisplayBrightness(uint8_t lvl) {
  if (lvl == 0)
    digitalWrite(PIN_LCD_BL, LOW);
  else
    digitalWrite(PIN_LCD_BL, HIGH);
}

void hwDisplaySleep(bool off) {
  if (off) {
    digitalWrite(PIN_LCD_BL, LOW);
    s_gfx->displayOff();
  } else {
    s_gfx->displayOn();
    digitalWrite(PIN_LCD_BL, HIGH);
  }
}
