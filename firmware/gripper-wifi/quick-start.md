# WiFi 夹爪控制快速上手 / WiFi Gripper Quick Start

## 硬件 / Hardware

```
ESP32-S3              Bus Servo Adapter A
──────                ──────
GPIO17 (U1TXD)  →    TX       (直连 / straight)
GPIO18 (U1RXD)  →    RX       (直连 / straight)
GND             →    GND

舵机 7.4V 电源  →    Bus Adapter DC-IN  (独立供电 / separate power)
ESP32 通过 USB-C 供电 / powered via USB-C
```

## 网络 / Network

| 参数 | 值 |
|------|-----|
| WiFi SSID | ZTE-P5cS5Y |
| 密码 / Password | 12345678 |
| 夹爪 IP / Gripper IP | 192.168.58.101 |
| UDP 端口 / Port | 20009 |

## 上传固件 / Upload Firmware

1. Arduino IDE 安装 **ESP32** 板支持 (Espressif)
2. Library Manager 安装 **SCServo**
3. 打开 / Open: `esp32/gripper_wifi/gripper_wifi.ino`
4. 工具 → 开发板 → `ESP32S3 Dev Module`
5. USB CDC On Boot → `Enabled`
6. 端口 / Port → `COM10`
7. 点击上传 / Click Upload

## 一键启动 / One-Click Launch

双击项目根目录下的 **`启动夹爪控制.bat`**，自动启动服务并打开浏览器。  
Double-click **`启动夹爪控制.bat`** in the project root to auto-start the server and open the browser.

Proxy 服务器窗口不要关，关了就断连。  
Keep the proxy server window open - closing it disconnects.

**手动启动 / Manual start:** 见下方 / see below.

## 启动服务 / Start Server

每次想控制夹爪时，需要先启动 Node.js 代理服务器。  
Run the Node.js proxy server before controlling the gripper.

```bash
# 1. 进入项目目录 / Go to project folder
cd d:/Documents/Thirdhand/GITHUB/UIEAclub_ThirdHand/web-control/server

# 2. 如果旧服务还占着端口，先杀掉 / Kill old server if running
taskkill /F /IM node.exe

# 3. 启动代理 / Start proxy
node proxy.js
```

启动成功后会显示 / Successful startup output:
```
═══════════════════════════════════════════════
  ThirdHand Web Control Panel
  Local:   http://localhost:3000
  Network: http://192.168.58.xx:3000       ← 局域网内其他设备用这个
  ESP32:   192.168.58.100:20008            ← 机械臂
  Gripper: 192.168.58.101:20009            ← 夹爪
═══════════════════════════════════════════════
[WiFi] connected to 192.168.58.100:20008     ← 机械臂在线
[Gripper] connected to 192.168.58.101:20009  ← 夹爪在线
```

看到 `[Gripper] connected` 就说明夹爪连上了。  
Seeing `[Gripper] connected` means the gripper is online.

**提示 / Tip:** 保持终端窗口开着，Ctrl+C 可关闭服务。  
Keep the terminal window open. Ctrl+C to stop the server.

## 网页控制 / Web Control

浏览器打开 / Open browser:

```
http://localhost:3000
```

右侧抽屉底部找到 **夹爪控制** / Find **夹爪控制** at bottom of right drawer。

## 控制按钮 / Controls

| 按钮 / Button | 功能 / Function | UDP 命令 |
|---------------|-----------------|----------|
| **OPEN 张开** | 夹爪全开 (84mm) | `open` |
| **CLOSE 闭合** | 夹爪全闭 (0mm) | `close` |
| **GRIP 夹取检测** | 闭合 + 遇阻自动停止 / Close + stop on contact | `grip` |
| 滑块 + **GO** | 移动到指定位置 / Move to position | `pos <0-3800>` |
| **25% / 50% / 75%** | 预设位置 / Preset positions | `pos 950/1900/2850` |

## 状态显示 / Status

| 显示 / Display | 说明 / Description |
|----------------|-------------------|
| 状态 / State | 张开 / 闭合 / 运动中 / 夹到! |
| 负载 / Load | 实时力矩反馈 (阈值1320触发夹取) |
| 位置 / Pos | 舵机原始值 (0-3800) |
| 开度 / Opening | 夹爪间距 mm (0-84mm) |

## 夹取检测 / Grasp Detection

- 点 **GRIP** → 夹爪闭合，实时监控负载
- 负载 > 1320 → 夹到物体，停在当前位置
- 到达 3800 → 完全闭合（未夹到物）
- 夹取中可点 OPEN/CLOSE 打断 / Click OPEN/CLOSE to interrupt

## 力矩限制 / Torque Limit

- `TORQUE_LIMIT = 500` (50% 最大力矩) — 在 `servo_config.h` 中调整 / adjustable
- 范围 0-1000, 越小越无力 / Lower = less force
- 启动时自动写入舵机寄存器 34 / Auto-written to servo register 34 on boot

## 夹取锁定 / Grasp Lock

- 夹取成功后 → **禁止 CLOSE**，防止夹坏已夹住的物体
- Successful grasp → CLOSE is blocked to prevent crushing
- 必须先点 OPEN 解锁 / Must press OPEN first to unlock
- 返回错误: `ERR: grasped, open first`

## 过载保护 / Overload Protection

- 静止状态负载 > 1350 → 自动张开 / Auto-open
- 防止夹坏物体或烧电机 / Prevents damage
- 代码位置: `servo_config.h` → `OVERLOAD_THRESHOLD`

## 串口监视 / Serial Monitor

波特率 / Baud: `115200`

正常输出 / Normal output:
```
=== Thirdhand Gripper Controller ===
[SERVO] ID:1 test read: 4
[WiFi] connected! IP: 192.168.58.101
[UDP] listening on port 20009
```

## 故障排查 / Troubleshooting

| 问题 / Problem | 检查 / Check |
|---------------|-------------|
| 舵机不动 / Servo not moving | 7.4V 供电 / Power ON |
| test read: -1 | TX/RX 直连接法 / Straight TX-TX, RX-RX |
| 网页无夹爪区 / No gripper UI | Ctrl+F5 强制刷新 / Hard refresh |
| 负载始终 0 / Load always 0 | 舵机通电 / Servo powered |
| heartbeat lost | WiFi 信号 / Check WiFi |
| 劲太大 / Too strong | 调低 `TORQUE_LIMIT` (0-1000) |
| 夹到后还继续夹 / Keeps closing | 调低 `GRIP_LOAD_THRESHOLD` (1320) |
