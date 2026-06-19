# UIEA ThirdHand — 开发日志

## 2025-06-19 固件 v2.0 重写

### 变更概要
将 deskpet-firmware 的完整硬件层移植到 Fairino 机器人控制器，替换原简陋固件。

**硬件目标**: Waveshare ESP32-S3-Touch-LCD-1.69 V2.1

### 架构

```
┌─────────────────────────────────────────────┐
│  main.cpp  — 集成入口                        │
│  setup(): 硬件→WiFi→机器人→Web→UI            │
│  loop():  LVGL→WiFi→触摸→CNDE→UDP→自检      │
├─────────────────────────────────────────────┤
│  Hardware Layer (hw/)          Robot Layer  │
│  ├─ display.*  ST7789V+LVGL   ├─ robot/fairino_udp.* │
│  ├─ input.*    CST816T touch   │   UDP帧 port 20007 │
│  ├─ imu.*      QMI8658 I2C     └─ robot/cnde_client.* │
│  ├─ audio.*    PWM buzzer         TCP状态 port 20005 │
│  ├─ power.*    Battery ADC                    │
│  └─ melody.*   Buzzer tunes                  │
├─────────────────────────────────────────────┤
│  UI Layer (ui/)          Network Layer       │
│  ├─ ui_core.*  5屏管理器  ├─ wifi_manager.*  │
│  ├─ ui_home.*  关节角主页  │   多凭据+静态IP  │
│  ├─ ui_robot.* Jog控制屏  └─ web_server.*    │
│  ├─ ui_settings.* 设置页     REST API+log    │
│  ├─ ui_wifi.*  WiFi配网页                    │
│  ├─ ui_system.* 系统信息                     │
│  └─ ui_melody.* 旋律键盘                     │
└─────────────────────────────────────────────┘
```

### 文件清单 (firmware/s3-exo/)

| 文件 | 说明 |
|------|------|
| `platformio.ini` | env:esp32-s3-robot, LVGL8.4, GFX, ArduinoJson7, NeoPixel |
| `lv_conf.h` | LVGL 8.4 配置 (4字体, DMA双缓冲) |
| `no_ota_8mb.csv` | 8MB 分区表 |
| `src/config.h` | WiFi/Robot 默认值, 自检参数 |
| `src/stats.h` | NVS Settings + WiFi 5槽凭据 (header-only) |
| `src/main.cpp` | 完整集成入口 (setup/loop/LED/自检FSM/UDP命令) |
| `src/hw/pins.h` | BOARD_WAVESHARE 引脚映射 |
| `src/hw/display.*` | ST7789V GFX + LVGL 渲染 |
| `src/hw/input.*` | CST816T 触摸→LVGL |
| `src/hw/imu.*` | QMI8658 裸I2C |
| `src/hw/audio.*` | PWM蜂鸣器 + AI语音预留 |
| `src/hw/power.*` | 18650电池ADC |
| `src/hw/melody.*` | RTTTL旋律播放 |
| `src/robot/fairino_udp.*` | 法奥UDP帧协议 |
| `src/robot/cnde_client.*` | CNDE实时状态TCP |
| `src/wifi_manager.*` | WiFi状态机 + 静态IP + 多凭据 |
| `src/web_server.*` | ESPAsyncWebServer REST API + mDNS |
| `src/ui/ui_core.*` | 5屏滑动管理器 |
| `src/ui/ui_home.*` | 主页: 6轴关节角+CNDE状态+WiFi信息 |
| `src/ui/ui_robot.*` | 机器人控制: 6轴Jog+自检+Servo命令 |
| `src/ui/ui_settings.*` | 设置: WiFi/亮度/休眠/机器人IP |
| `src/ui/ui_wifi.*` | WiFi扫描/连接/凭据管理 |
| `src/ui/ui_system.*` | 系统信息页 |
| `src/ui/ui_melody.*` | 旋律键盘 |
| `tools/fairino_cmd.py` | PC端命令工具 |

### 编译 & 烧录

```bash
cd firmware/s3-exo
pio run --environment esp32-s3-robot
pio run --environment esp32-s3-robot --target upload
pio device monitor --baud 115200
```

### 网络端口

| 端口 | 协议 | 方向 | 用途 |
|------|------|------|------|
| 80 | HTTP | 入站 | Web UI + REST API |
| 20005 | TCP | 出站→机器人 | CNDE 状态回读 |
| 20007 | UDP | 出站→机器人 | 伺服控制帧 |
| 20008 | UDP | 入站 | PC 命令服务器 |

### WiFi

- 默认静态 IP: `192.168.58.100`
- 凭据存 NVS (namespace "robot"), 最多5个, 自动切换
- BLE "Fairino-Mini" 广播供配网

### 自检

9个预定义位姿, BOOT按钮(IO0)触发或UDP `selftest` 命令
超时 300s, LED 黄色=运行中, 绿色=完成, 红色=失败

### 已知问题

- LVGL Montserrat 字体不含 Unicode 箭头, UI 统一用 ASCII `>`
- 必须 WiFi 连接 + 机器人上电后操控功能才可用

---

## 2025-06-20 仓库重组 v2.1

### 变更概要
将单模块项目重组为规范的单仓库多模块结构，加上协作基础设施。

### 结构变化

```
旧: UIEAclub_ThirdHand/
    └── esp32-fairino-client/          ← 所有代码在一个目录

新: ThirdHand/
    ├── firmware/s3-exo/              ← 外骨骼固件
    ├── sdk/                          ← 第三方 SDK
    ├── docs/                         ← 文档
    ├── .github/ISSUE_TEMPLATE/       ← Issue/PR 模板
    ├── CONTRIBUTING.md               ← 贡献指南
    └── README.md                     ← 项目总览
```

### 源码内部分层

| 原路径 | 新路径 |
|--------|--------|
| `src/fairino_udp.*` | `src/robot/fairino_udp.*` |
| `src/cnde_client.*` | `src/robot/cnde_client.*` |
| `src/hw/*` | 不动 |
| `src/ui/*` | 不动 |
| `src/exo/` | 新建（预留） |

### 新增文件

| 文件 | 用途 |
|------|------|
| `CONTRIBUTING.md` | 贡献指南（分支命名/Commit格式/编译验证） |
| `.github/ISSUE_TEMPLATE/bug_report.md` | Bug 报告模板 |
| `.github/ISSUE_TEMPLATE/feature_request.md` | 功能请求模板 |
| `.github/PULL_REQUEST_TEMPLATE.md` | PR 模板 |
| `docs/architecture.md` | 系统架构文档 |

### 编译验证

```bash
pio run -d firmware/s3-exo --environment esp32-s3-robot
# RAM 40.9%, Flash 28.8% — 通过
```
