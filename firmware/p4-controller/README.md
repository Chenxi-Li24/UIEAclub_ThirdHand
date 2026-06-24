# ESP32-P4 控制终端固件

ESP32-P4 (JC8012P4A1) 上的 Arduino 固件，作为 PC/Web 与法奥 FR5 机器人之间的 **UDP 命令中继**。

## 功能

- **UDP 命令服务器**（端口 20008）：接收 PC/Web/外骨骼的运动指令
- **Fairino UDP 客户端**（端口 20007）：转发 ServoJ 伺服控制命令到机器人
- **CNDE TCP 客户端**（端口 20005）：实时回读机器人关节状态
- **状态广播**：周期性（500ms）广播 `JOINTS:` UDP 包给 PC/Web
- **自检流程**：BOOT 按键触发，9 个预设位置依次执行
- **WS2812 LED 状态指示**：WiFi/运行/错误状态可视化

## 硬件

| 组件 | 说明 |
|---|---|
| 主控 | ESP32-P4 (JC8012P4A1) |
| LED | WS2812 RGB (GPIO 26) |
| 按键 | BOOT (GPIO 35) |
| 网络 | WiFi STA，静态 IP |

## 文件结构

```
p4-controller/
├── p4-controller.ino     # 主程序：UDP 命令服务器 + 自检状态机 + LED/按键
├── config.h              # WiFi 凭据 / 静态 IP / 机器人 IP / 自检参数 / 引脚
├── fairino_udp.{cpp,h}   # Fairino UDP 帧协议客户端（端口 20007）
├── cnde_client.{cpp,h}   # CNDE TCP 客户端（端口 20005），回读关节状态
└── wifi_manager.{cpp,h}  # WiFi 状态机，静态 IP 连接，3 次重试
```

## 配置

编辑 [config.h](config.h)：

```c
#define WIFI_SSID       "ZTE-P5cS5Y"
#define WIFI_PASS       "12345678"
#define STATIC_IP       "192.168.58.100"
#define ROBOT_IP        "192.168.58.2"
#define ROBOT_UDP_PORT  20007

#define SELF_TEST_CMDT      2.0f    // ServoJ 指令周期 (秒)
#define SELF_TEST_SETTLE_MS 5000    // 每个位置停留时间
#define SELF_TEST_TIMEOUT   300000  // 自检总超时 (5min)

#define PIN_WS2812      26
#define BOOT_BUTTON     35
```

## 通信协议

### PC/外骨骼 → ESP32（UDP 20008）

| 命令 | 说明 |
|---|---|
| `status` | 查询当前关节状态 |
| `servo start` | 进入伺服模式 |
| `servo end` | 退出伺服模式 |
| `servo j1 <j1> <j2> <j3> <j4> <j5> <j6>` | 关节控制（6 个角度） |
| `selftest` | 启动自检流程 |
| `estop` | 急停 |
| `help` | 命令帮助 |

### ESP32 → 机器人（UDP 20007）

Fairino 帧格式：`/f/bIII{count}III{cmdID}III{len}III{content}III/b/f`

| 命令 ID | 说明 |
|---|---|
| `CMD_SERVO_J = 376` | ServoJ 关节控制 |
| `CMD_SERVO_MOVE_START = 689` | 进入伺服模式 |
| `CMD_SERVO_MOVE_END = 690` | 退出伺服模式 |

### 机器人 → ESP32（TCP 20005）

CNDE 协议帧：`0x5A5A | count | type | len | data | 0xA5A5`
- data 含 6×double 关节角 + 机器人状态

### ESP32 → PC/Web（UDP 广播）

```
JOINTS:j1,j2,j3,j4,j5,j6,state
```
周期 500ms，state ∈ {0:IDLE, 1:MOVING, 2:E-STOP, 3:ERROR, 4:LOCKED}

## 编译烧录

使用 Arduino IDE：
1. 打开 `p4-controller.ino`
2. 开发板选择 ESP32-P4
3. 编译并烧录

## LED 状态指示

| 颜色 | 状态 |
|---|---|
| 蓝色呼吸 | WiFi 连接中 |
| 绿色呼吸 | WiFi 已连接 |
| 黄色 | 自检运行中 |
| 红色呼吸 | 错误/超时 |
