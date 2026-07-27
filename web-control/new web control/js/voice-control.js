const VOICE_PROTOCOL = 'thirdhand.voice.v1';
const DEFAULT_ENDPOINT = 'ws://127.0.0.1:3001/v1/voice';
const TARGET_SAMPLE_RATE = 16000;
const FRAME_MS = 100;
const FRAME_SAMPLES = TARGET_SAMPLE_RATE * FRAME_MS / 1000;
const MAX_RECORDING_MS = 30000;
const MAX_BUFFERED_BYTES = 1024 * 1024;
const SESSION_READY_TIMEOUT_MS = 5000;

const INTENT_VALIDATORS = {
  'robot.estop': () => true,
  'robot.status': () => true,
  'robot.preset': args => typeof args?.name === 'string' && args.name.length > 0,
  'gripper.open': () => true,
  'gripper.close': () => true,
  'gripper.grip': () => true,
  'gripper.set_position': args =>
    Number.isInteger(args?.position) && args.position >= 0 && args.position <= 3800
};

function createId() {
  if (globalThis.crypto?.randomUUID) return globalThis.crypto.randomUUID();
  return `voice-${Date.now()}-${Math.random().toString(16).slice(2)}`;
}

function createEnvelope(type, sessionId, payload = {}, replyTo = null) {
  return {
    v: 1,
    type,
    messageId: createId(),
    replyTo,
    sessionId: sessionId || null,
    ts: Date.now(),
    payload
  };
}

function formatDuration(milliseconds) {
  const seconds = Math.max(0, Math.floor(milliseconds / 1000));
  return `00:${String(seconds).padStart(2, '0')}`;
}

function formatMicError(error) {
  const messages = {
    NotAllowedError: '麦克风权限被拒绝。请在浏览器地址栏重新允许麦克风。',
    NotFoundError: '没有检测到可用麦克风。',
    NotReadableError: '麦克风正被其他程序占用，或系统无法读取该设备。',
    OverconstrainedError: '所选麦克风已不可用，已尝试回退到默认设备。',
    SecurityError: '当前页面不允许使用麦克风。',
    AbortError: '麦克风启动被系统中断。'
  };
  return messages[error?.name] || error?.message || '麦克风启动失败。';
}

class StreamingPcmEncoder {
  constructor() {
    this.reset(48000);
  }

  reset(inputSampleRate) {
    this.inputSampleRate = inputSampleRate || 48000;
    this.ratio = this.inputSampleRate / TARGET_SAMPLE_RATE;
    this.sourceRemainder = new Float32Array(0);
    this.sourcePosition = 0;
    this.targetQueue = [];
    this.sequence = 0;
  }

  push(samples) {
    if (!(samples instanceof Float32Array) || samples.length === 0) return [];

    const combined = new Float32Array(this.sourceRemainder.length + samples.length);
    combined.set(this.sourceRemainder);
    combined.set(samples, this.sourceRemainder.length);

    const output = [];
    let position = this.sourcePosition;
    while (position < combined.length - 1) {
      const index = Math.floor(position);
      const fraction = position - index;
      const value = combined[index] + (combined[index + 1] - combined[index]) * fraction;
      output.push(value);
      position += this.ratio;
    }

    const consumed = Math.floor(position);
    this.sourceRemainder = combined.slice(consumed);
    this.sourcePosition = position - consumed;
    this.targetQueue.push(...output);

    const frames = [];
    while (this.targetQueue.length >= FRAME_SAMPLES) {
      frames.push(this._encode(this.targetQueue.splice(0, FRAME_SAMPLES)));
    }
    return frames;
  }

  flush() {
    if (this.targetQueue.length === 0) return [];
    const samples = this.targetQueue.splice(0, FRAME_SAMPLES);
    while (samples.length < FRAME_SAMPLES) samples.push(0);
    return [this._encode(samples)];
  }

  _encode(samples) {
    const buffer = new ArrayBuffer(12 + FRAME_SAMPLES * 2);
    const bytes = new Uint8Array(buffer);
    const view = new DataView(buffer);
    bytes.set([0x54, 0x48, 0x56, 0x31], 0); // THV1
    view.setUint32(4, this.sequence, true);
    view.setUint32(8, this.sequence * FRAME_MS, true);

    for (let index = 0; index < samples.length; index += 1) {
      const clamped = Math.max(-1, Math.min(1, samples[index]));
      const pcm = clamped < 0 ? clamped * 0x8000 : clamped * 0x7fff;
      view.setInt16(12 + index * 2, Math.round(pcm), true);
    }

    this.sequence += 1;
    return buffer;
  }
}

class MicrophoneManager {
  constructor(callbacks = {}) {
    this.callbacks = callbacks;
    this.stream = null;
    this.audioContext = null;
    this.sourceNode = null;
    this.analyserNode = null;
    this.workletNode = null;
    this.silentGain = null;
    this.meterFrame = 0;
    this.currentDeviceId = '';
    this.ready = false;
    this._deviceChangeHandler = () => this.refreshDevices();
    navigator.mediaDevices?.addEventListener?.('devicechange', this._deviceChangeHandler);
  }

  async enable(deviceId = '') {
    if (!navigator.mediaDevices?.getUserMedia) {
      throw new Error('浏览器未提供麦克风接口。请使用本机 Edge/Chrome，并通过 localhost 或 HTTPS 打开。');
    }

    await this.disable();
    this.callbacks.onState?.('requesting');

    const audioConstraints = {
      echoCancellation: true,
      noiseSuppression: true,
      autoGainControl: true
    };
    if (deviceId) audioConstraints.deviceId = { exact: deviceId };

    const stream = await navigator.mediaDevices.getUserMedia({
      audio: audioConstraints,
      video: false
    });

    this.stream = stream;
    const track = stream.getAudioTracks()[0];
    this.currentDeviceId = track?.getSettings?.().deviceId || deviceId || '';
    track?.addEventListener('ended', () => {
      if (this.stream === stream) {
        this.ready = false;
        this.callbacks.onDeviceLost?.();
      }
    });

    const AudioContextClass = window.AudioContext || window.webkitAudioContext;
    this.audioContext = new AudioContextClass();
    await this.audioContext.resume();
    await this.audioContext.audioWorklet.addModule(new URL('./pcm-worklet.js', import.meta.url));

    this.sourceNode = this.audioContext.createMediaStreamSource(stream);
    this.analyserNode = this.audioContext.createAnalyser();
    this.analyserNode.fftSize = 512;
    this.analyserNode.smoothingTimeConstant = 0.72;
    this.workletNode = new AudioWorkletNode(this.audioContext, 'thirdhand-pcm-capture');
    this.silentGain = this.audioContext.createGain();
    this.silentGain.gain.value = 0;

    this.sourceNode.connect(this.analyserNode);
    this.sourceNode.connect(this.workletNode);
    this.workletNode.connect(this.silentGain);
    this.silentGain.connect(this.audioContext.destination);
    this.workletNode.port.onmessage = event => {
      this.callbacks.onSamples?.(event.data.samples, this.audioContext?.sampleRate || 48000);
    };

    this.ready = true;
    this._startMeter();
    await this.refreshDevices();
    this.callbacks.onState?.('ready');
    return this.currentDeviceId;
  }

  async refreshDevices() {
    if (!navigator.mediaDevices?.enumerateDevices) return [];
    const devices = (await navigator.mediaDevices.enumerateDevices())
      .filter(device => device.kind === 'audioinput');
    this.callbacks.onDevices?.(devices, this.currentDeviceId);
    return devices;
  }

  async switchDevice(deviceId) {
    return this.enable(deviceId);
  }

  async disable() {
    this.ready = false;
    cancelAnimationFrame(this.meterFrame);
    this.meterFrame = 0;

    const stream = this.stream;
    this.stream = null;
    stream?.getTracks().forEach(track => track.stop());

    for (const node of [this.sourceNode, this.analyserNode, this.workletNode, this.silentGain]) {
      try { node?.disconnect(); } catch {}
    }

    this.sourceNode = null;
    this.analyserNode = null;
    this.workletNode = null;
    this.silentGain = null;

    const context = this.audioContext;
    this.audioContext = null;
    if (context && context.state !== 'closed') {
      try { await context.close(); } catch {}
    }

    this.callbacks.onLevel?.(0);
    this.callbacks.onState?.('off');
  }

  dispose() {
    navigator.mediaDevices?.removeEventListener?.('devicechange', this._deviceChangeHandler);
    return this.disable();
  }

  _startMeter() {
    const data = new Float32Array(this.analyserNode.fftSize);
    const tick = () => {
      if (!this.analyserNode || !this.ready) return;
      this.analyserNode.getFloatTimeDomainData(data);
      let energy = 0;
      for (const sample of data) energy += sample * sample;
      const rms = Math.sqrt(energy / data.length);
      this.callbacks.onLevel?.(Math.min(1, rms * 7));
      this.meterFrame = requestAnimationFrame(tick);
    };
    tick();
  }
}

class VoiceSocket {
  constructor(callbacks = {}) {
    this.callbacks = callbacks;
    this.socket = null;
    this.endpoint = '';
    this.state = 'offline';
    this.manualClose = false;
    this.reconnectTimer = 0;
    this.heartbeatTimer = 0;
    this.missedPongs = 0;
  }

  connect(endpoint) {
    this.endpoint = endpoint;
    this.manualClose = false;
    clearTimeout(this.reconnectTimer);
    this._closeSocket();
    this._setState('connecting');

    let socket;
    try {
      socket = new WebSocket(endpoint, VOICE_PROTOCOL);
    } catch (error) {
      this.callbacks.onError?.(error.message);
      this._setState('offline');
      this._scheduleReconnect();
      return;
    }

    this.socket = socket;
    socket.binaryType = 'arraybuffer';
    socket.addEventListener('open', () => {
      this.missedPongs = 0;
      this._setState('ready');
      this._startHeartbeat();
    });
    socket.addEventListener('message', event => {
      if (typeof event.data !== 'string') return;
      try {
        const message = JSON.parse(event.data);
        if (message.type === 'pong') this.missedPongs = 0;
        this.callbacks.onMessage?.(message);
      } catch {
        this.callbacks.onError?.('AI 返回了无法解析的消息。');
      }
    });
    socket.addEventListener('error', () => {
      this.callbacks.onError?.('无法连接本地 AI 3001。');
    });
    socket.addEventListener('close', () => {
      if (this.socket !== socket) return;
      this._stopHeartbeat();
      this.socket = null;
      this._setState('offline');
      if (!this.manualClose) this._scheduleReconnect();
    });
  }

  sendJson(type, sessionId, payload = {}) {
    if (!this.isReady()) return false;
    this.socket.send(JSON.stringify(createEnvelope(type, sessionId, payload)));
    return true;
  }

  sendAudio(buffer) {
    if (!this.isReady() || this.socket.bufferedAmount > MAX_BUFFERED_BYTES) return false;
    this.socket.send(buffer);
    return true;
  }

  isReady() {
    return this.socket?.readyState === WebSocket.OPEN;
  }

  disconnect() {
    this.manualClose = true;
    clearTimeout(this.reconnectTimer);
    this._stopHeartbeat();
    this._closeSocket();
    this._setState('offline');
  }

  _closeSocket() {
    const socket = this.socket;
    this.socket = null;
    if (socket && socket.readyState < WebSocket.CLOSING) socket.close();
  }

  _scheduleReconnect() {
    clearTimeout(this.reconnectTimer);
    this.reconnectTimer = setTimeout(() => {
      if (!this.manualClose && this.endpoint) this.connect(this.endpoint);
    }, 2500);
  }

  _startHeartbeat() {
    this._stopHeartbeat();
    this.heartbeatTimer = setInterval(() => {
      if (this.missedPongs >= 2) {
        this.callbacks.onError?.('AI 心跳超时，正在重新连接。');
        this.socket?.close();
        return;
      }
      this.missedPongs += 1;
      this.sendJson('ping', null);
    }, 15000);
  }

  _stopHeartbeat() {
    clearInterval(this.heartbeatTimer);
    this.heartbeatTimer = 0;
  }

  _setState(state) {
    this.state = state;
    this.callbacks.onState?.(state);
  }
}

export class VoiceControl {
  constructor() {
    this.panel = document.getElementById('voice-panel');
    this.toggleButton = document.getElementById('btn-voice-toggle');
    this.endpointInput = document.getElementById('voice-ai-endpoint');
    this.deviceSelect = document.getElementById('voice-device-select');
    this.micButton = document.getElementById('voice-mic-toggle');
    this.recordButton = document.getElementById('voice-record-toggle');
    this.encoder = new StreamingPcmEncoder();
    this.sessionId = null;
    this.recording = false;
    this.starting = false;
    this.recordingStartedAt = 0;
    this.recordingTimer = 0;
    this.recordingLimitTimer = 0;
    this.sessionReadyTimer = 0;
    this.pendingCandidate = null;
    this.finalTranscript = '';
    this.lastSocketErrorAt = 0;

    this.microphone = new MicrophoneManager({
      onState: state => this._renderMicState(state),
      onDevices: (devices, currentDeviceId) => this._renderDevices(devices, currentDeviceId),
      onLevel: level => this._renderLevel(level),
      onSamples: (samples, sampleRate) => this._handleSamples(samples, sampleRate),
      onDeviceLost: () => this._handleDeviceLost()
    });

    this.voiceSocket = new VoiceSocket({
      onState: state => this._renderAiState(state),
      onMessage: message => this._handleVoiceMessage(message),
      onError: message => this._handleSocketError(message)
    });
  }

  init() {
    if (!this.panel || !this.toggleButton) return;

    const savedEndpoint = localStorage.getItem('voiceAiEndpoint') || DEFAULT_ENDPOINT;
    this.endpointInput.value = savedEndpoint;

    this.toggleButton.addEventListener('click', () => this.togglePanel());
    document.getElementById('voice-panel-close').addEventListener('click', () => this.closePanel());
    document.getElementById('voice-ai-reconnect').addEventListener('click', () => this._connectAi());
    this.endpointInput.addEventListener('keydown', event => {
      if (event.key === 'Enter') this._connectAi();
    });
    this.endpointInput.addEventListener('change', () => {
      localStorage.setItem('voiceAiEndpoint', this.endpointInput.value.trim());
    });
    this.micButton.addEventListener('click', () => this._toggleMicrophone());
    this.deviceSelect.addEventListener('change', () => this._changeMicrophone());
    this.recordButton.addEventListener('click', () => {
      if (this.recording || this.starting) this.stopRecording();
      else this.startRecording();
    });
    document.getElementById('voice-intent-confirm').addEventListener('click', () => this._confirmCandidate());
    document.getElementById('voice-intent-cancel').addEventListener('click', () => this._cancelCandidate());
    document.addEventListener('visibilitychange', () => {
      if (!document.hidden && this.microphone.ready) this.microphone.refreshDevices();
    });
    window.addEventListener('pagehide', () => this.dispose(), { once: true });

    this._renderMicState('off');
    this._renderAiState('offline');
    this._connectAi();
  }

  togglePanel() {
    if (this.panel.classList.contains('open')) this.closePanel();
    else this.openPanel();
  }

  openPanel() {
    this.panel.classList.add('open');
    this.panel.setAttribute('aria-hidden', 'false');
    this.toggleButton.setAttribute('aria-expanded', 'true');
    if (!this.voiceSocket.isReady()) this._connectAi();
    if (this.microphone.ready) this.microphone.refreshDevices();
  }

  closePanel() {
    this.panel.classList.remove('open');
    this.panel.setAttribute('aria-hidden', 'true');
    this.toggleButton.setAttribute('aria-expanded', 'false');
  }

  async dispose() {
    clearInterval(this.recordingTimer);
    clearTimeout(this.recordingLimitTimer);
    clearTimeout(this.sessionReadyTimer);
    this.voiceSocket.disconnect();
    await this.microphone.dispose();
  }

  async startRecording() {
    if (!this.microphone.ready) {
      this._showNotice('请先启用并选择麦克风。', 'warning');
      return;
    }
    if (!this.voiceSocket.isReady()) {
      this._showNotice('本地 AI 3001 未连接，无法发送语音。', 'warning');
      return;
    }

    this._resetCandidate();
    this.sessionId = createId();
    this.encoder.reset(this.microphone.audioContext?.sampleRate || 48000);
    this.starting = true;
    this.finalTranscript = '';
    this._setSessionState('starting', '准备录音');

    const sent = this.voiceSocket.sendJson('session.start', this.sessionId, {
      audio: {
        encoding: 'pcm_s16le',
        sampleRate: TARGET_SAMPLE_RATE,
        channels: 1,
        frameMs: FRAME_MS
      }
    });

    if (!sent) {
      this.starting = false;
      this._setSessionState('error', '发送失败');
      return;
    }

    clearTimeout(this.sessionReadyTimer);
    this.sessionReadyTimer = setTimeout(() => {
      if (!this.starting) return;
      this.voiceSocket.sendJson('session.cancel', this.sessionId, { reason: 'ready_timeout' });
      this._abortRecording('AI 未在5秒内准备好录音会话。');
    }, SESSION_READY_TIMEOUT_MS);
  }

  stopRecording(reason = 'user_stop') {
    if (!this.recording && !this.starting) return;
    if (!this.voiceSocket.isReady()) {
      this._abortRecording('AI 连接已中断，录音未发送。');
      return;
    }
    this.starting = false;
    this.recording = false;
    clearInterval(this.recordingTimer);
    clearTimeout(this.recordingLimitTimer);
    clearTimeout(this.sessionReadyTimer);

    for (const frame of this.encoder.flush()) {
      if (!this.voiceSocket.sendAudio(frame)) {
        this._showNotice('音频发送队列过大，录音已停止。', 'error');
        break;
      }
    }

    this.voiceSocket.sendJson('session.stop', this.sessionId, { reason });
    this._setSessionState('processing', '正在识别');
    this._renderRecordButton();
  }

  _connectAi() {
    const endpoint = this.endpointInput.value.trim() || DEFAULT_ENDPOINT;
    localStorage.setItem('voiceAiEndpoint', endpoint);
    this.voiceSocket.connect(endpoint);
  }

  async _toggleMicrophone() {
    if (this.recording || this.starting) {
      this._showNotice('请先停止当前录音。', 'warning');
      return;
    }

    if (this.microphone.ready) {
      await this.microphone.disable();
      return;
    }

    try {
      await this.microphone.enable(this.deviceSelect.value);
      this._showNotice('麦克风已启用，音频仅在点击“开始录音”后发送。', 'success');
    } catch (error) {
      this._renderMicState('error');
      this._showNotice(formatMicError(error), 'error');
      if (error?.name === 'OverconstrainedError') {
        try { await this.microphone.enable(''); } catch {}
      }
    }
  }

  async _changeMicrophone() {
    if (!this.microphone.ready || this.recording || this.starting) return;
    this.deviceSelect.disabled = true;
    try {
      await this.microphone.switchDevice(this.deviceSelect.value);
      this._showNotice('已切换麦克风。', 'success');
    } catch (error) {
      this._showNotice(formatMicError(error), 'error');
    } finally {
      this.deviceSelect.disabled = !this.microphone.ready;
    }
  }

  _handleSamples(samples, sampleRate) {
    if (!this.recording) return;
    if (this.encoder.inputSampleRate !== sampleRate && this.encoder.sequence === 0) {
      this.encoder.reset(sampleRate);
    }
    for (const frame of this.encoder.push(samples)) {
      if (!this.voiceSocket.sendAudio(frame)) {
        this._abortRecording('AI 接收速度不足，录音已安全停止。');
        return;
      }
    }
  }

  _handleVoiceMessage(message) {
    if (message.type !== 'pong' && message.sessionId && message.sessionId !== this.sessionId) return;

    switch (message.type) {
      case 'session.ready':
        clearTimeout(this.sessionReadyTimer);
        this.starting = false;
        this.recording = true;
        this.recordingStartedAt = performance.now();
        this._setSessionState('recording', '正在录音');
        this._renderRecordButton();
        this.recordingTimer = setInterval(() => {
          const elapsed = performance.now() - this.recordingStartedAt;
          document.getElementById('voice-recording-time').textContent = formatDuration(elapsed);
        }, 200);
        this.recordingLimitTimer = setTimeout(() => {
          this.stopRecording('duration_limit');
          this._showNotice('已达到30秒上限，开始识别。', 'warning');
        }, MAX_RECORDING_MS);
        break;

      case 'transcript.partial':
        this._renderPartial(message.payload?.text || '', message.payload?.revision || 0);
        break;

      case 'session.processing':
        this._setSessionState('processing', '正在识别');
        break;

      case 'transcript.final':
        this.finalTranscript = message.payload?.text || '';
        this._renderFinalTranscript(this.finalTranscript, message.payload?.confidence);
        break;

      case 'intent.candidate':
        this.pendingCandidate = message.payload;
        if (message.payload?.requiresConfirmation === false && message.payload?.intent === 'robot.estop') {
          this._simulateCandidate(message.payload, true);
        } else {
          this._renderCandidate(message.payload);
          this._setSessionState('confirm', '等待确认');
        }
        break;

      case 'session.completed':
        if (!this.pendingCandidate) this._setSessionState('ready', '可以录音');
        break;

      case 'session.cancelled':
        this._setSessionState('ready', '已取消');
        break;

      case 'error':
        this.starting = false;
        if (this.recording) this.stopRecording('server_error');
        this._setSessionState('error', '协议错误');
        this._showNotice(message.payload?.message || 'Voice Protocol 返回错误。', 'error');
        break;
    }
  }

  _renderAiState(state) {
    const status = document.getElementById('voice-ai-status');
    const led = document.getElementById('voice-toggle-led');
    const labels = {
      offline: 'AI 离线',
      connecting: 'AI 连接中',
      ready: 'AI 已连接'
    };
    status.textContent = labels[state] || state;
    status.dataset.state = state;
    led.dataset.state = state;
    if (state === 'offline' && (this.recording || this.starting)) {
      this._abortRecording('AI 连接已中断，录音已停止。');
    }
    this._renderRecordButton();
  }

  _renderMicState(state) {
    const status = document.getElementById('voice-mic-status');
    const labels = {
      off: '麦克风关闭',
      requesting: '等待授权',
      ready: '麦克风就绪',
      error: '麦克风异常'
    };
    status.textContent = labels[state] || state;
    status.dataset.state = state;
    this.micButton.textContent = state === 'ready' ? '关闭麦克风' : '启用麦克风';
    this.deviceSelect.disabled = state !== 'ready' || this.recording || this.starting;
    this._renderRecordButton();
  }

  _renderDevices(devices, currentDeviceId) {
    const previous = currentDeviceId || this.deviceSelect.value;
    this.deviceSelect.replaceChildren();

    if (devices.length === 0) {
      const option = new Option('未检测到麦克风', '');
      this.deviceSelect.add(option);
      this.deviceSelect.disabled = true;
      return;
    }

    devices.forEach((device, index) => {
      const label = device.label || `麦克风 ${index + 1}`;
      this.deviceSelect.add(new Option(label, device.deviceId));
    });

    if ([...this.deviceSelect.options].some(option => option.value === previous)) {
      this.deviceSelect.value = previous;
    }
    this.deviceSelect.disabled = !this.microphone.ready || this.recording || this.starting;
  }

  _renderLevel(level) {
    const percent = Math.round(level * 100);
    document.getElementById('voice-level-fill').style.width = `${percent}%`;
    document.getElementById('voice-level-value').textContent = `${percent}`;
    this.panel.style.setProperty('--voice-level', `${percent}%`);
  }

  _renderRecordButton() {
    const canRecord = this.microphone.ready && this.voiceSocket.isReady();
    this.recordButton.disabled = !canRecord && !this.recording && !this.starting;
    this.recordButton.classList.toggle('is-recording', this.recording || this.starting);
    this.recordButton.querySelector('span:last-child').textContent =
      this.recording || this.starting ? '停止录音' : '开始录音';
    this.deviceSelect.disabled = !this.microphone.ready || this.recording || this.starting;
  }

  _setSessionState(state, label) {
    const status = document.getElementById('voice-session-status');
    status.textContent = label;
    status.dataset.state = state;
    this.panel.dataset.sessionState = state;
    if (state !== 'recording') {
      document.getElementById('voice-recording-time').textContent = '00:00';
    }
  }

  _renderPartial(text, revision) {
    if (!text) return;
    let message = document.querySelector('.voice-message--partial');
    if (!message) {
      message = this._createMessage('ai', '', '正在识别');
      message.classList.add('voice-message--partial');
    }
    if (revision >= Number(message.dataset.revision || 0)) {
      message.dataset.revision = revision;
      message.querySelector('.voice-message-text').textContent = text;
    }
  }

  _renderFinalTranscript(text, confidence) {
    document.querySelector('.voice-message--partial')?.remove();
    const meta = Number.isFinite(confidence) ? `最终文字 · ${Math.round(confidence * 100)}%` : '最终文字';
    this._createMessage('user', text || '未识别到文字', meta);
  }

  _renderCandidate(candidate) {
    const card = document.getElementById('voice-intent-card');
    const supported = this._isAllowedCandidate(candidate);
    const intentLabel = this._intentLabel(candidate.intent, candidate.args);
    document.getElementById('voice-intent-name').textContent = intentLabel;
    document.getElementById('voice-intent-source').textContent =
      `来自：“${candidate.sourceText || this.finalTranscript || '—'}”`;
    document.getElementById('voice-intent-confidence').textContent =
      Number.isFinite(candidate.confidence) ? `${Math.round(candidate.confidence * 100)}%` : '--';
    const confirmButton = document.getElementById('voice-intent-confirm');
    confirmButton.disabled = !supported;
    confirmButton.textContent = supported ? '确认模拟执行' : '不支持此动作';
    card.hidden = false;
  }

  _confirmCandidate() {
    if (!this.pendingCandidate || !this._isAllowedCandidate(this.pendingCandidate)) return;
    this._simulateCandidate(this.pendingCandidate, false);
  }

  _cancelCandidate() {
    if (!this.pendingCandidate) return;
    this._createMessage('system', `已取消：${this._intentLabel(this.pendingCandidate.intent, this.pendingCandidate.args)}`, '没有发送控制指令');
    this._resetCandidate();
    this._setSessionState('ready', '可以录音');
  }

  _simulateCandidate(candidate, immediate) {
    const label = this._intentLabel(candidate.intent, candidate.args);
    this._createMessage(
      'system',
      `模拟执行：${label}`,
      immediate ? '急停候选 · 原型未连接硬件' : '用户已确认 · 原型未连接硬件'
    );
    this._resetCandidate();
    this._setSessionState('ready', '模拟完成');
  }

  _resetCandidate() {
    this.pendingCandidate = null;
    document.getElementById('voice-intent-card').hidden = true;
    const confirmButton = document.getElementById('voice-intent-confirm');
    confirmButton.disabled = false;
    confirmButton.textContent = '确认模拟执行';
  }

  _createMessage(role, text, meta) {
    const list = document.getElementById('voice-conversation');
    list.querySelector('.voice-empty')?.remove();

    const message = document.createElement('article');
    message.className = `voice-message voice-message--${role}`;

    const label = document.createElement('div');
    label.className = 'voice-message-meta';
    label.textContent = meta;

    const content = document.createElement('div');
    content.className = 'voice-message-text';
    content.textContent = text;

    message.append(label, content);
    list.appendChild(message);
    list.scrollTop = list.scrollHeight;
    return message;
  }

  _showNotice(message, tone = 'info') {
    this._createMessage('system', message, tone === 'error' ? '需要处理' : '系统提示');
  }

  _handleSocketError(message) {
    const now = Date.now();
    if (now - this.lastSocketErrorAt < 15000) return;
    this.lastSocketErrorAt = now;
    this._showNotice(message, 'error');
  }

  _abortRecording(message) {
    this.starting = false;
    this.recording = false;
    clearInterval(this.recordingTimer);
    clearTimeout(this.recordingLimitTimer);
    clearTimeout(this.sessionReadyTimer);
    this._setSessionState('error', '录音已停止');
    this._renderRecordButton();
    if (message) this._showNotice(message, 'error');
  }

  _isAllowedCandidate(candidate) {
    const validator = INTENT_VALIDATORS[candidate?.intent];
    return Boolean(validator && validator(candidate?.args || {}));
  }

  _intentLabel(intent, args = {}) {
    const labels = {
      'robot.estop': '机械臂急停',
      'robot.status': '查询机器人状态',
      'robot.preset': `前往预设位 ${args?.name || ''}`.trim(),
      'gripper.open': '打开夹爪',
      'gripper.close': '关闭夹爪',
      'gripper.grip': '执行夹取',
      'gripper.set_position': `夹爪移动到 ${args?.position ?? '--'}`
    };
    return labels[intent] || `不支持的动作：${intent || 'unknown'}`;
  }

  async _handleDeviceLost() {
    this.stopRecording('device_lost');
    this._showNotice('当前麦克风已断开，正在尝试使用默认设备。', 'warning');
    try {
      await this.microphone.enable('');
    } catch (error) {
      this._renderMicState('error');
      this._showNotice(formatMicError(error), 'error');
    }
  }
}
