# Contributing to ThirdHand

## 仓库结构 — 代码放哪里

```
UIEAclub_ThirdHand/
├── firmware/                       # 固件
│   ├── p4-controller/              # ESP32-P4 控制终端（命令中继）
│   ├── s3-exo/                     # ESP32-S3 外骨骼（触摸屏 + 传感器）
│   └── modules/                    # 传感器与控制模块（IMU/激光/舵机）
├── web-control/                    # Web 控制面板
│   ├── server/                     # Node.js 代理
│   └── web/                        # 前端（Three.js）
├── 3d-models/                      # 3D 建模文件（URDF/STL/GLB/STEP）
├── references/                     # 参考资料（厂商资料 + SDK）
├── docs/                           # 文档
└── .github/                        # Issue / PR 模板
```

**代码归属：**

| 内容 | 目录 |
|---|---|
| ESP32-P4 控制终端固件 | `firmware/p4-controller/` |
| ESP32-S3 外骨骼固件 | `firmware/s3-exo/` |
| 传感器/舵机模块（开发的单一模块请放这里） | `firmware/modules/` |
| Web 控制面板 | `web-control/` |
| 3D 模型文件（文件夹直接放3d-models下） | `3d-models/` |
| 厂商资料 / SDK | `references/` |
| 文档 | `docs/` |

## 分支命名

```
feat/p4-xxx             — P4 控制终端新功能
feat/s3-xxx             — S3 外骨骼新功能
feat/web-xxx            — Web 控制面板新功能
feat/module-xxx         — 传感器/舵机模块

fix/p4-xxx              — 修复 P4 固件 bug
fix/s3-xxx              — 修复 S3 固件 bug
fix/web-xxx             — 修复 Web bug

docs/xxx                — 文档更新
refactor/xxx            — 代码重构
```

## Commit 格式

```
模块: 动词 描述

示例:
  p4-controller: 添加 ServoMoveStart/End 命令
  s3-exo: 添加 I2S 麦克风驱动
  web-control: 重构右侧抽屉交互
  modules: 新增激光测距模块
  docs: 更新 README 架构图
```

## 编译验证

### P4 控制终端（Arduino IDE）

使用 Arduino IDE 打开 `firmware/p4-controller/p4-controller.ino`，选择 ESP32-P4 开发板，编译烧录。

### S3 外骨骼（PlatformIO）

```bash
cd firmware/s3-exo
pio run --environment esp32-s3-robot        # 编译
pio run --target upload                      # 烧录
pio device monitor --baud 115200             # 串口监视
```

### Web 控制面板（Node.js）

```bash
cd web-control/server
npm install
npm start                                    # 启动 http://localhost:3000
```

## Pull Request 流程

1. 从 `main` 创建分支
2. 修改代码
3. 编译/运行验证通过
4. 提交 PR，选择对应模板
5. 等待 Review

## 代码规范

- 固件代码遵循 Arduino 风格，模块化分文件
- Web 前端使用 ES6+ 语法，类封装
- 注释使用中文，关键函数需有功能说明
- 配置项集中放在 `config.h` / `config.js`，不硬编码
