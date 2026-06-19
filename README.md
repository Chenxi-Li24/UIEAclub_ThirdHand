# 🦾 ThirdHand — 桌面级仿人机械臂

> **UIEA 俱乐部** | 基于法奥机械臂 + ESP32 外骨骼 + AI 语音的仿人机械臂系统

| | | | |
|---|---|---|---|
| ✅ 固件 v2.0 | 🔜 外骨骼 IMU+磁编码 | 📋 P4 终端 | 📋 语音 AI Agent |

---

## 📋 项目概览

本项目构建一套**完整的桌面级仿人机械臂控制系统**，包含三大部分：

| 层次 | 硬件 | 功能 |
|------|------|------|
| **终端**（规划） | ESP32-P4 + 10.1" 大屏 + 摄像头 + 喇叭 | 人机交互、语音 AI、视觉识别 |
| **外骨骼**（当前） | ESP32-S3 + 1.69" 触摸屏 + 4×IMU + 8×磁编码器 | 传感器融合、机器人实时控制 |
| **机械臂** | 法奥 FR5/FR10 六轴协作机器人 | 实体执行、CNDE 实时状态回读 |

---

## 🏗️ 系统架构

```
┌─────────────────────────────────────────────────────┐
│               ESP32-P4 终端（规划中）                 │
│  10.1" 大屏 · 摄像头 · 喇叭 · 电池                   │
│  ┌───────────────────────────────────────────┐      │
│  │       AI Agent (LLM + 语音 + 视觉)        │      │
│  └──────────────┬────────────────────────────┘      │
└─────────────────┼────────────────────────────────────┘
                  │ WiFi UDP
                  ▼
┌─────────────────────────────────────────────────────┐
│          ESP32-S3 外骨骼控制器（当前）                │
│     1.69" ST7789V 触摸屏 · 4×IMU · 8×磁编码器        │
│                                                     │
│  ┌───────────┐  ┌───────────┐  ┌─────────────────┐  │
│  │  传感器层  │  │  通信层   │  │   UI 层         │  │
│  │  IMU 融合  │  │  WiFi STA │  │  主页 (关节角)  │  │
│  │  磁编码器  │  │  UDP 20007│  │  机器人控制     │  │
│  │  PWM 蜂鸣器│  │  TCP 20005│  │  设置/WiFi/     │  │
│  │  18650 电池│  │  Web 80   │  │  旋律          │  │
│  │  WS2812 LED│  │  UDP 20008│  │  5 页滑动触控   │  │
│  └───────────┘  └───────────┘  └─────────────────┘  │
└─────────────────┼────────────────────────────────────┘
                  │ UDP
                  ▼
┌─────────────────────────────────────────────────────┐
│            法奥 FR5 / FR10 机械臂                    │
│  6 轴协作机器人 · ServoJ 伺服控制 · CNDE 状态反馈    │
│  关节角 · 速度 · 扭矩 · TCP 位置 · 错误码            │
└─────────────────────────────────────────────────────┘
```

---

## 🔧 外骨骼硬件详解

### Waveshare ESP32-S3-Touch-LCD-1.69 V2.1

| 组件 | 型号 | 接口 | 说明 |
|------|------|------|------|
| **主控** | ESP32-S3 Xtensa LX7 双核 240MHz | — | 8MB Flash, 320KB RAM, WiFi+BLE |
| **屏幕** | ST7789V 1.69" 240×280 | SPI | LVGL 8.4 双缓冲 DMA |
| **触摸** | CST816T 电容触控 | I2C 0x15 | 5 页滑动手势 |
| **IMU** | QMI8658 6 轴 | I2C 0x6B | 加速度+陀螺仪 |
| **LED** | WS2812 RGB | NeoPixel | 状态指示 |
| **蜂鸣器** | 无源 PWM | GPIO 42 | 提示音/旋律 |
| **电池** | 18650 分压检测 | ADC GPIO1 | 电压/百分比 |

### 总线分配

```
SPI 屏幕:  GPIO 6(SCLK) 7(MOSI) 5(CS) 4(DC) 8(RST) 15(BL)
I2C 总线:  GPIO 11(SDA) 10(SCL)  →  QMI8658(0x6B) + CST816T(0x15)
PWM 蜂鸣器: GPIO 42
WS2812:    GPIO 21
BOOT:      GPIO 0
电池 ADC:  GPIO 1

空闲引脚:  GPIO 2, 3, 9, 12, 16, 17, 18  ← 给 I2S 麦克风/功放
```

---

## 🗂️ 软件架构

```
firmware/s3-exo/src/
├── main.cpp                  ← 集成入口（setup + loop）
├── config.h                  ← WiFi/机器人/自检参数
├── stats.h                   ← NVS 配置（Settings + WiFi 5 槽）
│
├── hw/                       ← 硬件抽象层（DeskPet 移植）
│   ├── display.*             ← ST7789V + LVGL 渲染
│   ├── input.*               ← CST816T 触摸 → LVGL
│   ├── imu.*                 ← QMI8658 I2C 驱动
│   ├── audio.*               ← PWM 蜂鸣器 + AI 语音预留
│   ├── power.*               ← 18650 电池 ADC
│   └── melody.*              ← RTTTL 旋律播放
│
├── robot/                    ← 机器人层
│   ├── fairino_udp.*         ← 法奥 UDP 帧协议（port 20007）
│   └── cnde_client.*         ← CNDE 状态回读（TCP 20005）
│
├── exo/                      ← 外骨骼（预留）
│   └── ...                   ← IMU 融合 + 磁编码器驱动
│
├── ui/                       ← LVGL GUI（5 页滑屏）
│   ├── ui_core.*             ← 屏幕管理器
│   ├── ui_home.*             ← 主页：6 轴关节角 + CNDE + WiFi
│   ├── ui_robot.*            ← 机器人控制：Jog / 自检 / Servo
│   ├── ui_settings.*         ← 设置：亮度 / 休眠 / IP
│   ├── ui_wifi.*             ← WiFi 扫描 / 连接
│   ├── ui_system.*           ← 系统信息
│   └── ui_melody.*           ← 旋律键盘
│
├── wifi_manager.*            ← WiFi 状态机 + 静态 IP
└── web_server.*              ← REST API + mDNS + log
```

---

## 🚀 快速开始

```bash
# 1. 编译
cd firmware/s3-exo
pio run --environment esp32-s3-robot

# 2. 烧录（自动识别 COM 口）
pio run --target upload

# 3. 串口监视
pio device monitor --baud 115200
```

### 默认配置

| 参数 | 值 |
|------|-----|
| 外骨骼 IP | `192.168.58.100`（静态） |
| 机器人 IP | `192.168.58.2` |
| WiFi SSID | `Xiaomi_7D5E` |
| 上电 LED | 蓝色呼吸 → 绿色（WiFi 连接后） |

---

## 🌐 通信端口

| 端口 | 协议 | 方向 | 用途 | 格式 |
|------|------|------|------|------|
| **80** | HTTP | 入站 | Web API + REST | JSON |
| **20005** | TCP | 出站 → 机器人 | CNDE 实时状态 | 二进制帧 |
| **20007** | UDP | 出站 → 机器人 | 伺服控制 | `/f/bIII...III/b/f` |
| **20008** | UDP | 入站 | PC 命令服务器 | 文本 |

### UDP 命令服务器（port 20008）

```bash
# PC 端发送命令
echo "status" | ncat -u 192.168.58.100 20008
# → J1:60.5 J2:-69.6 J3:-91.0 J4:-84.3 J5:100.5 J6:-8.9 robot:0 prog:0 err:0/0

echo "help" | ncat -u 192.168.58.100 20008
# → help | status | test | selftest | servo start | servo end | servo j1 <j1-j6>

echo "servo j1 30 -70 -90 -80 90 0" | ncat -u 192.168.58.100 20008
```

---

## 🧪 自检系统

9 个预定义位姿，按顺序执行：

| 段 | 说明 | 触发 | LED |
|----|------|------|-----|
| 1-9 | 关节遍历 9 个位姿 | BOOT 按钮 或 `selftest` 命令 | 🟡 黄色 |
| 完成 | 回到原点 | 自动 | ⚫ 熄灭 |
| 超时 | 300s 无响应 | — | 🔴 红色呼吸 |

---

## 🛠️ 技术栈

| 类别 | 技术 |
|------|------|
| MCU | ESP32-S3 (espressif32 @ 240MHz) |
| 框架 | Arduino (PlatformIO) |
| GUI | LVGL 8.4 + Arduino_GFX |
| 网络 | WiFi STA + ESPAsyncWebServer + mDNS |
| 机器人协议 | 法奥 UDP 帧 `/f/bIII...III/b/f` |
| 状态回读 | CNDE TCP 实时状态（双精度浮点关节角） |
| 配置存储 | NVS (namespace "robot") |
| LED | Adafruit NeoPixel |
| 固件大小 | Flash 28.8% / RAM 40.9% |

---

## 📌 路线图

| 版本 | 内容 | 状态 |
|------|------|------|
| **v1.0** | 法奥 UDP 控制 + 串口 | ✅ 完成 |
| **v2.0** | LVGL 触摸屏 + WiFi + CNDE 状态 | ✅ 完成 |
| **v2.1** | 仓库重组 + 协作文件 | ✅ 完成 |
| **v3.0** | 外骨骼 IMU 融合 + 磁编码器驱动 | 🔜 进行中 |
| v3.1 | I2S 麦克风 + 功放（INMP441 + MAX98357） | 📋 规划 |
| v4.0 | ESP32-P4 终端 + 10.1" 大屏 + 摄像头 | 📋 规划 |
| v5.0 | 语音 LLM Agent（Claude / DeepSeek Tool Use） | 📋 规划 |

---

## 🤝 贡献

欢迎提交 Issue 和 PR！

- [贡献指南](CONTRIBUTING.md)
- [Bug 报告](.github/ISSUE_TEMPLATE/bug_report.md)
- [功能请求](.github/ISSUE_TEMPLATE/feature_request.md)
- [系统架构文档](docs/architecture.md)
- [开发日志](docs/DEVELOPMENT.md)
