// audio_i2c.cpp - Shared I2C for ES7210 + ES8311 (reuses touch's I2C_NUM_0)
// I2C_NUM_0 already installed by gsl3680_touch::begin() — no re-init needed
// Uses explicit i2c_cmd_link API to avoid "null address" errors from
// i2c_master_write_to_device(NULL,0) on pre-initialized buses
#include "voice/audio_i2c.h"
#include "pins_config.h"
#include <driver/i2c.h>
#include <Arduino.h>

static bool s_ready = false;
static portMUX_TYPE s_i2cSpinlock = portMUX_INITIALIZER_UNLOCKED;

bool audioI2cInit(int sda, int scl) {
    if (s_ready) return true;

    // ── Install I2C driver if not already installed ────────
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.scl_io_num = scl;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;
    esp_err_t ret = i2c_param_config(I2C_NUM_0, &conf);
    if (ret != ESP_OK) {
        // Driver might be installed; try install first
        ret = i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
        if (ret == ESP_OK) {
            ret = i2c_param_config(I2C_NUM_0, &conf);
        }
    } else {
        // param_config OK, install driver if not already
        ret = i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
        if (ret == ESP_ERR_INVALID_STATE) ret = ESP_OK;  // Already installed
    }
    if (ret != ESP_OK) {
        Serial.printf("[AudioI2C] Driver install FAILED: %s\n", esp_err_to_name(ret));
        return false;
    }
    s_ready = true;
    Serial.printf("[AudioI2C] I2C_NUM_0 installed on GPIO%d/GPIO%d\n", sda, scl);

    // ── I2C bus scan ─────────────────────────────────────────────────
    Serial.print("[AudioI2C] Scanning I2C bus: ");
    int found = 0;
    for (int addr = 1; addr < 128; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK) {
            Serial.printf("0x%02X ", addr);
            found++;
        }
    }
    if (found) Serial.printf("(%d devices)\n", found);
    else       Serial.println("(none!)");
    return true;
}

bool audioI2cWriteReg(uint8_t devAddr, uint8_t reg, uint8_t val) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK;
}

bool audioI2cProbe(uint8_t devAddr) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK;
}
