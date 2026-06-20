# GY-85 IMU Cube Visualizer Demo

This folder contains a minimum IMU visualization demo for ThirdHand.

## Completed work

- XIAO ESP32S3 reads GY-85 9-axis IMU through I2C
- I2C scanner detected 0x1E, 0x53 and 0x68
- Serial output provides 9-axis IMU data
- Python reads serial data from COM5
- CSV logging is supported
- A stable 3D cube tilt visualizer is included

## Data format

The ESP32S3 outputs 9 values:

| Value | Meaning |
|---|---|
| ax | Acceleration X |
| ay | Acceleration Y |
| az | Acceleration Z |
| gx_cal | Calibrated gyroscope X |
| gy_cal | Calibrated gyroscope Y |
| gz_cal | Calibrated gyroscope Z |
| mx | Magnetometer X |
| my | Magnetometer Y |
| mz | Magnetometer Z |

Serial output order:

ax, ay, az, gx_cal, gy_cal, gz_cal, mx, my, mz

## Hardware

| Module | Model |
|---|---|
| MCU | Seeed Studio XIAO ESP32S3 |
| IMU | GY-85 9-axis IMU |
| Communication | I2C + Serial |

## Wiring

| GY-85 | XIAO ESP32S3 |
|---|---|
| 3.3V | 3V3 |
| GND | GND |
| SDA | D4 / GPIO5 |
| SCL | D5 / GPIO6 |

## Files

| File | Description |
|---|---|
| imu_cube_visualizer_stable.py | Stable cube tilt visualizer |
| imu_logger.py | Serial logger for saving IMU data to CSV |
| requirements.txt | Python dependencies |
| sample_imu_log.csv | Example static test data |

## How to run

1. Upload the ESP32 IMU reader program to XIAO ESP32S3.
2. Close Arduino Serial Monitor.
3. Install Python dependencies.
4. Run the cube visualizer script.
5. Tilt the IMU and observe the cube motion.

## Note

The current visualizer mainly shows stable roll/pitch tilt based on gravity direction.

Yaw is locked in the stable version to avoid gyro integration drift.

This is a basic interface and visualization demo, not a final high-precision AHRS system.
