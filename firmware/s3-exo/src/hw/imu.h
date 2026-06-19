// src/hw/imu.h — QMI8658 6-axis IMU

#pragma once
#include <stdint.h>

bool hwImuInit();
void hwImuAccel(float *ax, float *ay, float *az);  // 返回 g 值

// Global IMU state — updated by hwImuRead(), read by UI
extern float g_imu_ax, g_imu_ay, g_imu_az;
void hwImuRead();  // 读一次传感器，写入 g_imu_ax/ay/az

// Debug: read back init diagnostics
void hwImuDiag(uint8_t *who, uint8_t *st1, uint8_t *c1, uint8_t *c7);
void hwImuRaw(int16_t *rx, int16_t *ry, int16_t *rz);
void hwImuRegDump();
