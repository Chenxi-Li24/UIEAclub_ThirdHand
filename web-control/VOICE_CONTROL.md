# ThirdHand Voice Control v1

这是 `UIEAclub_ThirdHand` 网页控制台的语音控制入口，用于验证：

- 麦克风启用、检测与选择；
- 浏览器到本地 AI 的 Voice Protocol v1；
- TCP/WebSocket 3001 音频流；
- 语音转文字与候选动作确认界面。

当前的 Voice Mock 不会启动机器人代理，也不会向机械臂或夹爪发送数据。识别出的动作只会显示为候选指令，用户确认后也仅在界面中模拟执行。

## 启动

首次使用：

```powershell
cd web-control\server
npm install
```

启动网页和 Voice Mock：

```powershell
npm run voice:dev
```

然后在 Edge 或 Chrome 打开：

```text
http://127.0.0.1:3000/
```

页面默认连接：

```text
ws://127.0.0.1:3001/v1/voice
```

按 `Ctrl+C` 同时停止网页和 Mock。

## 协议假跑

```powershell
npm run test:voice-protocol
```

该测试会临时同时绑定 UDP 3001 和 TCP 3001，发送一帧假 PCM 音频，验证 partial、final 和候选动作事件，随后释放端口。它不会连接任何硬件。

启动 `npm run voice:dev` 后，还可以运行真实浏览器烟雾测试：

```powershell
npm run test:voice-browser
```

该测试使用 Edge 的假麦克风走完设备检测、录音、转写和候选动作界面，并生成 `voice-control-preview.png`。

完整协议见 [VOICE_PROTOCOL_V1.md](../docs/VOICE_PROTOCOL_V1.md)。
