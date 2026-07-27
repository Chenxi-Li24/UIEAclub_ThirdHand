# ThirdHand Voice Protocol v1

## 1. 目的

本协议是控制网页与未来 Jetson 本地 AI 服务之间的通信合同。Jetson 负责语音转文字和候选动作识别；网页负责展示、白名单校验和用户确认。

Jetson 不得直接连接机器人控制端口，也不得返回可原样透传的机械臂命令。

## 2. 连接

| 项目 | v1 规定 |
| --- | --- |
| 传输 | TCP WebSocket |
| 开发地址 | `ws://127.0.0.1:3001/v1/voice` |
| Jetson 地址 | `ws://<jetson-ip>:3001/v1/voice` |
| WebSocket 子协议 | `thirdhand.voice.v1` |
| 并发 | 每条连接最多一个活动录音会话 |

现有机器人代理使用 UDP 3001。TCP 3001 与 UDP 3001 可以同时存在，但配置和文档必须明确写出传输类型。

页面升级为 HTTPS 后，语音服务必须升级为 `wss://`。

## 3. JSON envelope

所有非音频消息均为 UTF-8 JSON：

```json
{
  "v": 1,
  "type": "session.start",
  "messageId": "uuid",
  "replyTo": null,
  "sessionId": "uuid",
  "ts": 1785168000000,
  "payload": {}
}
```

`v`、`type`、`messageId`、`sessionId`、`ts` 和 `payload` 必填。回复请求时使用 `replyTo`。

## 4. 音频

- 编码：PCM signed 16-bit little-endian
- 采样率：16000 Hz
- 声道：单声道
- 帧长：100 ms
- PCM 载荷：每帧3200字节

二进制 WebSocket 帧结构：

| 字节 | 内容 |
| --- | --- |
| 0–3 | ASCII `THV1` |
| 4–7 | `sequence`，uint32 little-endian |
| 8–11 | 会话内 `elapsedMs`，uint32 little-endian |
| 12–3211 | 3200字节 PCM S16LE |

WebSocket 保证顺序；`sequence` 用于诊断客户端采集或发送逻辑中的丢帧。

## 5. 正常事件顺序

```text
session.start
→ session.ready
→ binary audio frames
→ transcript.partial (0..n)
→ session.stop
→ session.processing
→ transcript.final
→ intent.candidate
→ session.completed
```

`transcript.partial`：

```json
{
  "segmentId": "segment-1",
  "revision": 2,
  "text": "打开夹"
}
```

同一 `segmentId` 只保留最高 `revision`，不得把每次 partial 直接追加。

`intent.candidate`：

```json
{
  "intent": "gripper.open",
  "sourceText": "打开夹爪",
  "confidence": 0.95,
  "requiresConfirmation": true,
  "args": {}
}
```

v1 候选动作名称：

- `robot.estop`
- `robot.status`
- `robot.preset`
- `gripper.open`
- `gripper.close`
- `gripper.grip`
- `gripper.set_position`

禁止 `servo_raw`、任意关节角度、复位、网络配置和外骨骼控制。

## 6. 状态、心跳与限制

客户端状态：

```text
offline → connecting → idle → starting → recording
                                      ↓
completed ← awaiting_confirmation ← processing
```

- 单次录音最长 30 秒；
- 客户端每 15 秒发送一次 `ping`；
- 连续两次未收到 `pong`，关闭连接并进入 `offline`；
- WebSocket `bufferedAmount` 超过 1 MiB 时停止录音，不得静默丢音频；
- 断线后创建新会话，不补发旧音频。

## 7. 错误

错误消息的 `payload`：

```json
{
  "code": "UNSUPPORTED_AUDIO",
  "message": "Use PCM S16LE, 16 kHz, mono, 100 ms frames.",
  "recoverable": false
}
```

标准错误码：

- `UNSUPPORTED_VERSION`
- `UNSUPPORTED_AUDIO`
- `INVALID_STATE`
- `BAD_MESSAGE`
- `BAD_AUDIO_FRAME`
- `AUDIO_SEQUENCE_GAP`
- `AI_UNAVAILABLE`
- `TIMEOUT`
- `OVERLOAD`
- `ORIGIN_FORBIDDEN`
- `INTERNAL`

## 8. 安全边界

- Voice AI 只能返回文字和候选动作。
- 网页必须对白名单和参数范围进行验证。
- 除 `robot.estop` 外，候选动作必须由用户确认。
- v1 原型确认后只显示“模拟执行”，不得调用现有 `/ws`。
- 语音急停不能代替实体急停或页面急停。
