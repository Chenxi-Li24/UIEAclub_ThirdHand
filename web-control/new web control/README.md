# ThirdHand New Web Control

这是一个独立的网页控制与语音协议原型。所有运行所需的网页、脚本、协议文档和 TCR050 模型都包含在当前文件夹内，不依赖开发者电脑上的绝对路径。

当前版本用于验证：

- 麦克风授权、设备检测与输入选择；
- 浏览器通过 TCP WebSocket `3001` 端口发送 PCM 音频；
- 本地 Voice Mock 返回实时转写、最终转写和候选动作；
- 用户在页面确认候选动作；
- TCP 3001 与现有 UDP 3001 可以同时使用。

Voice Mock 与真实机器人通信完全隔离。确认候选动作只会在网页中模拟执行，不会向机械臂或夹爪发送命令。

## Windows 一键启动

确保电脑已安装 Node.js，然后双击：

```text
start-web-control.bat
```

第一次启动时脚本会自动安装依赖。服务启动后，默认浏览器会打开：

```text
http://127.0.0.1:3000/
```

请保留服务窗口。需要停止时，在服务窗口按 `Ctrl+C`，或者关闭该窗口。

## 手动启动

从仓库根目录进入当前文件夹：

```powershell
cd "web-control\new web control"
npm install
npm start
```

网页地址：

```text
http://127.0.0.1:3000/
```

Voice Protocol v1 地址：

```text
ws://127.0.0.1:3001/v1/voice
```

## 测试

3001 端口协议往返测试：

```powershell
npm run test:protocol
```

先运行 `npm start`，再打开另一个终端执行浏览器测试：

```powershell
npm run test:browser
```

浏览器测试使用 Edge 的虚拟麦克风，不会访问真实硬件。生成的 `voice-control-preview.png` 已被 `.gitignore` 排除。

完整协议见 [docs/voice-protocol-v1.md](docs/voice-protocol-v1.md)。
