/**
 * WSClient — WebSocket 客户端
 * 连接 Node.js 代理，收发 JSON 消息
 */
class WSClient {
  constructor() {
    this.ws = null;
    this.connected = false;
    this.listeners = {};
    this._reconnectDelay = 1000;
    this._maxDelay = 5000;
    this._reconnectTimer = null;
  }

  connect() {
    const proto = location.protocol === 'https:' ? 'wss' : 'ws';
    const url = `${proto}://${location.host}/ws`;
    console.log(`[WS] connecting to ${url}...`);

    this.ws = new WebSocket(url);

    this.ws.onopen = () => {
      this.connected = true;
      this._reconnectDelay = 1000;
      console.log('[WS] connected');
      this._emit('connection', { connected: true });
    };

    this.ws.onmessage = (evt) => {
      try {
        const data = JSON.parse(evt.data);
        this._emit('message', data);
        if (data.type) this._emit(data.type, data);
      } catch (e) {
        console.warn('[WS] bad JSON:', e);
      }
    };

    this.ws.onclose = () => {
      this.connected = false;
      console.log('[WS] disconnected');
      this._emit('connection', { connected: false });
      this._scheduleReconnect();
    };

    this.ws.onerror = (err) => {
      console.error('[WS] error:', err);
    };
  }

  _scheduleReconnect() {
    if (this._reconnectTimer) return;
    console.log(`[WS] reconnecting in ${this._reconnectDelay}ms...`);
    this._reconnectTimer = setTimeout(() => {
      this._reconnectTimer = null;
      this._reconnectDelay = Math.min(this._reconnectDelay * 1.5, this._maxDelay);
      this.connect();
    }, this._reconnectDelay);
  }

  send(data) {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      console.warn('[WS] not connected, cannot send');
      return false;
    }
    this.ws.send(JSON.stringify(data));
    return true;
  }

  on(event, callback) {
    if (!this.listeners[event]) this.listeners[event] = [];
    this.listeners[event].push(callback);
  }

  _emit(event, data) {
    const cbs = this.listeners[event];
    if (cbs) cbs.forEach(cb => cb(data));
  }
}
