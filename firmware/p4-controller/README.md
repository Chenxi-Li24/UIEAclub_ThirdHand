# ESP32-P4 控制终端固件

ESP32-P4 (JC8012P4A1) 上的 Arduino 固件，作为 Web/外骨骼与法奥 FR5 机器人之间的 **UDP 命令中继**，带 LVGL 触摸屏 UI。

## 功能

### 机器人控制
- **UDP 命令服务器**（端口 20008）：接收 Web/PC/外骨骼的运动指令
- **Fairino UDP 客户端**（端口 20007）：转发 ServoJ 伺服控制命令到机器人
- **CNDE TCP 客户端**（端口 20005）：实时回读机器人关节状态
- **状态广播**：周期性（500ms）广播 `JOINTS:` UDP 包给 Web
- **自检流程**：BOOT 按键触发，9 个预设位置依次执行

### LVGL 屏幕 UI
- **10.1" IPS 800×1280 MIPI DSI 屏**（JD9365DA 驱动）
- **GSL3680 I2C 电容触摸**
- **5 个标签页**：
  - Dashboard：关节角仪表盘
  - Control：Jog 按钮控制
  - System：系统信息
  - WiFi：WiFi 扫描与连接
  - About：关于信息

### 状态指示
- **WS2812 LED**：WiFi/运行/错误状态可视化
- **BOOT 按键**：触发自检流程

## 硬件

| 组件 | 说明 |
|---|---|
| 主控 | ESP32-P4 (JC8012P4A1) |
| 屏幕 | 10.1" IPS 800×1280 MIPI DSI (JD9365DA) |
| 触摸 | GSL3680 I2C 电容触摸 |
| LED | WS2812 RGB (GPIO 26) |
| 按键 | BOOT (GPIO 35) |
| 网络 | WiFi STA，静态 IP |

## 文件结构

```
p4-controller/
├── p4-controller.ino        # 主程序：UDP 命令服务器 + 自检 + LVGL 调度
├── config.h                 # WiFi / 机器人 IP / 自检参数 / 引脚
├── pins_config.h            # JC8012P4A1 引脚定义（LCD/触摸/LED）
├── lv_conf.h                # LVGL 8.4 配置（800×1280, RGB565）
├── cmd_queue.h              # 命令队列 + 心跳监控 + 状态机
├── fairino_udp.{cpp,h}      # Fairino UDP 帧协议客户端（端口 20007）
├── cnde_client.{cpp,h}      # CNDE TCP 客户端（端口 20005），回读关节状态
├── wifi_manager.{cpp,h}     # WiFi 状态机，静态 IP 连接
├── hw/
│   ├── display.h            # 显示初始化接口
│   └── input.h              # 触摸初始化接口
├── src/
│   ├── hw/
│   │   ├── display.cpp      # LVGL 显示驱动注册 + DPI Panel 初始化
│   │   └── input.cpp        # LVGL 触摸输入驱动注册
│   ├── lcd/
│   │   ├── esp_lcd_jd9365.{c,h}  # JD9365DA MIPI DSI 面板驱动
│   │   └── jd9365_lcd.{cpp,h}    # LCD 初始化封装
│   ├── touch/
│   │   ├── esp_lcd_gsl3680.{c,h} # GSL3680 触摸驱动
│   │   ├── esp_lcd_touch.{c,h}   # esp_lcd_touch 接口实现
│   │   ├── gsl3680_touch.{cpp,h} # 触摸初始化封装
│   │   └── gsl_point_id.{c,h}    # GSL3680 固件数据
│   ├── ui/
│   │   ├── ui_core.cpp      # LVGL 核心 + 5 标签页管理器
│   │   ├── ui_dashboard.cpp # 仪表盘页（关节角弧形仪表）
│   │   ├── ui_control.cpp   # 控制页（Jog 按钮 + 动作按钮）
│   │   ├── ui_system.cpp    # 系统页（CPU/内存/温度条）
│   │   ├── ui_wifi.cpp      # WiFi 页（扫描列表 + 密码输入）
│   │   └── ui_about.cpp     # 关于页
│   └── psram_shim.cpp       # PSRAM 堆分配适配（帧缓冲用）
└── ui/                      # UI 头文件
```

## 配置

编辑 [config.h](config.h)：

```c
// WiFi
#define WIFI_SSID       "ZTE-P5cS5Y"
#define WIFI_PASS       "12345678"
#define STATIC_IP       "192.168.58.100"
#define ROBOT_IP        "192.168.58.2"
#define ROBOT_UDP_PORT  20007

// 自检运动参数
#define SELF_TEST_CMDT      2.0f    // ServoJ 指令周期 (秒)
#define SELF_TEST_SETTLE_MS 5000    // 每个位置停留时间
#define SELF_TEST_TIMEOUT   300000  // 自检总超时 (5min)

// 屏幕与触摸
#define ENABLE_DISPLAY  1   // JD9365DA MIPI DSI 800×1280
#define ENABLE_TOUCH    1   // GSL3680 I2C 电容触摸

// 引脚
#define PIN_WS2812      26
#define BOOT_BUTTON     35
```

## 编译烧录

### 环境要求

- **Arduino IDE 2.x** 或 **arduino-cli 1.5+**
- **ESP32 板支持包 v3.1.0+**（需支持 ESP32-P4）
- **LVGL 8.4.x** 库（库管理器安装）
- **Adafruit NeoPixel** 库

### arduino-cli 方式（推荐）

```bash
# 安装依赖库
arduino-cli lib install lvgl@8.4.0
arduino-cli lib install "Adafruit NeoPixel"

# 编译（关键：必须启用 PSRAM，否则帧缓冲分配失败）
arduino-cli compile --fqbn "esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=huge_app" firmware/p4-controller

# 烧录（替换 COMx 为实际端口）
arduino-cli upload -p COMx --fqbn "esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=huge_app" firmware/p4-controller

# 串口监视
arduino-cli monitor -p COMx --config baudrate=115200
```

### Arduino IDE 方式

1. 打开 `firmware/p4-controller/p4-controller.ino`
2. 开发板选择：**ESP32P4 Dev Module**
3. 开发板选项设置：
   - **PSRAM**: Enabled
   - **Flash Size**: 16MB (128Mb)
   - **Partition Scheme**: Huge APP (3MB No OTA/1MB SPIFFS)
4. 编译并烧录

### 编译参数说明

| 参数 | 值 | 说明 |
|---|---|---|
| `PSRAM` | `enabled` | **必须启用**，800×1280 帧缓冲需要 ~2MB PSRAM |
| `FlashSize` | `16M` | JC8012P4A1 板载 16MB Flash |
| `PartitionScheme` | `huge_app` | 3MB APP 空间（代码约 1.46MB，占 46%） |

> **注意**：不启用 PSRAM 会导致 `esp_lcd_new_panel_dpi: no memory for frame buffer` 错误并崩溃重启。

## 通信协议

### Web/外骨骼 → ESP32（UDP 20008）

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

### ESP32 → Web（UDP 广播）

```
JOINTS:j1,j2,j3,j4,j5,j6,state
```
周期 500ms，state ∈ {0:IDLE, 1:MOVING, 2:E-STOP, 3:ERROR, 4:LOCKED}

## LED 状态指示

| 颜色 | 状态 |
|---|---|
| 红色 | 启动中 |
| 蓝色呼吸 | WiFi 连接中 |
| 黄色 | WiFi 正在连接 |
| 绿色呼吸 | WiFi 已连接，空闲 |
| 黄色（亮） | 自检运行中 |
| 红色呼吸 | 错误/超时 |
| 熄灭 | 自检完成 |

## 串口输出示例

```
=== ESP32 Fairino Client ===
[MAIN] LVGL display OK
[MAIN] LVGL UI initialized
[WiFi] STA connected to ZTE-P5cS5Y
[WiFi] OK  IP: 192.168.58.100  RSSI: -31
[CNDE] connected OK
[MAIN] UDP cmd server listening on port 20008
[MAIN] WiFi ONLINE — IP: 192.168.58.100
[CNDE] J1:60.5 J2:-69.6 J3:-91.0 J4:-84.3 J5:100.5 J6:-8.9 rs:1 ps:1
J1:60.5 J2:-69.6 J3:-91.0 J4:-84.3 J5:100.5 J6:-8.9 st:IDLE hb:402
```

## 更新固件

### 修改代码后重新烧录

```bash
# 1. 编译
arduino-cli compile --fqbn "esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=huge_app" firmware/p4-controller

# 2. 烧录（板子需通过 USB 连接，按 RESET 后端口出现即可）
arduino-cli upload -p COM18 --fqbn "esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=huge_app" firmware/p4-controller

# 3. 验证（打开串口监视器查看启动日志）
arduino-cli monitor -p COM18 --config baudrate=115200
```

### 常见问题

| 问题 | 原因 | 解决 |
|---|---|---|
| `no memory for frame buffer` | 未启用 PSRAM | 编译参数加 `PSRAM=enabled` |
| `Sketch too big` | 分区表空间不足 | 用 `PartitionScheme=huge_app`（3MB APP） |
| `Could not open COMx` | 端口被占用或板子崩溃循环 | 关闭占用程序，按 RESET 重新枚举 |
| 屏幕不亮 | LVGL 初始化失败 | 检查 `ENABLE_DISPLAY=1` 和 PSRAM |
| 触摸无响应 | GSL3680 初始化失败 | 检查 `ENABLE_TOUCH=1` 和 I2C 接线 |
