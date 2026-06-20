// hw/input.cpp - GSL3680 I2C touch -> LVGL indev
#include "hw/input.h"
#include "pins_config.h"
#include <Arduino.h>
#include <lvgl.h>
#include "src/touch/gsl3680_touch.h"

static gsl3680_touch s_touch(TP_I2C_SDA, TP_I2C_SCL, TP_RST, TP_INT);

static void touchRead(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    static uint16_t lastX = 0, lastY = 0;
    uint16_t touchX = 0, touchY = 0;
    bool touched = s_touch.getTouch(&touchX, &touchY);
    if (touched) {
        touchX = LCD_H_RES - touchX;
        lastX = touchX; lastY = touchY;
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
        touchX = lastX; touchY = lastY;
    }
    data->point.x = touchX;
    data->point.y = touchY;
}

bool hwInputInit() {
    s_touch.begin();
    static lv_indev_drv_t indevDrv;
    lv_indev_drv_init(&indevDrv);
    indevDrv.type = LV_INDEV_TYPE_POINTER;
    indevDrv.read_cb = touchRead;
    lv_indev_drv_register(&indevDrv);
    Serial.println("[TOUCH] GSL3680 OK");
    return true;
}

void hwInputUpdate() {}
