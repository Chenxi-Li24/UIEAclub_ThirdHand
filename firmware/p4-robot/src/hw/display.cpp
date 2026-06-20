// hw/display.cpp - JD9365DA MIPI DSI 800x1280 + LVGL framebuffer
// Based on JC8012P4A1 BSP example
#include "hw/display.h"
#include "pins_config.h"
#include <Arduino.h>
#include "src/lcd/jd9365_lcd.h"

static lv_disp_draw_buf_t s_drawBuf;
static lv_color_t *s_buf1 = nullptr;
static lv_color_t *s_buf2 = nullptr;
jd9365_lcd g_lcd(LCD_RST);

static void dispFlush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    const int x1 = area->x1;
    const int y1 = area->y1;
    const int x2 = area->x2 + 1;
    const int y2 = area->y2 + 1;
    g_lcd.lcd_draw_bitmap(x1, y1, x2, y2, (uint16_t *)&color_p->full);
    lv_disp_flush_ready(disp);
}

bool hwDisplayInit() {
    g_lcd.begin();
    g_lcd.example_bsp_init_lcd_backlight();
    g_lcd.example_bsp_set_lcd_backlight(100);
    Serial.printf("[DISP] JD9365DA MIPI DSI %dx%d OK\n", g_lcd.width(), g_lcd.height());
    return true;
}

bool hwDisplayInitLVGL() {
    lv_init();
    size_t bufPixels = LCD_H_RES * LCD_V_RES;
    size_t bufBytes = bufPixels * sizeof(int32_t);
    s_buf1 = (lv_color_t *)heap_caps_malloc(bufBytes, MALLOC_CAP_SPIRAM);
    s_buf2 = (lv_color_t *)heap_caps_malloc(bufBytes, MALLOC_CAP_SPIRAM);
    if (!s_buf1 || !s_buf2) {
        Serial.printf("[DISP] PSRAM alloc FAILED (%u bytes)\n", bufBytes);
        return false;
    }
    lv_disp_draw_buf_init(&s_drawBuf, s_buf1, s_buf2, bufPixels);
    static lv_disp_drv_t dispDrv;
    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res = LCD_H_RES;
    dispDrv.ver_res = LCD_V_RES;
    dispDrv.flush_cb = dispFlush;
    dispDrv.draw_buf = &s_drawBuf;
    dispDrv.full_refresh = true;
    lv_disp_drv_register(&dispDrv);
    Serial.println("[DISP] LVGL OK (ARGB8888 full_refresh)");
    return true;
}

void hwDisplayBrightness(uint8_t lvl) {
    g_lcd.example_bsp_set_lcd_backlight(lvl > 0 ? lvl : 0);
}

void hwDisplaySleep(bool off) {
    if (off) {
        g_lcd.example_bsp_set_lcd_backlight(0);
    } else {
        g_lcd.example_bsp_set_lcd_backlight(100);
    }
}
