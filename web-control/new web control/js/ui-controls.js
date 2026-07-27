/**
 * UIControls — 关节控制面板
 * 6 个关节滑块 + 数值输入 + 预设按钮 + 状态显示
 */
class UIControls {
  constructor(container, wsClient, armModel) {
    this.container = container;
    this.ws = wsClient;
    this.arm = armModel;
    this.sliders = [];
    this.inputs = [];
    this.liveMode = false;
    this.presets = {};
    this._build();
  }

  _build() {
    this.container.innerHTML = '';

    // 连接状态
    this._addSection('连接状态', `
      <div id="conn-status">
        <span class="status disconnected" id="ws-status">● 未连接</span>
        <span class="status" id="esp32-status">ESP32: --</span>
      </div>
    `);

    // 关节控制
    const jointSection = document.createElement('div');
    jointSection.className = 'panel-section';
    jointSection.innerHTML = '<h3>关节控制</h3>';

    const jointNames = ['J1 (底座)', 'J2 (肩)', 'J3 (肘)', 'J4 (腕旋转)', 'J5 (腕摆动)', 'J6 (末端)'];
    const limits = this.arm.jointLimits;

    for (let i = 0; i < 6; i++) {
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

    this.container.appendChild(jointSection);

    // 操作按钮
    this._addSection('操作', `
      <div class="btn-group">
        <button class="btn btn-primary" id="btn-send">发送到机械臂</button>
        <button class="btn btn-danger" id="btn-estop">急停 (E-Stop)</button>
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
    document.getElementById('chk-live').addEventListener('change', (e) => {
      this.liveMode = e.target.checked;
    });

    // 预设位置
    const presetSection = document.createElement('div');
    presetSection.className = 'panel-section';
    presetSection.innerHTML = '<h3>预设位置</h3>';
    const presetGrid = document.createElement('div');
    presetGrid.className = 'preset-grid';
    presetGrid.id = 'preset-grid';
    presetSection.appendChild(presetGrid);
    this.container.appendChild(presetSection);

    // 末端位置
    this._addSection('末端位置', `
      <div class="ee-pos" id="ee-pos">
        X: <span id="ee-x">0.00</span> &nbsp;
        Y: <span id="ee-y">0.00</span> &nbsp;
        Z: <span id="ee-z">0.00</span> mm
      </div>
    `);

    // 视图控制
    this._addSection('视图', `
      <div class="btn-group">
        <button class="btn btn-secondary" id="btn-grid">切换网格</button>
        <button class="btn btn-secondary" id="btn-axes">切换坐标轴</button>
        <button class="btn btn-secondary" id="btn-reset">重置视角</button>
      </div>
    `);

    // 通信日志
    this._addSection('通信日志', `
      <div class="log-area" id="log-area"></div>
    `);

    // WebSocket 事件监听
    this.ws.on('connection', (data) => this._updateConnection(data.connected));
    this.ws.on('config', (data) => {
      if (data.presets) this._setPresets(data.presets);
    });
    this.ws.on('state', (data) => {
      if (data.joints) this.setJointValues(data.joints);
    });
    this.ws.on('esp32_msg', (data) => this._log('← ' + data.raw));
    this.ws.on('error', (data) => this._log('⚠ ' + data.msg));
  }

  _addSection(title, html) {
    const section = document.createElement('div');
    section.className = 'panel-section';
    section.innerHTML = `<h3>${title}</h3>${html}`;
    this.container.appendChild(section);
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
    if (!grid) return;
    grid.innerHTML = '';

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

  _updateConnection(connected) {
    const el = document.getElementById('ws-status');
    if (el) {
      el.className = `status ${connected ? 'connected' : 'disconnected'}`;
      el.textContent = `● ${connected ? '已连接' : '未连接'}`;
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
