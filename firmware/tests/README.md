# ESP32-S3 本地验证程序

这些独立 PlatformIO 项目用于验证 ESP32-S3-Touch-LCD-1.69 的基础功能，不依赖 `firmware/s3-exo` 主程序。

## 项目

| 目录 | 用途 | 环境名 |
|---|---|---|
| `esp32s3_serial_test` | USB CDC 串口连续计数 | `esp32-s3-serial-test` |
| `esp32s3_wifi_scan_test` | 扫描附近 2.4GHz WiFi，不连接网络 | `esp32-s3-wifi-scan-test` |
| `esp32s3_wifi_connect_test` | 连接指定 WiFi，输出 IP 与 RSSI | `esp32-s3-wifi-connect-test` |
| `esp32s3_gy85_test` | 检测并读取外接 GY-85 IMU | `esp32-s3-gy85-test` |

## 通用命令

进入对应项目目录后执行：

```powershell
# 编译
pio run --environment <环境名>

# 烧录到 COM5
pio run --environment <环境名> --target upload --upload-port COM5

# 串口监视
pio device monitor --port COM5 --baud 115200
```

如果 `pio` 不在 PATH，可使用：

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --environment <环境名>
```

## WiFi 凭据

WiFi 连接测试不包含真实账号或密码。复制：

```text
esp32s3_wifi_connect_test/src/secrets.example.h
```

为：

```text
esp32s3_wifi_connect_test/src/secrets.h
```

然后仅在本地填写 SSID 和密码。`secrets.h` 已被 `.gitignore` 排除。

## GY-85 接线

| ESP32-S3 | GY-85 |
|---|---|
| `5V` | `VCC_IN` |
| `G` | `GND` |
| `SCL` | `SCL` |
| `SDA` | `SDA` |

GY-85 测试程序使用 GPIO11（SDA）和 GPIO10（SCL），并读取 ADXL345、ITG-3205 以及 HMC5883L/QMC5883L 兼容磁力计。
