# Contributing to ThirdHand

## 仓库结构 — 代码放哪里

```
ThirdHand/
├── firmware/
│   └── s3-exo/               ← ESP32-S3 外骨骼固件
│       ├── src/hw/           ← 硬件抽象层（显示屏/触摸/IMU/蜂鸣器/电池）
│       ├── src/robot/        ← 法奥机器人 UDP 控制 + CNDE 状态
│       ├── src/exo/          ← 外骨骼 IMU 融合 + 磁编码器
│       ├── src/ui/           ← LVGL 屏幕页面（5 页滑动）
│       ├── src/              ← main.cpp, wifi_manager, web_server, config
│       └── platformio.ini
├── visualizer/               ← Web 3D 可视化（Three.js）
├── voice-ai/                 ← PC 端 LLM Agent
├── sdk/                      ← 第三方 SDK
├── hardware/                 ← PCB / CAD / BOM
├── docs/                     ← 文档
└── .github/ISSUE_TEMPLATE/   ← Issue / PR 模板
```

**固件相关 →** `firmware/s3-exo/`
**可视化相关 →** `visualizer/`
**AI Agent →** `voice-ai/`
**硬件设计 →** `hardware/`
**文档 →** `docs/`

## 分支命名

```
feat/s3-exo-xxx        — 外骨骼固件新功能
feat/visualizer-xxx    — 可视化新功能
feat/voice-ai-xxx      — AI Agent 新功能
feat/hardware-xxx      — 硬件设计

fix/s3-exo-xxx         — 修复固件 bug
fix/visualizer-xxx     — 修复可视化 bug

docs/xxx               — 文档更新
```

## Commit 格式

```
模块: 动词 描述

示例:
  s3-exo: 添加 I2S 麦克风驱动
  visualizer: 更新机器人模型 URL
  voice-ai: 添加 DeepSeek 支持
  docs: 更新架构图
```

## 编译验证

固件:
```bash
cd firmware/s3-exo
pio run --environment esp32-s3-robot
```

烧录:
```bash
pio run --target upload --upload-port COM10
```

串口监视:
```bash
pio device monitor --baud 115200
```

## Pull Request 流程

1. 从 main 创建分支
2. 修改代码
3. 编译验证通过
4. 提交 PR，选择对应模板
5. 等待 Review
