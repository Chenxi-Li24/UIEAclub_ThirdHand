# ESP32-S3 外骨骼固件

ESP32-S3 (Waveshare ESP32-S3-Touch-LCD-1.69 V2.1) 上的 PlatformIO 固件，集成 **LVGL 触摸屏 UI + 传感器采集 + Web API**，通过 ESP32-P4 控制终端间接控制法奥 FR5 机器人。

## 功能

- **LVGL 5 页滑动 UI**：主页（关节角）/ 机器人控制 / 系统状态 / 设置 / 更多；WiFi 与蜂鸣器为按需打开的子页面
- **机器人控制**：经 P4 控制终端转发 ServoJ 伺服命令（UDP 20008）
- **UDP 命令发送**（端口 20008）：向 P4 控制终端发送运动指令
- **Web API**（端口 80）：REST + mDNS，远程查询/控制
- **传感器**：QMI8658 IMU（加速度+陀螺仪）
- **自检系统**：9 个预设位置依次执行
- **旋律播放**：RTTTL 格式，PWM 蜂鸣器
- **电池监测**：18650 分压 ADC 检测

## 硬件

| 组件 | 型号 | 接口 |
|---|---|---|
| 主控 | ESP32-S3 Xtensa LX7 双核 240MHz | 8MB Flash, 8MB OPI PSRAM, 320KB SRAM |
| 屏幕 | ST7789V 1.69" 240×280 | SPI |
| 触摸 | CST816T 电容触控 | I2C 0x15 |
| IMU | QMI8658 6 轴 | I2C 0x6B |
| LED | WS2812 RGB | NeoPixel GPIO 21 |
| 蜂鸣器 | 无源 PWM | GPIO 42 |
| 电池 | 18650 分压检测 | ADC GPIO 1 |

### 引脚分配

```
SPI 屏幕:  GPIO 6(SCLK) 7(MOSI) 5(CS) 4(DC) 8(RST) 15(BL)
I2C 总线:  GPIO 11(SDA) 10(SCL)  →  QMI8658(0x6B) + CST816T(0x15)
PWM 蜂鸣器: GPIO 42
WS2812:    GPIO 21
BOOT:      GPIO 0
电池 ADC:  GPIO 1
```

## 文件结构

```
s3-exo/
├── platformio.ini              # PlatformIO 配置
├── lv_conf.h                   # LVGL 配置
├── no_ota_8mb.csv              # 分区表
├── tools/
│   └── fairino_cmd.py          # 命令行测试工具
└── src/
    ├── main.cpp                # 集成入口（setup + loop）
    ├── config.h                # WiFi/机器人/自检参数
    ├── stats.h                 # NVS 配置（Settings + WiFi 5 槽）
    ├── hw/                     # 硬件抽象层
    │   ├── display.*           # ST7789V + LVGL 渲染
    │   ├── input.*             # CST816T 触摸 → LVGL
    │   ├── imu.*               # QMI8658 I2C 驱动
    │   ├── audio.*             # PWM 蜂鸣器 + AI 语音预留
    │   ├── power.*             # 18650 电池 ADC
    │   ├── melody.*            # RTTTL 旋律播放
    │   └── pins.h              # 引脚定义
    ├── robot/                  # 机器人协议层
    │   ├── fairino_udp.*       # 法奥 UDP 帧协议（端口 20007）
    │   ├── safe_motion.*       # 16ms S 曲线插补 + 分轴限速 + 20°/s 硬限速
    │   └── cnde_client.*       # CNDE 状态回读（端口 20005）
    ├── ui/                     # LVGL GUI（5 页滑屏）
    │   ├── ui_core.*           # 屏幕管理器
    │   ├── ui_home.*           # 主页：6 轴关节角 + CNDE + WiFi
    │   ├── ui_robot.*          # 机器人控制：Jog / 自检 / Servo
    │   ├── ui_settings.*       # 设置：亮度 / 休眠 / IP
    │   ├── ui_wifi.*           # WiFi 扫描 / 连接
    │   ├── ui_system.*         # 系统信息
    │   └── ui_melody.*         # 旋律键盘
    ├── wifi_manager.*          # WiFi 状态机 + 静态 IP
    └── web_server.*            # REST API + mDNS + log
```

## 配置

编辑 [src/config.h](src/config.h)：

```c
#define WIFI_SSID       "ZTE-P5cS5Y"
#define WIFI_PASS       "12345678"
#define STATIC_IP       "192.168.58.102"
#define ROBOT_IP        "192.168.58.2"
#define ROBOT_UDP_PORT  20007

#define SELF_TEST_SETTLE_MS 5000
#define SELF_TEST_TIMEOUT   300000
```

所有 Jog、UDP 目标与自检动作都通过 `SafeServoMotion` 下发：`cmdT=0.016s`，J1-J6 指令速度统一限制为 `20°/s`，单周期最大约 `0.32°`；六轴加速度统一限制为 `10°/s²`，并限制加加速度。SDK 未开放的 `acc`、`vel`、`filterT`、`gain` 固定为 `0`。

## 通信端口

| 端口 | 协议 | 方向 | 用途 |
|---|---|---|---|
| 80 | HTTP | 入站 | Web API + REST |
| 20005 | TCP | 入站 ← 法奥控制器 | CNDE 关节状态回读 |
| 20007 | UDP | 出站 → 法奥控制器 | 安全插补后的 ServoJ 指令 |
| 20008 | UDP | 入站 | 控制命令服务器 |

> 当前 `s3-exo` 是可直连法奥控制器的旧控制终端；主系统仍建议使用 `p4-controller` 作为唯一机器人控制入口。

## 编译烧录

```bash
cd firmware/s3-exo
pio run --environment esp32-s3-robot        # 编译
pio run --target upload                      # 烧录
pio device monitor --baud 115200             # 串口监视
```

## 技术栈

| 类别 | 技术 |
|---|---|
| MCU | ESP32-S3 (240MHz, 8MB Flash) |
| 框架 | Arduino (PlatformIO) |
| GUI | LVGL 8.4 + Arduino_GFX |
| 网络 | WiFi STA + ESPAsyncWebServer + mDNS |
| 机器人协议 | 经 P4 控制终端转发（UDP 20008） |
| 配置存储 | NVS (namespace "robot") |
| LED | Adafruit NeoPixel |
