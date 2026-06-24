# 🦾 ThirdHand — 桌面级仿人机械臂

> **UIEA 俱乐部** | 基于法奥机械臂 + ESP32 控制终端 + 外骨骼 + Web 可视化的仿人机械臂系统

| | | | | |
|---|---|---|---|---|
| ✅ P4 控制终端 | ✅ S3 外骨骼 | ✅ Web 控制面板 | 🔜 外骨骼 IMU+磁编码 | 📋 语音 AI Agent |

---

## 📋 项目概览

本项目构建一套**完整的桌面级仿人机械臂控制系统**，包含四大部分：

| 层次 | 硬件 | 功能 |
|------|------|------|
| **控制终端** | ESP32-P4 + UDP 命令中继 | 统一接收 Web/外骨骼指令，转发至法奥机器人并回读状态 |
| **外骨骼** | ESP32-S3 + 1.69" 触摸屏 + 4×IMU + 8×磁编码器 | 传感器融合，经 P4 控制终端控制机器人 |
| **Web 控制台** | 浏览器 (Three.js) + Node.js 代理 | 3D 可视化、关节控制、状态监控 |
| **机械臂** | 法奥 FR5/FR10 六轴协作机器人 | 实体执行、CNDE 实时状态回读 |

---

## 🏗️ 系统架构

```
【链路 A：Web 控制台】                            【链路 B：S3 外骨骼】

┌───────────────────────────┐
│   Web 控制台（浏览器）      │
│  Three.js · 关节滑块       │
└────────────┬──────────────┘
             │ WebSocket
             ▼
┌───────────────────────────┐
│   Node.js 代理服务器       │              ┌───────────────────────────┐
│  WebSocket ↔ UDP 桥接      │              │   ESP32-S3 外骨骼控制器    │
└────────────┬──────────────┘              │  1.69" 触摸屏 · 4×IMU      │
             │ UDP 20008                   │  8×磁编码器 · LVGL 5 页    │
             │                             └──────────────┬────────────┘
             │                                            │ UDP 20008
             │                                            │
             └──────────────────┬─────────────────────────┘
                                ▼
                ┌───────────────────────────┐
                │  ESP32-P4 控制终端（中继）  │
                │  UDP 20008 · Fairino 20007│
                │  CNDE TCP 20005 · 自检    │
                └──────────────┬────────────┘
                               │ UDP 20007 + TCP 20005
                               ▼
                ┌───────────────────────────┐
                │   法奥 FR5 / FR10 机械臂   │
                │  ServoJ · CNDE 状态反馈   │
                └───────────────────────────┘
```

> Web 控制台（链路 A）与 S3 外骨骼（链路 B）均通过 UDP 20008 将命令发送给 P4 控制终端，由 P4 统一转发至 FR5 机械臂并回读 CNDE 状态。

---

## 🗂️ 仓库结构

```
UIEAclub_ThirdHand/
├── firmware/                       # 固件
│   ├── p4-controller/              # ESP32-P4 控制终端（命令中继）
│   ├── s3-exo/                     # ESP32-S3 外骨骼（触摸屏 + 传感器）
│   └── modules/                    # 传感器与控制模块（IMU/激光/舵机）
│       └── imu_gy85_demo/
├── web-control/                    # Web 控制面板
│   ├── server/                     # Node.js 代理（WebSocket ↔ UDP）
│   └── web/                        # 前端（Three.js + URDF）
├── 3d-models/                      # 3D 建模文件（URDF/STL/GLB/STEP）
├── references/                     # 参考资料
│   ├── JC8012P4A1/                 # 屏幕硬件资料
│   └── sdk/                        # 法奥 C++ SDK
├── docs/                           # 开发文档
└── .github/                        # Issue/PR 模板
```

---

## 🚀 快速开始

### 1. Web 控制面板

```bash
cd web-control/server
npm install
npm start
# 浏览器打开 http://localhost:3000
```

### 2. ESP32-P4 控制终端固件

使用 Arduino IDE 打开 `firmware/p4-controller/p4-controller.ino`，选择 ESP32-P4 开发板，编译烧录。

### 3. ESP32-S3 外骨骼固件

```bash
cd firmware/s3-exo
pio run --environment esp32-s3-robot        # 编译
pio run --target upload                      # 烧录
pio device monitor --baud 115200             # 串口监视
```

---

## 🌐 通信端口

| 端口 | 协议 | 方向 | 用途 | 格式 |
|------|------|------|------|------|
| **3000** | HTTP/WS | 浏览器 ↔ Node.js | Web 控制面板 | JSON |
| **20005** | TCP | P4 → 机器人 | CNDE 实时状态回读 | 二进制帧 |
| **20007** | UDP | P4 → 机器人 | ServoJ 伺服控制 | `/f/bIII...III/b/f` |
| **20008** | UDP | Web/S3 → P4 | 命令服务器（P4 转发） | 文本 |
| **80** | HTTP | S3 外骨骼 Web API | REST + mDNS | JSON |

### UDP 命令（端口 20008）

```bash
echo "status" | ncat -u 192.168.58.100 20008
# → J1:60.5 J2:-69.6 J3:-91.0 J4:-84.3 J5:100.5 J6:-8.9 robot:0 prog:0 err:0/0

echo "servo j1 30 -70 -90 -80 90 0" | ncat -u 192.168.58.100 20008
echo "selftest" | ncat -u 192.168.58.100 20008
```

---

## 🛠️ 技术栈

| 类别 | 技术 |
|------|------|
| 控制终端 MCU | ESP32-P4 (JC8012P4A1) |
| 外骨骼 MCU | ESP32-S3 (240MHz, 8MB Flash) |
| 框架 | Arduino (PlatformIO) |
| Web 前端 | Three.js + URDFLoader |
| Web 后端 | Node.js + express + ws |
| GUI | LVGL 8.4 + Arduino_GFX |
| 机器人协议 | 法奥 UDP 帧 + CNDE TCP 状态 |
| LED | Adafruit NeoPixel (WS2812) |

---

## 📌 路线图

| 版本 | 内容 | 状态 |
|------|------|------|
| **v1.0** | 法奥 UDP 控制 + 串口 | ✅ 完成 |
| **v2.0** | S3 外骨骼 LVGL 触摸屏 + WiFi + CNDE | ✅ 完成 |
| **v2.1** | 仓库重组 + 协作基础设施 | ✅ 完成 |
| **v2.2** | P4 控制终端 + Web 控制面板重构 | ✅ 完成 |
| **v3.0** | 外骨骼 IMU 融合 + 磁编码器驱动 | 🔜 进行中 |
| v4.0 | 语音 LLM Agent（Claude / DeepSeek Tool Use） | 📋 规划 |

---

## 🤝 贡献

欢迎提交 Issue 和 PR！

- [贡献指南](CONTRIBUTING.md)
- [Bug 报告](.github/ISSUE_TEMPLATE/bug_report.md)
- [功能请求](.github/ISSUE_TEMPLATE/feature_request.md)
- [开发日志](docs/DEVELOPMENT.md)
