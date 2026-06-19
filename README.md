# ThirdHand — 桌面级仿人机械臂

UIEA 俱乐部项目。基于法奥机械臂 + ESP32 外骨骼 + 语音 AI 控制的仿人机械臂系统。

## 硬件概览

| 组件 | 型号 | 说明 |
|------|------|------|
| 机械臂 | 法奥 FR5 / FR10 | 6 轴协作机器人 |
| 外骨骼控制器 | **ESP32-S3** + 1.69" ST7789V | 4×IMU + 8×磁编码 + 触摸屏 |
| 终端（规划） | ESP32-P4 + 10.1" 屏幕 | 摄像头/语音/电池 |

## 快速开始

```bash
# 烧录外骨骼固件
cd firmware/s3-exo
pio run --target upload

# 可视化（规划中）
cd visualizer && python server/bridge.py

# AI Agent（规划中）
cd voice-ai && python agent.py
```

## 仓库结构

```
firmware/           ESP32 固件（S3 外骨骼 + P4 终端）
visualizer/         Web 3D 可视化（Three.js）
voice-ai/           PC 端 LLM Agent（Claude / DeepSeek）
hardware/           硬件设计文件（PCB / CAD / BOM）
sdk/                第三方 SDK
docs/               文档
.github/            Issue / PR 模板
```

## 通信

| 端口 | 协议 | 用途 |
|------|------|------|
| 80 | HTTP | 外骨骼 Web API |
| 20005 | TCP | 法奥 CNDE 状态 |
| 20007 | UDP | 法奥伺服控制 |
| 20008 | UDP | PC 命令服务器 |

## 贡献

参见 [CONTRIBUTING.md](CONTRIBUTING.md)
