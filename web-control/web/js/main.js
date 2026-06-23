console.log('[main.js] Loading modules...');
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
console.log('[main.js] Modules imported, THREE keys:', Object.keys(THREE).length);

// === scene.js ===
/**
 * Scene — Three.js 场景管理
 * 创建场景、相机、灯光、轨道控制、辅助元素
 */
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

    // 场景
    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x0d1117);
    this.scene.fog = new THREE.Fog(0x0d1117, 800, 3000);

    // 相机
    this.camera = new THREE.PerspectiveCamera(50, w / h, 1, 5000);
    this.camera.position.set(600, 400, 600);
    this.camera.lookAt(0, 200, 0);

    // 渲染器
    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.renderer.setSize(w, h);
    this.renderer.setPixelRatio(window.devicePixelRatio);
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    this.container.appendChild(this.renderer.domElement);

    // 轨道控制
    if (OrbitControls) {
      this.controls = new OrbitControls(this.camera, this.renderer.domElement);
      this.controls.enableDamping = true;
      this.controls.dampingFactor = 0.08;
      this.controls.target.set(0, 250, 0);
      this.controls.minDistance = 200;
      this.controls.maxDistance = 2000;
    }

    // 灯光 - 加强照明确保模型可见
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

    // 网格地面
    this.grid = new THREE.GridHelper(1200, 24, 0x2d3a5c, 0x1a2340);
    this.scene.add(this.grid);

    // 坐标轴
    this.axes = new THREE.AxesHelper(250);
    this.scene.add(this.axes);

    // 窗口 resize
    window.addEventListener('resize', () => this._onResize());

    // 渲染循环
    this._animate();
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
  }

  toggleAxes() {
    this.showAxes = !this.showAxes;
    this.axes.visible = this.showAxes;
  }

  resetView() {
    this.camera.position.set(600, 400, 600);
    if (this.controls) {
      this.controls.target.set(0, 250, 0);
      this.controls.update();
    }
  }
}


// === arm-model.js ===
/**
 * ArmModel — URDF 模型加载
 * 用 urdf-loader 加载 TCR050 URDF，自动处理关节轴和运动学链
 */
class ArmModel {
  constructor(scene) {
    this.scene = scene;
    this.robot = null;
    this.jointAngles = [0, 0, 0, 0, 0, 0];
    this.jointNames = []; // 从 URDF 解析的关节名
    this.loaded = false;
    this.installMode = 'floor'; // 'floor' | 'wall'
    // 零点偏移: 滑块/控制值=0 时，实际关节角度 = zeroOffset
    this.zeroOffset = [0, 90, -90, 90, 90, 0];
    this.loadingPromise = this._loadURDF();
  }

  _loadURDF() {
    return new Promise((resolve, reject) => {
      const loader = new window.URDFLoader();
      loader.load('models/TCR050/TCR050.urdf', (robot) => {
        this.robot = robot;
        // URDF 用米制，场景用毫米 → 放大 1000 倍
        robot.scale.set(1000, 1000, 1000);
        // URDF Z-up → Three.js Y-up: 绕 X 轴旋转 -90°
        robot.rotation.x = -Math.PI / 2;

        // 统一材质
        robot.traverse(child => {
          if (child.isMesh) {
            child.material = new THREE.MeshStandardMaterial({
              color: 0xcccccc, metalness: 0.2, roughness: 0.6,
              flatShading: false, side: THREE.DoubleSide,
            });
          }
        });

        // 提取关节名 (按 URDF 中的顺序)
        this.jointNames = Object.keys(robot.joints);
        console.log('[ArmModel] URDF joints:', this.jointNames);

        // 解析关节限位
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

  /** 设置关节角度 (控制值)，自动加上零点偏移 */
  setJointAngles(angles) {
    if (!this.robot || !angles) return;
    for (let i = 0; i < this.jointNames.length && i < angles.length; i++) {
      const offset = this.zeroOffset[i] || 0;
      const actual = angles[i] + offset; // 控制值 + 零点偏移 = 实际角度
      const [mn, mx] = this.jointLimits[i] || [-180, 180];
      const clamped = Math.max(mn, Math.min(mx, actual));
      this.robot.joints[this.jointNames[i]].setJointValue(clamped * Math.PI / 180);
    }
    this.jointAngles = angles.slice();
  }

  getJointAngles() { return this.jointAngles.slice(); }

  /** 切换安装方式: 'floor'(地面) | 'wall'(壁挂) */
  setInstallMode(mode) {
    if (!this.robot) return;
    this.installMode = mode;
    if (mode === 'floor') {
      // 地面安装: Z-up → Y-up, 绕 X 轴 -90°
      this.robot.rotation.set(-Math.PI / 2, 0, 0);
    } else {
      // 壁挂安装: 垂直于地面安装面，绕 Z 轴 +90° (翻转上下)
      this.robot.rotation.set(0, 0, Math.PI / 2);
    }
    console.log('[ArmModel] Install mode:', mode);
  }

  /** 回零: 所有关节控制值归零 (实际角度 = zeroOffset) */
  goHome() {
    this.setJointAngles(new Array(this.jointNames.length).fill(0));
    console.log('[ArmModel] Home: all zeros');
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


// === ws-client.js ===
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


// === ui-controls.js ===
/**
 * UIControls — 关节控制面板
 * 6 个关节滑块 + 数值输入 + 预设按钮 + 状态显示
 */
class UIControls {
  constructor(armContainer, bottomContainer, wsClient, armModel) {
    this.armContainer = armContainer;
    this.bottomContainer = bottomContainer;
    this.ws = wsClient;
    this.arm = armModel;
    this.sliders = [];
    this.inputs = [];
    this.liveMode = false;
    this.syncMode = false;
    this.presets = {};
    // build 延迟到 URDF 加载完成后调用
  }

  build() {
    this.armContainer.innerHTML = '';
    this.bottomContainer.innerHTML = '';
    this.container = this.bottomContainer; // _addSection 默认加到底部

    // ── 设备连接 (仅 WiFi) ──
    this._addSection('设备连接', `
      <div class="conn-panel">
        <div class="conn-row">
          <label>ESP32 IP</label>
          <input type="text" id="wifi-host" class="conn-input" value="192.168.58.100">
        </div>
        <div class="conn-row">
          <label>UDP 端口</label>
          <input type="number" id="wifi-port" class="conn-input" value="20008">
        </div>
        <button class="btn btn-primary" id="btn-connect-wifi">连接</button>
        <button class="btn btn-danger" id="btn-disconnect-wifi" style="display:none">断开</button>
      </div>
      <div id="conn-status" class="conn-status-bar">
        <span class="status disconnected" id="wifi-status">● 未连接</span>
      </div>
    `);

    // 绑定连接面板事件
    this._bindConnectionPanel();


    // 关节控制
    const jointSection = document.createElement('div');
    jointSection.className = 'panel-section';
    jointSection.innerHTML = '<h3>关节控制</h3>';

    const jointNames = this.arm.jointNames || [];
    const limits = this.arm.jointLimits || [];
    const count = jointNames.length;

    for (let i = 0; i < count; i++) {
      const row = document.createElement('div');
      row.className = 'joint-control-row';
      row.innerHTML = `
        <label>${jointNames[i]}</label>
        <div class="slider-group">
          <input type="range" class="joint-slider" id="slider-j${i}"
                 min="${limits[i][0]}" max="${limits[i][1]}" step="0.1" value="0">
          <input type="number" class="joint-input" id="input-j${i}"
                 min="${limits[i][0]}" max="${limits[i][1]}" step="0.1" value="0">
          <span class="joint-unit">°</span>
        </div>
      `;
      jointSection.appendChild(row);

      const slider = row.querySelector('.joint-slider');
      const input = row.querySelector('.joint-input');
      this.sliders.push(slider);
      this.inputs.push(input);

      slider.addEventListener('input', () => {
        const val = parseFloat(slider.value);
        input.value = val.toFixed(1);
        this._updateArm(i, val);
        if (this.liveMode) this._sendServo();
      });

      input.addEventListener('change', () => {
        const val = parseFloat(input.value) || 0;
        slider.value = val;
        this._updateArm(i, val);
        if (this.liveMode) this._sendServo();
      });
    }

    this.armContainer.appendChild(jointSection);

    // 实时关节角度
    this._addArmSection('实时角度', `
      <div class="rt-angles" id="rt-angles">
        <span class="rt-angle" id="rt-j1">J1: --</span>
        <span class="rt-angle" id="rt-j2">J2: --</span>
        <span class="rt-angle" id="rt-j3">J3: --</span>
        <span class="rt-angle" id="rt-j4">J4: --</span>
        <span class="rt-angle" id="rt-j5">J5: --</span>
        <span class="rt-angle" id="rt-j6">J6: --</span>
      </div>
      <div class="toggle-row">
        <label>
          <input type="checkbox" id="chk-sync"> 同步 3D 模型
        </label>
        <span class="hint">实时数据驱动模型</span>
      </div>
    `);

    document.getElementById('chk-sync').addEventListener('change', (e) => {
      this.syncMode = e.target.checked;
    });

    // 操作按钮
// 状态指示    this._addArmSection('状态', `      <div style="display:flex;align-items:center;gap:12px;margin-bottom:8px;">        <span id="state-badge" style="padding:6px 16px;border-radius:8px;font-weight:bold;font-size:16px;background:#1a1a2e;color:#aaa;">DISCONNECTED</span>        <span id="hb-indicator" style="font-size:12px;color:#666;">hb ---</span>      </div>    `);
    this._addArmSection('操作', `
      <div class="btn-group">
        <button class="btn btn-primary" id="btn-send">发送到机械臂</button>
        <button class="btn btn-danger" id="btn-estop" style="font-size:18px;font-weight:bold;padding:10px 24px;">S 急停 (E-STOP)</button>
      </div>
      <div class="btn-group">
        <button class="btn btn-success" id="btn-home">回零</button>
        <button class="btn btn-warning" id="btn-reset" style="display:none;">R 复位 (Reset)</button>
      </div>
      <div class="toggle-row">
        <label>
          <input type="checkbox" id="chk-live"> 实时发送模式
        </label>
        <span class="hint">拖动滑块即时发送指令</span>
      </div>
    `);

    document.getElementById('btn-send').addEventListener('click', () => this._sendServo());
    document.getElementById('btn-estop').addEventListener('click', () => {
      this.ws.send({ cmd: 'estop' });
    });
    document.getElementById('btn-home').addEventListener('click', () => {
      this.arm.goHome();
      this.setJointValues(new Array(this.arm.jointNames.length).fill(0));
      this._log('→ 回零');
    });
    document.getElementById('chk-live').addEventListener('change', (e) => {
      this.liveMode = e.target.checked;
    });

    // 预设位置
    this._addArmSection('预设位置', '<div class="preset-grid" id="preset-grid"></div>');

    // 末端位置
    this._addArmSection('末端位置', `
      <div class="ee-pos" id="ee-pos">
        X: <span id="ee-x">0.00</span> &nbsp;
        Y: <span id="ee-y">0.00</span> &nbsp;
        Z: <span id="ee-z">0.00</span> mm
      </div>
    `);

    // 安装方式
    this._addArmSection('安装方式', `
      <div class="btn-group">
        <button class="btn btn-primary" id="btn-floor">地面安装</button>
        <button class="btn btn-secondary" id="btn-wall">壁挂安装</button>
      </div>
    `);
    document.getElementById('btn-floor').addEventListener('click', () => {
      this.arm.setInstallMode('floor');
      document.getElementById('btn-floor').className = 'btn btn-primary';
      document.getElementById('btn-wall').className = 'btn btn-secondary';
    });
    document.getElementById('btn-wall').addEventListener('click', () => {
      this.arm.setInstallMode('wall');
      document.getElementById('btn-floor').className = 'btn btn-secondary';
      document.getElementById('btn-wall').className = 'btn btn-primary';
    });

    // 视图控制
    this._addArmSection('视图', `
      <div class="btn-group">
        <button class="btn btn-secondary" id="btn-grid">切换网格</button>
        <button class="btn btn-secondary" id="btn-axes">切换坐标轴</button>
        <button class="btn btn-secondary" id="btn-reset">重置视角</button>
      </div>
    `);

    // 通信日志（底部栏）
    this._addBottomSection('通信日志', '<div class="log-area" id="log-area"></div>', false);

    // WebSocket 事件监听
    this.ws.on('connection', (data) => {
      // header 中的 WS 状态
      const wsEl = document.getElementById('ws-status');
      if (wsEl) {
        wsEl.className = `status ${data.connected ? 'connected' : 'disconnected'}`;
        wsEl.textContent = data.connected ? '● 已连接' : '● 未连接';
      }
      // 设备连接状态
      if (data.mode) this._updateConnection(data);
    });
    this.ws.on('config', (data) => {
      console.log('[UI] config received, presets:', data.presets ? Object.keys(data.presets) : 'none');
      if (data.presets) this._setPresets(data.presets);
      if (data.connection) this._updateConnection(data.connection);
    });
    this.ws.on('state', (data) => {
      if (data.joints) this.setJointValues(data.joints);
    });
    this.ws.on('cnde_state', (data) => {
      // CNDE 回读的是机器人真实关节角，与控制值同坐标系，无需转换
      if (data.joints && data.joints.length >= 6) {
        // 更新实时角度栏（内部按 syncMode 决定是否同步滑块/模型）
        this._updateRTAngles(data.joints);
if (data.stateName && this.stateBadge) {        const colors = { IDLE:'#2ea043', MOVING:'#58a6ff', 'E-STOP':'#da3633', ERROR:'#da3633', LOCKED:'#d29922' };        this.stateBadge.textContent = data.stateName;        this.stateBadge.style.background = colors[data.stateName] || '#1a1a2e';        this.stateBadge.style.color = '#fff';        if (this.btnReset) this.btnReset.style.display = (data.stateName === 'E-STOP' || data.stateName === 'ERROR' || data.stateName === 'LOCKED') ? '' : 'none';      }      if (data.heartbeatAge !== undefined && this.hbIndicator) {        const ok = data.heartbeatAge < 2000;        this.hbIndicator.textContent = ok ? 'hb OK' : 'hb LOST';        this.hbIndicator.style.color = ok ? '#2ea043' : '#da3633';      }
        this._log(`← J: ${data.joints.map(j => j.toFixed(1)).join(' ')}`);
      }
    });
    this.ws.on('esp32_msg', (data) => {
      this._log('← ' + data.raw);
      // 尝试解析关节角度 (格式: "J1,J2,J3,J4,J5,J6")
      const parts = data.raw.split(',').map(Number);
      if (parts.length === 6 && parts.every(n => !isNaN(n))) {
        this._updateRTAngles(parts);
      }
    });
    this.ws.on('error', (data) => this._log('⚠ ' + data.msg));

    // 如果 config 已提前到达，补渲染预设
    if (Object.keys(this.presets).length) {
      console.log('[UI] deferred preset render, count:', Object.keys(this.presets).length);
      this._setPresets(this.presets);
    }
    console.log('[UI] build done, preset-grid:', document.getElementById('preset-grid'));
  }

  _addSection(title, html, collapsible = true) {
    const section = document.createElement('div');
    section.className = 'panel-section';
    section.innerHTML = `<h3>${title}</h3><div class="section-content">${html}</div>`;
    if (collapsible) {
      section.querySelector('h3').addEventListener('click', () => {
        section.classList.toggle('collapsed');
      });
    }
    this.container.appendChild(section);
  }

  _addArmSection(title, html, collapsible = true) {
    const section = document.createElement('div');
    section.className = 'panel-section';
    section.innerHTML = `<h3>${title}</h3><div class="section-content">${html}</div>`;
    if (collapsible) {
      section.querySelector('h3').addEventListener('click', () => {
        section.classList.toggle('collapsed');
      });
    }
    this.armContainer.appendChild(section);
  }

  _addBottomSection(title, html, collapsible = true) {
    const section = document.createElement('div');
    section.className = 'panel-section';
    section.innerHTML = `<h3>${title}</h3><div class="section-content">${html}</div>`;
    if (collapsible) {
      section.querySelector('h3').addEventListener('click', () => {
        section.classList.toggle('collapsed');
      });
    }
    this.bottomContainer.appendChild(section);
  }

  _bindConnectionPanel() {
    // 连接无线
    document.getElementById('btn-connect-wifi').addEventListener('click', () => {
      const host = document.getElementById('wifi-host').value;
      const port = parseInt(document.getElementById('wifi-port').value);
      this.ws.send({ cmd: 'connect', mode: 'wifi', host, port });
      this._log(`→ 连接 ${host}:${port}`);
      this._setConnectingStatus('wifi', `${host}:${port}`);
    });

    // 断开无线
    document.getElementById('btn-disconnect-wifi').addEventListener('click', () => {
      this.ws.send({ cmd: 'disconnect', mode: 'wifi' });
      this._log('→ 断开');
    });
  }

  /** 显示"连接中..."状态 */
  _setConnectingStatus(mode, info) {
    const el = document.getElementById('wifi-status');
    if (!el) return;
    el.className = 'status connecting';
    el.textContent = `● ${info} ⏳`;
  }

  _updateConnection(data) {
    const el = document.getElementById('wifi-status');
    const btnConnect = document.getElementById('btn-connect-wifi');
    const btnDisconnect = document.getElementById('btn-disconnect-wifi');
    if (data.connected) {
      const info = `${data.host}:${data.port}`;
      if (el) { el.className = 'status connected'; el.textContent = `● ${info} ✓`; }
      if (btnConnect) btnConnect.style.display = 'none';
      if (btnDisconnect) btnDisconnect.style.display = '';
    } else {
      if (el) { el.className = 'status disconnected'; el.textContent = '● 未连接'; }
      if (btnConnect) btnConnect.style.display = '';
      if (btnDisconnect) btnDisconnect.style.display = 'none';
      if (data.error) this._log(`⚠ 连接失败: ${data.error}`);
    }
  }

  _updateArm(index, value) {
    const angles = this.arm.getJointAngles();
    angles[index] = value;
    this.arm.setJointAngles(angles);
    this._updateEEPos();
  }

  _updateEEPos() {
    const pos = this.arm.getEndEffectorPosition();
    const el = (id) => document.getElementById(id);
    if (el('ee-x')) el('ee-x').textContent = pos.x.toFixed(1);
    if (el('ee-y')) el('ee-y').textContent = pos.y.toFixed(1);
    if (el('ee-z')) el('ee-z').textContent = pos.z.toFixed(1);
  }

  _updateRTAngles(joints) {
    for (let i = 0; i < 6; i++) {
      const el = document.getElementById(`rt-j${i + 1}`);
      if (el) el.textContent = `J${i + 1}: ${joints[i].toFixed(1)}°`;
    }
    if (this.syncMode) {
      this.setJointValues(joints);
    }
  }

  _sendServo() {
    const joints = this.arm.getJointAngles();
    this.ws.send({ cmd: 'servo', joints: joints });
    this._log(`→ servo ${joints.map(j => j.toFixed(1)).join(' ')}`);
  }

  setJointValues(angles) {
    for (let i = 0; i < 6; i++) {
      if (this.sliders[i]) this.sliders[i].value = angles[i];
      if (this.inputs[i]) this.inputs[i].value = angles[i].toFixed(1);
    }
    this.arm.setJointAngles(angles);
    this._updateEEPos();
  }

  _setPresets(presets) {
    this.presets = presets;
    const grid = document.getElementById('preset-grid');
    if (!grid) { console.warn('[UI] preset-grid not found'); return; }
    grid.innerHTML = '';
    console.log('[UI] rendering presets:', Object.keys(presets));

    for (const [name, joints] of Object.entries(presets)) {
      const btn = document.createElement('button');
      btn.className = 'btn btn-secondary btn-preset';
      btn.textContent = name;
      btn.title = joints.map(j => j.toFixed(1)).join(', ');
      btn.addEventListener('click', () => {
        this.setJointValues(joints);
        this.ws.send({ cmd: 'preset', name: name });
        this._log(`→ preset: ${name}`);
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
    while (area.children.length > 100) {
      area.removeChild(area.firstChild);
    }
  }

  bindViewControls(sceneManager) {
    const bind = (id, fn) => {
      const el = document.getElementById(id);
      if (el) el.addEventListener('click', fn);
    };
    bind('btn-grid', () => sceneManager.toggleGrid());
    bind('btn-axes', () => sceneManager.toggleAxes());
    bind('btn-reset', () => sceneManager.resetView());
  }
}


// === app.js ===
/**
 * App - Main entry point
 */
function startApp() {
  console.log('[App] ThirdHand Web Control starting...');

  const ws = new WSClient();
  const viewport = document.getElementById('three-container');
  const scene = new SceneManager(viewport);
  const arm = new ArmModel(scene.scene);
  const armPanel = document.getElementById('arm-panel');
  const bottomPanel = document.getElementById('bottom-panel');
  const ui = new UIControls(armPanel, bottomPanel, ws, arm);

  // 面板切换按钮
  const btnToggleConn = document.getElementById('btn-toggle-conn');
  const btnToggleArm = document.getElementById('btn-toggle-arm');
  if (btnToggleConn) {
    btnToggleConn.addEventListener('click', () => {
      bottomPanel.classList.toggle('hidden');
      btnToggleConn.classList.toggle('active');
    });
  }
  if (btnToggleArm) {
    btnToggleArm.addEventListener('click', () => {
      armPanel.classList.toggle('hidden');
      btnToggleArm.classList.toggle('active');
    });
  }

  // 主题切换
  const themeSelect = document.getElementById('theme-select');
  if (themeSelect) {
    const saved = localStorage.getItem('theme') || 'dark';
    if (saved !== 'dark') document.documentElement.setAttribute('data-theme', saved);
    themeSelect.value = saved;
    themeSelect.addEventListener('change', () => {
      const v = themeSelect.value;
      if (v === 'dark') document.documentElement.removeAttribute('data-theme');
      else document.documentElement.setAttribute('data-theme', v);
      localStorage.setItem('theme', v);
    });
  }

  // 等待 URDF 模型加载完成后再构建 UI
  arm.loadingPromise.then(() => {
    ui.build();
    ui.bindViewControls(scene);
    ws.connect();
    // 默认姿态: 控制值 [0, -90, 90, -90, -90, 0]
    arm.setJointAngles([0, -90, 90, -90, -90, 0]);
    ui.setJointValues([0, -90, 90, -90, -90, 0]);
    console.log('[App] URDF model loaded, Ready.');
  }).catch(e => {
    console.error('[App] Model load failed:', e);
  });
}

// DOM 可能已加载完成 (main.js 是动态加载的)
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', startApp);
} else {
  startApp();
}
