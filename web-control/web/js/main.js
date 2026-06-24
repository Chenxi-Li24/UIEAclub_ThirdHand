console.log('[main.js] Loading modules...');
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
console.log('[main.js] Modules imported, THREE keys:', Object.keys(THREE).length);

// === SceneManager ===
class SceneManager {
  constructor(container) {
    this.container = container;
    this.scene = null;
    this.camera = null;
    this.renderer = null;
    this.controls = null;
    this.grid = null;
    this.axes = null;
    this.showGrid = true;
    this.showAxes = true;
    this._init();
  }

  _init() {
    const w = this.container.clientWidth;
    const h = this.container.clientHeight;

    this.scene = new THREE.Scene();
    // 背景色跟随 CSS --bg-primary（通过计算样式获取），默认深空色
    const bgColor = this._getComputedBgColor();
    this.scene.background = bgColor;
    this.scene.fog = new THREE.Fog(bgColor.getHex(), 800, 3000);

    this.camera = new THREE.PerspectiveCamera(50, w / h, 1, 5000);
    this.camera.position.set(600, 400, 600);
    this.camera.lookAt(0, 200, 0);

    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.renderer.setSize(w, h);
    this.renderer.setPixelRatio(window.devicePixelRatio);
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    this.container.appendChild(this.renderer.domElement);

    if (OrbitControls) {
      this.controls = new OrbitControls(this.camera, this.renderer.domElement);
      this.controls.enableDamping = true;
      this.controls.dampingFactor = 0.08;
      this.controls.target.set(0, 250, 0);
      this.controls.minDistance = 200;
      this.controls.maxDistance = 2000;
    }

    this.scene.add(new THREE.AmbientLight(0xffffff, 0.6));
    this.scene.add(new THREE.HemisphereLight(0xffffff, 0x444444, 0.4));

    const dirLight1 = new THREE.DirectionalLight(0xffffff, 0.8);
    dirLight1.position.set(500, 800, 500);
    this.scene.add(dirLight1);

    const dirLight2 = new THREE.DirectionalLight(0xffffff, 0.4);
    dirLight2.position.set(-500, 400, -300);
    this.scene.add(dirLight2);

    const dirLight3 = new THREE.DirectionalLight(0xffffff, 0.3);
    dirLight3.position.set(0, -200, 500);
    this.scene.add(dirLight3);

    this.grid = new THREE.GridHelper(1200, 24, 0x2d3a5c, 0x1a2340);
    this.scene.add(this.grid);

    this.axes = new THREE.AxesHelper(250);
    this.scene.add(this.axes);

    window.addEventListener('resize', () => this._onResize());
    // 监听主题切换更新背景色
    const observer = new MutationObserver(() => this._updateBgColor());
    observer.observe(document.documentElement, { attributes: true, attributeFilter: ['data-theme'] });

    this._animate();
  }

  _getComputedBgColor() {
    const style = getComputedStyle(document.documentElement);
    const bg = style.getPropertyValue('--bg-primary').trim();
    // 解析 css var，从 computedStyle 获取实际值
    const computed = document.documentElement.style.getPropertyValue('--bg-primary');
    if (computed) {
      const el = document.createElement('div');
      el.style.color = computed;
      document.body.appendChild(el);
      const color = getComputedStyle(el).color;
      document.body.removeChild(el);
      const m = color.match(/(\d+),\s*(\d+),\s*(\d+)/);
      if (m) return new THREE.Color(+m[1]/255, +m[2]/255, +m[3]/255);
    }
    return new THREE.Color(0x0a0e1a);
  }

  _updateBgColor() {
    const bgColor = this._getComputedBgColor();
    if (this.scene) {
      this.scene.background = bgColor;
      this.scene.fog.color = bgColor;
    }
  }

  _onResize() {
    const w = this.container.clientWidth;
    const h = this.container.clientHeight;
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(w, h);
  }

  _animate() {
    requestAnimationFrame(() => this._animate());
    if (this.controls) this.controls.update();
    this.renderer.render(this.scene, this.camera);
  }

  toggleGrid() {
    this.showGrid = !this.showGrid;
    this.grid.visible = this.showGrid;
    return this.showGrid;
  }

  toggleAxes() {
    this.showAxes = !this.showAxes;
    this.axes.visible = this.showAxes;
    return this.showAxes;
  }

  resetView() {
    this.camera.position.set(600, 400, 600);
    if (this.controls) {
      this.controls.target.set(0, 250, 0);
      this.controls.update();
    }
  }
}


// === ArmModel ===
class ArmModel {
  constructor(scene) {
    this.scene = scene;
    this.robot = null;
    this.jointAngles = [0, 0, 0, 0, 0, 0];
    this.jointNames = [];
    this.loaded = false;
    this.installMode = 'floor';
    this.zeroOffset = [0, 90, -90, 90, 90, 0];
    this.loadingPromise = this._loadURDF();
  }

  _loadURDF() {
    return new Promise((resolve, reject) => {
      const loader = new window.URDFLoader();
      loader.load('models/TCR050/TCR050.urdf', (robot) => {
        this.robot = robot;
        robot.scale.set(1000, 1000, 1000);
        robot.rotation.x = -Math.PI / 2;

        robot.traverse(child => {
          if (child.isMesh) {
            child.material = new THREE.MeshStandardMaterial({
              color: 0xcccccc, metalness: 0.2, roughness: 0.6,
              flatShading: false, side: THREE.DoubleSide,
            });
          }
        });

        this.jointNames = Object.keys(robot.joints);
        console.log('[ArmModel] URDF joints:', this.jointNames);

        this.jointLimits = this.jointNames.map(name => {
          const j = robot.joints[name];
          const lower = j.limit?.lower != null ? j.limit.lower * 180 / Math.PI : -180;
          const upper = j.limit?.upper != null ? j.limit.upper * 180 / Math.PI : 180;
          return [lower, upper];
        });

        this.scene.add(robot);
        this.loaded = true;
        console.log('[ArmModel] URDF loaded, joints:', this.jointNames.length);
        resolve();
      }, undefined, (err) => {
        console.error('[ArmModel] URDF load failed:', err);
        reject(err);
      });
    });
  }

  setJointAngles(angles) {
    if (!this.robot || !angles) return;
    for (let i = 0; i < this.jointNames.length && i < angles.length; i++) {
      const offset = this.zeroOffset[i] || 0;
      const actual = angles[i] + offset;
      const [mn, mx] = this.jointLimits[i] || [-180, 180];
      const clamped = Math.max(mn, Math.min(mx, actual));
      this.robot.joints[this.jointNames[i]].setJointValue(clamped * Math.PI / 180);
    }
    this.jointAngles = angles.slice();
  }

  getJointAngles() { return this.jointAngles.slice(); }

  setInstallMode(mode) {
    if (!this.robot) return;
    this.installMode = mode;
    if (mode === 'floor') {
      this.robot.rotation.set(-Math.PI / 2, 0, 0);
    } else {
      this.robot.rotation.set(0, 0, Math.PI / 2);
    }
    console.log('[ArmModel] Install mode:', mode);
  }

  goHome() {
    this.setJointAngles(new Array(this.jointNames.length).fill(0));
  }

  getEndEffectorPosition() {
    if (!this.robot) return { x: 0, y: 0, z: 0 };
    const p = new THREE.Vector3();
    const lastLink = this.jointNames[this.jointNames.length - 1];
    if (this.robot.joints[lastLink]) {
      this.robot.joints[lastLink].getWorldPosition(p);
    }
    return { x: p.x, y: p.y, z: p.z };
  }
}


// === WSClient ===
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


// === UIControls ===
class UIControls {
  constructor(wsClient, armModel) {
    this.ws = wsClient;
    this.arm = armModel;
    this.sliders = [];
    this.inputs = [];
    this.liveMode = false;
    this.syncMode = false;
    this.presets = {};
    this._drawerCollapsed = false;
  }

  build() {
    // ── 构建关节控制区 ────────────────────────────────
    const body = document.getElementById('joint-controls-body');
    if (!body) return;

    const jointNames = this.arm.jointNames || [];
    const limits = this.arm.jointLimits || [];

    for (let i = 0; i < jointNames.length; i++) {
      const row = document.createElement('div');
      row.className = 'joint-row';
      row.innerHTML = `
        <div class="joint-label">
          <span class="joint-name">${jointNames[i]}</span>
          <span class="joint-value" id="jval-${i}">0.0°</span>
        </div>
        <input type="range" class="joint-slider"
               id="slider-j${i}"
               min="${limits[i][0]}" max="${limits[i][1]}" step="0.1" value="0">
      `;
      body.appendChild(row);

      const slider = row.querySelector('.joint-slider');
      const valEl = row.querySelector('.joint-value');
      this.sliders.push(slider);
      // 默认姿态: [0, -90, 90, -90, -90, 0]
      const defaultVal = [0, -90, 90, -90, -90, 0][i] || 0;
      slider.value = defaultVal;
      valEl.textContent = defaultVal.toFixed(1) + '°';

      slider.addEventListener('input', () => {
        const val = parseFloat(slider.value);
        valEl.textContent = val.toFixed(1) + '°';
        this._updateArm(i, val);
        if (this.liveMode) this._sendServo();
      });
    }

    // ── 绑定各 UI 事件 ─────────────────────────────────

    // 连接按钮
    document.getElementById('btn-connect').addEventListener('click', () => {
      const host = document.getElementById('wifi-host').value;
      const port = parseInt(document.getElementById('wifi-port').value) || 20008;
      this.ws.send({ cmd: 'connect', mode: 'wifi', host, port });
      this._log(`→ 连接 ${host}:${port}`);
      this._setConnStatus('connecting', host);
    });

    document.getElementById('btn-disconnect').addEventListener('click', () => {
      this.ws.send({ cmd: 'disconnect', mode: 'wifi' });
      this._log('→ 断开连接');
      this._setConnStatus('disconnected', '');
    });

    // 发送按钮
    document.getElementById('btn-send').addEventListener('click', () => {
      this._sendServo();
    });

    // 急停
    document.getElementById('btn-estop-top').addEventListener('click', () => {
      this.ws.send({ cmd: 'estop' });
      this._log('→ E-STOP');
    });

    // 回零
    document.getElementById('btn-home').addEventListener('click', () => {
      this.arm.goHome();
      const zeros = new Array(this.arm.jointNames.length).fill(0);
      this.setJointValues(zeros);
      this._log('→ 回零');
    });

    // 实时发送
    document.getElementById('chk-live').addEventListener('change', (e) => {
      this.liveMode = e.target.checked;
    });

    // 同步 3D 模型
    document.getElementById('chk-sync').addEventListener('change', (e) => {
      this.syncMode = e.target.checked;
    });

    // 抽屉收起/展开
    const drawer = document.getElementById('control-drawer');
    const handle = document.getElementById('drawer-handle');
    const toggleBtn = document.getElementById('btn-drawer-toggle');

    const collapseDrawer = () => {
      drawer.classList.add('collapsed');
      this._drawerCollapsed = true;
    };
    const expandDrawer = () => {
      drawer.classList.remove('collapsed');
      this._drawerCollapsed = false;
    };
    const toggleDrawer = () => {
      if (this._drawerCollapsed) expandDrawer(); else collapseDrawer();
    };

    // handle 固定显示 ◂，仅收起时可见（CSS 控制）
    handle.textContent = '◂';
    handle.addEventListener('click', (e) => {
      e.stopPropagation();
      toggleDrawer();
    });
    toggleBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      toggleDrawer();
    });

    // section 折叠
    document.querySelectorAll('.sec-header').forEach(header => {
      header.addEventListener('click', () => {
        const section = header.closest('.drawer-section');
        section.classList.toggle('collapsed');
      });
    });

    // 抽屉内视图按钮（暂时禁止冒泡，防止触发视口交互）
    document.getElementById('btn-send').addEventListener('click', e => e.stopPropagation());
    document.getElementById('btn-home').addEventListener('click', e => e.stopPropagation());
    document.getElementById('btn-reset').addEventListener('click', e => e.stopPropagation());
    document.querySelectorAll('.sec-header').forEach(h => {
      h.addEventListener('click', e => e.stopPropagation());
    });
    document.querySelectorAll('.joint-slider').forEach(s => {
      s.addEventListener('input', e => e.stopPropagation());
    });

    // 抽屉内连接按钮
    document.getElementById('btn-connect').addEventListener('click', e => e.stopPropagation());
    document.getElementById('btn-disconnect').addEventListener('click', e => e.stopPropagation());

    // 日志面板（按钮在连接栏中）
    const logPanel = document.getElementById('log-panel');
    const logBtn = document.getElementById('btn-log-toggle');
    logBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      const open = logPanel.classList.toggle('open');
      logBtn.classList.toggle('active', open);
    });
    document.getElementById('btn-log-close').addEventListener('click', () => {
      logPanel.classList.remove('open');
      logBtn.classList.remove('active');
    });

    // WebSocket 事件监听
    this.ws.on('connection', (data) => {
      if (data.connected) {
        this._setConnStatus('connected', `${data.host}:${data.port}`);
      } else {
        this._setConnStatus('disconnected', '');
        // 断开时闪红框
        const app = document.getElementById('app');
        app.classList.remove('disconnected');
        void app.offsetWidth; // reflow
        app.classList.add('disconnected');
        setTimeout(() => app.classList.remove('disconnected'), 1100);
      }
    });

    this.ws.on('config', (data) => {
      console.log('[UI] config received, presets:', data.presets ? Object.keys(data.presets) : 'none');
      if (data.presets) this._setPresets(data.presets);
      if (data.connection) this._setConnStatus(data.connection.connected ? 'connected' : 'disconnected', `${data.connection.host}:${data.connection.port}`);
    });

    this.ws.on('cnde_state', (data) => {
      // 更新实时角度
      if (data.joints && data.joints.length >= 6) {
        this._updateRTAngles(data.joints);
        // 同步滑块
        if (this.syncMode) {
          this.setJointValues(data.joints);
        }
      }
      // 更新状态徽章
      if (data.stateName) {
        this._updateStateBadge(data.stateName);
        // E-STOP 遮罩
        const overlay = document.getElementById('estop-overlay');
        if (data.stateName === 'E-STOP') {
          overlay.classList.add('active');
        } else {
          overlay.classList.remove('active');
        }
        // 复位按钮
        const resetBtn = document.getElementById('btn-reset');
        if (resetBtn) {
          resetBtn.style.display = ['E-STOP', 'ERROR', 'LOCKED'].includes(data.stateName) ? '' : 'none';
        }
      }
      // 心跳
      if (data.heartbeatAge !== undefined) {
        const hb = document.getElementById('hb-label');
        if (hb) {
          const ok = data.heartbeatAge < 2000;
          hb.textContent = ok ? 'hb:OK' : 'hb:LOST';
          hb.className = `hb-label ${ok ? 'ok' : 'lost'}`;
        }
      }
    });

    this.ws.on('esp32_msg', (data) => {
      this._log('← ' + data.raw);
      const parts = data.raw.split(',').map(Number);
      if (parts.length === 6 && parts.every(n => !isNaN(n))) {
        this._updateRTAngles(parts);
      }
    });

    this.ws.on('error', (data) => this._log('⚠ ' + data.msg));

    // 如有预设提前到达，补渲染
    if (Object.keys(this.presets).length) {
      this._setPresets(this.presets);
    }
  }

  _setConnStatus(mode, info) {
    const dot = document.getElementById('conn-dot');
    const ip = document.getElementById('conn-ip');
    const btnConnect = document.getElementById('btn-connect');
    const btnDisconnect = document.getElementById('btn-disconnect');

    if (mode === 'connected') {
      dot.className = 'conn-dot connected';
      ip.textContent = info;
      btnConnect.style.display = 'none';
      btnDisconnect.style.display = '';
    } else if (mode === 'connecting') {
      dot.className = 'conn-dot connecting';
      ip.textContent = info;
      btnConnect.style.display = '';
      btnDisconnect.style.display = 'none';
    } else {
      dot.className = 'conn-dot';
      ip.textContent = '未连接';
      btnConnect.style.display = '';
      btnDisconnect.style.display = 'none';
    }
  }

  _updateStateBadge(stateName) {
    const badge = document.getElementById('state-badge');
    if (!badge) return;
    badge.textContent = stateName;
    badge.className = 'state-badge';
    const clsMap = {
      'IDLE': 'idle', 'MOVING': 'moving',
      'E-STOP': 'estop', 'ERROR': 'error', 'LOCKED': 'locked',
    };
    if (clsMap[stateName]) badge.classList.add(clsMap[stateName]);
  }

  _updateRTAngles(joints) {
    for (let i = 0; i < 6; i++) {
      const rt = document.getElementById(`rt-j${i + 1}`);
      if (rt) rt.textContent = `J${i + 1}:${joints[i].toFixed(1)}°`;
      const sj = document.getElementById(`sj${i + 1}`);
      if (sj) sj.textContent = `J${i + 1}:${joints[i].toFixed(1)}°`;
    }
    this._updateTCP();
  }

  _updateTCP() {
    const pos = this.arm.getEndEffectorPosition();
    const set = (id, v) => { const el = document.getElementById(id); if (el) el.textContent = v.toFixed(1); };
    set('tcp-x', pos.x); set('tcp-y', pos.y); set('tcp-z', pos.z);
    // 姿态角暂未实现，留占位
    set('tcp-rx', 0); set('tcp-ry', 0); set('tcp-rz', 0);
    const stcp = document.getElementById('stcp');
    if (stcp) stcp.textContent = `TCP:${pos.x.toFixed(0)},${pos.y.toFixed(0)},${pos.z.toFixed(0)}`;
  }

  _updateArm(index, value) {
    const angles = this.arm.getJointAngles();
    angles[index] = value;
    this.arm.setJointAngles(angles);
    this._updateTCP();
  }

  _sendServo() {
    const joints = this.arm.getJointAngles();
    this.ws.send({ cmd: 'servo', joints: joints });
    this._log(`→ servo ${joints.map(j => j.toFixed(1)).join(' ')}`);
  }

  setJointValues(angles) {
    for (let i = 0; i < 6; i++) {
      if (this.sliders[i]) this.sliders[i].value = angles[i];
      const valEl = document.getElementById(`jval-${i}`);
      if (valEl) valEl.textContent = angles[i].toFixed(1) + '°';
    }
    this.arm.setJointAngles(angles);
    this._updateTCP();
  }

  _setPresets(presets) {
    this.presets = presets;
    const grid = document.getElementById('preset-grid');
    if (!grid) return;
    grid.innerHTML = '';
    for (const [name, joints] of Object.entries(presets)) {
      const btn = document.createElement('button');
      btn.className = 'preset-btn';
      btn.textContent = name;
      btn.title = joints.map(j => j.toFixed(1)).join(', ');
      btn.addEventListener('click', (e) => {
        e.stopPropagation();
        this.setJointValues(joints);
        this.ws.send({ cmd: 'preset', name: name });
        this._log(`→ ${name}`);
      });
      grid.appendChild(btn);
    }
  }

  _log(text) {
    const area = document.getElementById('log-area');
    if (!area) return;
    const time = new Date().toLocaleTimeString('zh-CN', { hour12: false });
    const line = document.createElement('div');
    line.className = 'log-line';
    line.textContent = `[${time}] ${text}`;
    area.appendChild(line);
    area.scrollTop = area.scrollHeight;
    while (area.children.length > 100) area.removeChild(area.firstChild);
  }

  bindViewControls(sceneManager) {
    const bind = (id, fn) => {
      const el = document.getElementById(id);
      if (el) el.addEventListener('click', () => fn());
    };

    bind('btn-tool-grid', () => {
      const show = sceneManager.toggleGrid();
      document.getElementById('btn-tool-grid').classList.toggle('active', show);
    });

    bind('btn-tool-axes', () => {
      const show = sceneManager.toggleAxes();
      document.getElementById('btn-tool-axes').classList.toggle('active', show);
    });

    bind('btn-tool-reset', () => sceneManager.resetView());

    bind('btn-tool-floor', () => {
      this.arm.setInstallMode('floor');
      document.getElementById('btn-tool-floor').classList.add('active');
      document.getElementById('btn-tool-wall').classList.remove('active');
    });

    bind('btn-tool-wall', () => {
      this.arm.setInstallMode('wall');
      document.getElementById('btn-tool-wall').classList.add('active');
      document.getElementById('btn-tool-floor').classList.remove('active');
    });

    // 复位按钮
    document.getElementById('btn-reset').addEventListener('click', () => {
      this.ws.send({ cmd: 'reset' });
      this._log('→ 复位');
    });
  }
}


// === App Entry ===
function startApp() {
  console.log('[App] ThirdHand Web Control starting...');

  const ws = new WSClient();
  const viewport = document.getElementById('three-container');
  const scene = new SceneManager(viewport);
  const arm = new ArmModel(scene.scene);
  const ui = new UIControls(ws, arm);

  // 主题切换
  const themeSelect = document.getElementById('theme-select');
  if (themeSelect) {
    const saved = localStorage.getItem('theme') || 'deepspace';
    if (saved !== 'deepspace') {
      document.documentElement.setAttribute('data-theme', saved);
    }
    themeSelect.value = saved;
    themeSelect.addEventListener('change', () => {
      const v = themeSelect.value;
      if (v === 'deepspace') document.documentElement.removeAttribute('data-theme');
      else document.documentElement.setAttribute('data-theme', v);
      localStorage.setItem('theme', v);
      // 延迟更新场景背景，等 CSS 变量应用
      setTimeout(() => scene._updateBgColor(), 50);
    });
  }

  arm.loadingPromise.then(() => {
    ui.build();
    ui.bindViewControls(scene);
    ws.connect();
    // 默认姿态
    arm.setJointAngles([0, -90, 90, -90, -90, 0]);
    ui.setJointValues([0, -90, 90, -90, -90, 0]);
    console.log('[App] Ready.');
  }).catch(e => {
    console.error('[App] Model load failed:', e);
  });
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', startApp);
} else {
  startApp();
}
