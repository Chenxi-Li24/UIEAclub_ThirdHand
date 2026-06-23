/**
 * ThirdHand Web Control — Node.js Proxy Server
 *
 * WiFi/UDP bridge: Browser WebSocket ↔ ESP32-P4
 * Sends heartbeat every 1s, forwards estop/reset commands
 */

const express = require('express');
const http = require('http');
const { WebSocketServer } = require('ws');
const dgram = require('dgram');
const path = require('path');
const os = require('os');
const config = require('./config');

// ── Express 静态文件 ──────────────────────────────────────────────
const app = express();
app.use(express.static(path.join(__dirname, '..', 'web')));
const server = http.createServer(app);

// ── WebSocket 服务器 ──────────────────────────────────────────────
const wss = new WebSocketServer({ server, path: '/ws' });
const clients = new Set();

// ── 传输层抽象 ────────────────────────────────────────────────────
let transport = null;
let heartbeatTimer = null;

/** UDP (WiFi) 传输 — 单 socket 收发 */
class WifiTransport {
  constructor(host, port) {
    this.host = host;
    this.port = port;
    this.socket = dgram.createSocket('udp4');
    this.onMessageCallback = null;
    this.connected = true;

    this.socket.on('message', (msg, rinfo) => {
      const text = msg.toString('utf8').trim();
      console.log(`[UDP←${rinfo.address}] ${text}`);
      if (this.onMessageCallback) this.onMessageCallback(text);
    });
    this.socket.on('error', (err) => {
      console.error('[UDP] error:', err.message);
    });
    this.socket.bind(config.web.port + 1);
    console.log(`[WiFi] UDP target: ${host}:${port}, listening on ${config.web.port + 1}`);
  }

  send(text) {
    const buf = Buffer.from(text, 'utf8');
    this.socket.send(buf, 0, buf.length, this.port, this.host, (err) => {
      if (err) console.error('[UDP] send error:', err.message);
      else console.log(`[UDP→] ${text.trim()}`);
    });
  }

  onMessage(cb) { this.onMessageCallback = cb; }

  close() {
    this.connected = false;
    try { this.socket.close(); } catch {}
    console.log('[WiFi] transport closed');
  }

  getInfo() { return { mode: 'wifi', host: this.host, port: this.port, connected: this.connected }; }
}

// ── 连接 ESP32 (WiFi/UDP) ─────────────────────────────────────────
async function connectWifi(opts) {
  if (transport) {
    transport.close();
    transport = null;
  }
  if (heartbeatTimer) { clearInterval(heartbeatTimer); heartbeatTimer = null; }

  try {
    const host = opts.host || config.connection.wifi.host;
    const port = opts.port || config.connection.wifi.port;
    transport = new WifiTransport(host, port);

    // Start heartbeat every 1s
    heartbeatTimer = setInterval(() => {
      if (transport && transport.connected) {
        transport.send('heartbeat');
      }
    }, 1000);

    transport.onMessage((text) => {
      // Parse ESP32 joint angles + state + heartbeat age
      if (text.startsWith('JOINTS:')) {
        const parts = text.substring(7).split(',').map(Number);
        if (parts.length >= 8) {
          const state = parts[6];
          const hbAge = parts[7];
          const stateNames = ['IDLE', 'MOVING', 'E-STOP', 'ERROR', 'LOCKED'];
          broadcast({
            type: 'cnde_state',
            joints: parts.slice(0, 6),
            robotState: state,
            stateName: stateNames[state] || '???',
            heartbeatAge: hbAge,
            ts: Date.now()
          });
          return;
        }
        if (parts.length >= 7) {
          broadcast({
            type: 'cnde_state',
            joints: parts.slice(0, 6),
            robotState: 0,
            programState: parts[6],
            ts: Date.now()
          });
        }
        return;
      }
      try {
        const data = JSON.parse(text);
        broadcast(data);
      } catch {
        broadcast({ type: 'esp32_msg', raw: text });
      }
    });

    broadcast({ type: 'connection', ...transport.getInfo() });
    console.log(`[WiFi] connected to ${host}:${port}`);
  } catch (err) {
    broadcast({ type: 'connection', mode: 'wifi', connected: false, error: err.message });
    console.error('[WiFi] connect failed:', err.message);
    transport = null;
  }
}

// ── WebSocket 广播 ────────────────────────────────────────────────
function broadcast(data) {
  const json = JSON.stringify(data);
  for (const ws of clients) {
    if (ws.readyState === 1) ws.send(json);
  }
}

// ── WebSocket 连接处理 ────────────────────────────────────────────
wss.on('connection', (ws) => {
  clients.add(ws);
  console.log(`[WS] client connected (${clients.size} total)`);

  ws.send(JSON.stringify({
    type: 'config',
    presets: config.presets,
    servo: config.servo,
    connection: transport ? transport.getInfo() : { mode: 'wifi', connected: false }
  }));

  ws.on('message', (data) => {
    try {
      const msg = JSON.parse(data.toString());
      handleBrowserCommand(msg, ws);
    } catch (e) {
      console.error('[WS] bad JSON:', e.message);
    }
  });

  ws.on('close', () => {
    clients.delete(ws);
    console.log(`[WS] client disconnected (${clients.size} total)`);
  });
});

// ── 浏览器命令处理 ────────────────────────────────────────────────
async function handleBrowserCommand(msg, ws) {
  switch (msg.cmd) {
    case 'connect': {
      await connectWifi({ port: msg.port, host: msg.host });
      break;
    }

    case 'disconnect': {
      const info = transport ? transport.getInfo() : {};
      if (transport) {
        transport.close();
        transport = null;
      }
      if (heartbeatTimer) { clearInterval(heartbeatTimer); heartbeatTimer = null; }
      broadcast({ type: 'connection', mode: 'wifi', connected: false, port: info.port, host: info.host });
      console.log('[WiFi] disconnected');
      break;
    }

    case 'servo': {
      if (!transport || !transport.connected) {
        ws.send(JSON.stringify({ type: 'error', msg: '未连接到控制板' }));
        return;
      }
      if (!Array.isArray(msg.joints) || msg.joints.length !== 6) {
        ws.send(JSON.stringify({ type: 'error', msg: 'joints must be [j1..j6]' }));
        return;
      }
      const j = msg.joints.map(v => v.toFixed(3)).join(' ');
      transport.send(`servo j1 ${j}`);
      break;
    }

    case 'servo_raw': {
      if (transport && transport.connected) transport.send(msg.text);
      break;
    }

    case 'preset': {
      if (!transport || !transport.connected) {
        ws.send(JSON.stringify({ type: 'error', msg: '未连接到控制板' }));
        return;
      }
      const joints = config.presets[msg.name];
      if (!joints) {
        ws.send(JSON.stringify({ type: 'error', msg: `unknown preset: ${msg.name}` }));
        return;
      }
      const j = joints.map(v => v.toFixed(3)).join(' ');
      transport.send(`servo j1 ${j}`);
      break;
    }

    case 'estop': {
      if (transport && transport.connected) {
        // 连续发送停止指令确保立即打断当前运动
        transport.send('estop');
        transport.send('servo end');
        setTimeout(() => transport.send('estop'), 50);
        broadcast({ type: 'estop', ts: Date.now() });
      } else {
        ws.send(JSON.stringify({ type: 'error', msg: '未连接到控制板' }));
      }
      break;
    }

    case 'reset': {
      if (transport && transport.connected) transport.send('reset');
      else ws.send(JSON.stringify({ type: 'error', msg: '未连接到控制板' }));
      break;
    }

    case 'status': {
      if (transport && transport.connected) transport.send('status');
      break;
    }

    case 'ping': {
      ws.send(JSON.stringify({ type: 'pong', ts: Date.now() }));
      break;
    }

    default:
      ws.send(JSON.stringify({ type: 'error', msg: `unknown cmd: ${msg.cmd}` }));
  }
}

// ── 获取本机 IP ──────────────────────────────────────────────────
function getLocalIPs() {
  const interfaces = os.networkInterfaces();
  const ips = [];
  for (const name of Object.keys(interfaces)) {
    for (const iface of interfaces[name]) {
      if (iface.family === 'IPv4' && !iface.internal) {
        ips.push(iface.address);
      }
    }
  }
  return ips;
}

// ── 启动 ──────────────────────────────────────────────────────────
server.listen(config.web.port, config.web.host, async () => {
  const ips = getLocalIPs();
  console.log('═══════════════════════════════════════════════');
  console.log('  ThirdHand Web Control Panel');
  console.log(`  Local:   http://localhost:${config.web.port}`);
  ips.forEach(ip => console.log(`  Network: http://${ip}:${config.web.port}`));
  console.log(`  ESP32:   ${config.connection.wifi.host}:${config.connection.wifi.port}`);
  console.log('═══════════════════════════════════════════════');

  await connectWifi(config.connection.wifi);
});

process.on('SIGINT', () => {
  console.log('\n[shutdown] closing...');
  if (heartbeatTimer) clearInterval(heartbeatTimer);
  if (transport) transport.close();
  server.close();
  process.exit(0);
});
