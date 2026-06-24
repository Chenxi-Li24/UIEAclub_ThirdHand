# ThirdHand Web Control Panel

基于 Three.js 的法奥 FR5 机械臂 3D 可视化控制面板，通过 Node.js 代理 + ESP32-P4 转发运动指令。

## 架构

```
浏览器 (Three.js)
    │ WebSocket
    ▼
Node.js 代理 (proxy.js)
    │ UDP 20008
    ▼
ESP32-P4 控制终端
    │ UDP 20007 + TCP 20005
    ▼
法奥 FR5 机械臂
```

## 快速开始

```bash
cd web-control/server
npm install
npm start
```

浏览器打开 **http://localhost:3000**

## 功能

### 3D 可视化
- URDF 加载 TCR050 机械臂模型（毫米制）
- 6 轴关节实时渲染，地面/壁挂两种安装方式
- 网格、坐标轴、相机重置

### 控制功能
- **关节控制**：6 个滑块 + 数值输入，支持实时发送模式
- **预设位置**：Home + Test1~Test8（3×3 网格）
- **急停/复位**：E-STOP 连续发送 `estop` + `servo end`
- **回零**：3D 模型归零

### 状态监控
- 实时关节角显示（J1~J6）
- TCP 末端位置（X/Y/Z/Rx/Ry/Rz）
- 机器人状态徽章（IDLE/MOVING/E-STOP/ERROR/LOCKED）
- 心跳指示（hb:OK / hb:LOST）
- 通信日志（收发指令实时显示）

### 界面
- 全屏 3D 视口 + 右侧浮动控制抽屉（可收起）
- 9 种主题配色（科技深空/工业橙黑/赛博紫黑等）
- 响应式布局（桌面/平板/手机）

## 文件结构

```
web-control/
├── server/                  # Node.js 代理服务器
│   ├── proxy.js             # WebSocket ↔ UDP 桥接 + 心跳
│   ├── config.js            # ESP32 IP / ServoJ 参数 / 预设位置
│   └── package.json
└── web/                     # 前端静态文件
    ├── index.html           # 主页面（TopBar + 视口 + 抽屉 + StatusBar）
    ├── css/style.css        # 样式（9 主题 + 毛玻璃 + 动画）
    └── js/
        ├── main.js          # 主入口（SceneManager/ArmModel/WSClient/UIControls）
        └── lib/             # Three.js + URDFLoader
```

## 配置

编辑 [server/config.js](server/config.js)：

```js
module.exports = {
  esp32: { host: '192.168.58.100', port: 20008 },
  servo: { acc: 0.006, vel: 0.006, cmdT: 1.0 },
  presets: {
    home:  [0, -90, 90, -90, -90, 0],
    test1: [30, -70, -90, -80, 90, 0],
    // ...
  }
};
```

## WebSocket 消息协议

### 浏览器 → 服务端

| cmd | 字段 | 说明 |
|---|---|---|
| `connect` | host, port | 连接 ESP32 |
| `servo` | joints[6] | 关节控制 |
| `preset` | name | 预设位置 |
| `estop` | - | 急停 |
| `reset` | - | 复位 |
| `status` | - | 查询状态 |

### 服务端 → 浏览器

| type | 字段 | 说明 |
|---|---|---|
| `config` | presets, servo, connection | 初始配置 |
| `connection` | mode, host, port, connected | 连接状态 |
| `cnde_state` | joints[6], robotState, tcpPos | 实时状态 |
| `estop` | ts | 急停通知 |
| `error` | msg | 错误 |

## 3D 模型

模型文件位于仓库根目录的 `3d-models/`，通过 Node.js 代理的 `/models` 虚拟路径访问：
- `3d-models/models/TCR050/` — URDF + STL 网格
- `3d-models/models/glb/` — GLB 模型
- `3d-models/models/fr5.step` — STEP 文件
