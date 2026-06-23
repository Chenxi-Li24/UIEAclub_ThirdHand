# ThirdHand Web Control Panel

基于 Three.js 的 Fairino FR5 机械臂 3D 可视化控制面板，通过 ESP32 转发运动指令。

## 架构

浏览器 (Three.js) --WebSocket--> Node.js 代理 --UDP--> ESP32 --UDP--> FR5

## 快速开始

cd ThirdHand/web-control/server
npm install
npm start

浏览器打开 http://localhost:3000

## 功能

- 3D 可视化: 参数化 FR5 六轴机械臂模型 (DH 参数)
- 关节控制: 6 个关节滑块
- 预设位置: Home, Test1-Test8
- 实时模式: 拖动滑块即时发送指令
- 急停按钮: 立即停止机器人运动
- 通信日志: 实时显示收发指令

## 文件结构

server/          - Node.js 代理服务器
server/config.js - ESP32/机器人地址配置
server/proxy.js  - WebSocket <-> UDP 桥接
web/             - 前端静态文件
web/index.html   - 主页面
web/js/          - JavaScript 模块
web/css/         - 样式表

## 配置

编辑 server/config.js 修改 ESP32 IP、ServoJ 参数、预设位置