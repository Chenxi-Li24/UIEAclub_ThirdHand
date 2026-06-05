# ESP32 Fairino Robot Client — 开发日志

> 2026-06-05 | Chenxi Li

## 概述

ESP32-S3 通过 WiFi 与法奥协作机器人控制器通信，使用 UDP 帧协议 (port 20007) 发送
ServoJ 关节位置控制命令。ESP32 同时运行 UDP 命令服务器 (port 20008)，
PC 可通过 WiFi 发送文本命令控制机器人。

## 架构

```
PC (192.168.58.x) --WiFi--> ESP32-S3 (192.168.58.100:20008)
                                  |
                                  +--WiFi--> 法奥机器人 (192.168.58.2:20007)
```

- PC -> ESP32: 文本命令 over UDP, port 20008
- ESP32 -> 机器人: 法奥帧协议 over UDP, port 20007

## 通信协议

### 法奥 UDP 帧格式 (port 20007)

```
/f/bIII{count}III{cmdID}III{contentLen}III{content}III/b/f
```

### 已验证命令

| cmdID | 函数 | 用途 | 状态 |
|-------|------|------|------|
| 376 | ServoJ({j1..j6},{0,0,0,0},acc,vel,cmdT,filterT,gain,0) | 位置伺服 | 工作 |
| 689 | ServoMoveStart() | 伺服启动 | 不需要 |
| 690 | ServoMoveEnd() | 伺服停止 | 不需要 |

## 关键发现 & 踩坑记录

### 1. ServoJ 不需要 ServoMoveStart/End (最重要)
- **现象**: 带 Start/End 时机器人不动作；去掉后裸发 ServoJ 机器人动了
- **原因**: Start/End 可能是力矩伺服 (ServoJT) 独有的入口，位置伺服 (ServoJ) 是独立命令
- **结论**: ServoJ (cmdID=376) 直接发送即可，不需要配对的 Start/End

### 2. acc=0, vel=0 导致机器人不运动
- **现象**: 命令被响应（返回 "1"）但机械臂物理不动
- **原因**: 零加速度/零速度 = "别动"
- **解决**: 设置 acc=0.1, vel=0.1

### 3. 轴速度超限 (vel=0.5)
- **现象**: 示教器报 "轴1速度超限"
- **解决**: 降低 vel 到 0.1

### 4. 控制不够丝滑
- **现象**: 链路通顺但运动有顿挫感
- **原因**: 步进式控制 (2deg/step, cmdT=0.05s) 不够连续
- **TODO**: 探索更小步进、更短 cmdT，或尝试 ServoJT 力矩模式

### 5. 串口调试困难 (ESP32-S3 USB-Serial/JTAG)
- **问题**: Windows 打开 COM8 时 DTR 触发 ESP32 硬件复位，丢失启动日志
- **尝试**: ARDUINO_USB_CDC_ON_BOOT=1 期望软件 CDC 生成新 COM 口 —— Windows 上未生效
- **最终方案**: 弃用串口，通过 WiFi UDP (port 20008) 做命令通道

### 6. stats.h 静态变量 ODR 问题
- `_wifiSsid` / `_wifiPass` 在头文件中声明为 static，导致每个 .cpp 各有一份独立副本
- wifiCredLoad() 写入 wifi_manager.cpp 的副本，wifiCredSsid() 从 main.cpp 读取副本 -> 读到空字符串
- **规避**: 所有凭据操作统一在 wifi_manager.cpp 内完成

## 文件结构

```
esp32-fairino-client/
  platformio.ini          # ESP32-S3, Arduino, 8MB Flash
  .gitignore
  src/
    main.cpp              # WiFi + UDP cmd server + servo j1wave 命令
    wifi_manager.h/cpp     # WiFi 状态机 (复用 deskpet)
    stats.h                # NVS 凭据存储 (复用 deskpet)
    fairino_udp.h/cpp      # 法奥 UDP 帧客户端
  tools/
    fairino_cmd.py         # PC 端命令工具
  DEVELOPMENT.md           # 本文件
```

## 命令速查

```bash
python tools/fairino_cmd.py status                  # 查看状态
python tools/fairino_cmd.py 'servo j1wave 1 5'      # J1 往复: 1周期 5deg/s
python tools/fairino_cmd.py 'servo j1wave 3 10'     # J1 往复: 3周期 10deg/s
python tools/fairino_cmd.py 'servo end'              # 紧急停止
python tools/fairino_cmd.py help                     # 帮助
```

## 网络参数

- ESP32 WiFi: Xiaomi_7D5E / 静态IP 192.168.58.100 / GW 192.168.58.1
- 机器人: 192.168.58.2
- **注意**: ESP32 需要和机器人在同一个 L2 网段 (192.168.58.x)

## ServoJ 当前参数

- acc=0.1, vel=0.1, cmdT=0.05
- 步进角度: 2deg, 间隔由 deg/s 参数计算 (interval_ms = stepDeg / degPerSec * 1000)
- 参考关节(用户提供): J1=-59.14 J2=-57.95 J3=-102.47 J4=-106.39 J5=117.31 J6=-12.65
- J1 往复范围: -60deg <-> 0deg

## TODO

- [ ] 提升运动平滑度 (更小步进、更短 cmdT、或 ServoJT 力矩模式)
- [ ] Windows USB CDC 驱动修复
- [ ] 机器人状态查询 (关节角度、报警等)
- [ ] 非阻塞 wave 执行 (支持中途 servo end)
- [ ] 修复 stats.h ODR 问题 (改用 .cpp + extern)
