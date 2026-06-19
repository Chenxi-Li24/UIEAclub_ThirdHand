// src/hw/imu.cpp — QMI8658 6-axis IMU (I2C 0x6B)
// Waveshare ESP32-S3-Touch-LCD-1.69 V2.1 板载
// 裸 I2C 驱动，基于 SensorLib 官方寄存器映射
//
// QMI8658 register map:
//   0x02 CTRL1: interface control (addr_ai, big_endian, etc.)
//   0x03 CTRL2: accelerometer config (range[6:4], ODR[3:0])
//   0x04 CTRL3: gyroscope config (range[6:4], ODR[3:0])
//   0x08 CTRL7: sensor enable (accel_en[0], gyro_en[1])
//   0x2E STATUS0: data ready (accel[0], gyro[1])
//   0x35-0x3A: accel data (AX_L/H, AY_L/H, AZ_L/H)

#include "hw/imu.h"

#include "hw/pins.h"

#include <Arduino.h>
#include <Wire.h>

// ── QMI8658 registers ─────────────────────────────────────────────────
#define QMI_ADDR         0x6B
#define QMI_WHO_AM_I     0x00

#define QMI_CTRL1        0x02   // Interface control
#define QMI_CTRL2        0x03   // Accelerometer config
#define QMI_CTRL3        0x04   // Gyroscope config
#define QMI_CTRL7        0x08   // Sensor enable

#define QMI_STATUS0      0x2E   // Data ready status
#define QMI_AX_L         0x35   // Accel X low byte (6 bytes total)

// ── Register values ───────────────────────────────────────────────────
// CTRL1: addr_ai=1 (auto-increment for burst reads)
#define CTRL1_VAL  0x40   // bit6=1: address auto-increment enable

// CTRL2: accel ±4g, ODR 896.8Hz
//   bits[7]: self_test=0
//   bits[6:4]: range = 001 (±4g)
//   bits[3:0]: ODR = 0011 (896.8Hz)
#define CTRL2_VAL  0x13   // acc_range=±4g, acc_odr=896.8Hz

// CTRL3: gyro ±512dps, ODR 896.8Hz
//   bits[7]: self_test=0
//   bits[6:4]: range = 101 (±512dps)
//   bits[3:0]: ODR = 0011 (896.8Hz)
#define CTRL3_VAL  0x53   // gyr_range=±512dps, gyr_odr=896.8Hz

// CTRL7: enable accel + gyro
#define CTRL7_VAL  0x03   // bit0=accel_en, bit1=gyro_en

// Scale: ±4g → 4.0 / 32768 = 0.00012207 g/LSB
// Inverse: 32768 / 4.0 = 8192 LSB/g
#define ACCEL_SCALE  8192.0f

static bool s_ok = false;

static uint8_t qmiRead(uint8_t reg) {
  Wire.beginTransmission(QMI_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)QMI_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

static void qmiWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(QMI_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static int16_t qmiRead16(uint8_t reg) {
  Wire.beginTransmission(QMI_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)QMI_ADDR, (uint8_t)2);
  if (Wire.available() < 2) return 0;
  uint8_t lo = Wire.read(), hi = Wire.read();
  return (int16_t)((hi << 8) | lo);
}

// ── Public API ────────────────────────────────────────────────────────
bool hwImuInit() {
  // I2C scan
  Serial.print("hwImu: I2C scan → ");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      if (found > 0) Serial.print(", ");
      Serial.printf("0x%02X", addr);
      found++;
    }
  }
  if (found == 0) Serial.println("no devices!");
  else Serial.println();

  uint8_t who = qmiRead(QMI_WHO_AM_I);
  Serial.printf("hwImu: WHO_AM_I=0x%02X (expect 0x05)\n", who);

  if (who != 0x05) {
    Serial.println("hwImu: QMI8658 not found");
    return false;
  }

  // Soft reset via CTRL7
  qmiWrite(QMI_CTRL7, 0xB0);
  delay(20);

  // Configure interface (auto-increment for burst reads)
  qmiWrite(QMI_CTRL1, CTRL1_VAL);

  // Configure accelerometer (CTRL2) and gyroscope (CTRL3)
  qmiWrite(QMI_CTRL2, CTRL2_VAL);
  qmiWrite(QMI_CTRL3, CTRL3_VAL);

  // Enable both sensors
  qmiWrite(QMI_CTRL7, CTRL7_VAL);
  delay(50);

  // Verify
  uint8_t c1 = qmiRead(QMI_CTRL1);
  uint8_t c2 = qmiRead(QMI_CTRL2);
  uint8_t c3 = qmiRead(QMI_CTRL3);
  uint8_t c7 = qmiRead(QMI_CTRL7);
  uint8_t st = qmiRead(QMI_STATUS0);

  Serial.printf("hwImu: CTRL1=0x%02X CTRL2=0x%02X CTRL3=0x%02X CTRL7=0x%02X STATUS0=0x%02X\n",
                c1, c2, c3, c7, st);

  s_ok = (c7 == CTRL7_VAL) && (c2 == CTRL2_VAL);
  Serial.println(s_ok ? "hwImu: QMI8658 configured OK" : "hwImu: QMI8658 config FAIL");
  return s_ok;
}

void hwImuAccel(float *ax, float *ay, float *az) {
  if (!s_ok) { *ax = *ay = *az = 0; return; }
  int16_t rawX = qmiRead16(QMI_AX_L);
  int16_t rawY = qmiRead16(QMI_AX_L + 2);
  int16_t rawZ = qmiRead16(QMI_AX_L + 4);
  *ax = (float)rawX / ACCEL_SCALE;
  *ay = (float)rawY / ACCEL_SCALE;
  *az = (float)rawZ / ACCEL_SCALE;
}

float g_imu_ax = 0, g_imu_ay = 0, g_imu_az = 0;

void hwImuRead() {
  hwImuAccel(&g_imu_ax, &g_imu_ay, &g_imu_az);
}

// Debug
void hwImuDiag(uint8_t *who, uint8_t *st, uint8_t *c1, uint8_t *c7) {
  *who = 0x05;  // known
  *st  = qmiRead(QMI_STATUS0);
  *c1  = qmiRead(QMI_CTRL2);  // accel config
  *c7  = qmiRead(QMI_CTRL7);
}

void hwImuRaw(int16_t *rx, int16_t *ry, int16_t *rz) {
  if (!s_ok) { *rx = *ry = *rz = 0; return; }
  *rx = qmiRead16(QMI_AX_L);
  *ry = qmiRead16(QMI_AX_L + 2);
  *rz = qmiRead16(QMI_AX_L + 4);
}

void hwImuRegDump() {
  Serial.print("hwImu: REGDUMP ");
  for (uint8_t r = 0; r <= 0x40; r++) {
    uint8_t v = qmiRead(r);
    Serial.printf("%02X:%02X ", r, v);
    if ((r + 1) % 16 == 0) Serial.println();
  }
  Serial.println();
}
