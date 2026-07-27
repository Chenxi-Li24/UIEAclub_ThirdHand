'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');

const EDGE_CANDIDATES = [
  'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
  'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe'
];
const EDGE_PATH = EDGE_CANDIDATES.find(candidate => fs.existsSync(candidate));
const DEBUG_PORT = 9223;
const PAGE_URL = 'http://127.0.0.1:3000/';
const SCREENSHOT_PATH = path.join(__dirname, 'voice-control-preview.png');

if (!EDGE_PATH) {
  console.error('FAIL Microsoft Edge was not found.');
  process.exit(1);
}

const profileDir = fs.mkdtempSync(path.join(os.tmpdir(), 'thirdhand-voice-edge-'));
const safeTempPrefix = `${path.resolve(os.tmpdir())}${path.sep}`.toLowerCase();
let edge;
let cdp;
let commandId = 0;
const pending = new Map();
const exceptions = [];

function delay(milliseconds) {
  return new Promise(resolve => setTimeout(resolve, milliseconds));
}

async function waitForDebugTarget() {
  for (let attempt = 0; attempt < 50; attempt += 1) {
    try {
      const response = await fetch(`http://127.0.0.1:${DEBUG_PORT}/json`);
      const targets = await response.json();
      const page = targets.find(target => target.type === 'page');
      if (page?.webSocketDebuggerUrl) return page;
    } catch {}
    await delay(100);
  }
  throw new Error('Edge DevTools target did not become ready.');
}

function command(method, params = {}) {
  return new Promise((resolve, reject) => {
    const id = ++commandId;
    pending.set(id, { resolve, reject });
    cdp.send(JSON.stringify({ id, method, params }));
  });
}

async function evaluate(expression) {
  const response = await command('Runtime.evaluate', {
    expression,
    awaitPromise: true,
    returnByValue: true
  });
  if (response.exceptionDetails) {
    throw new Error(response.exceptionDetails.exception?.description || 'Runtime evaluation failed.');
  }
  return response.result?.result?.value;
}

async function waitFor(expression, timeoutMs = 6000) {
  const started = Date.now();
  while (Date.now() - started < timeoutMs) {
    if (await evaluate(expression)) return true;
    await delay(100);
  }
  throw new Error(`Condition timed out: ${expression}`);
}

async function run() {
  edge = spawn(EDGE_PATH, [
    '--headless=new',
    '--disable-gpu',
    '--hide-scrollbars',
    '--use-fake-device-for-media-stream',
    '--use-fake-ui-for-media-stream',
    '--autoplay-policy=no-user-gesture-required',
    `--remote-debugging-port=${DEBUG_PORT}`,
    `--user-data-dir=${profileDir}`,
    '--window-size=1440,1000',
    PAGE_URL
  ], {
    windowsHide: true,
    stdio: 'ignore'
  });

  const target = await waitForDebugTarget();
  cdp = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((resolve, reject) => {
    cdp.addEventListener('open', resolve, { once: true });
    cdp.addEventListener('error', reject, { once: true });
  });

  cdp.addEventListener('message', event => {
    const message = JSON.parse(event.data);
    if (message.id && pending.has(message.id)) {
      const request = pending.get(message.id);
      pending.delete(message.id);
      if (message.error) request.reject(new Error(message.error.message));
      else request.resolve(message);
      return;
    }
    if (message.method === 'Runtime.exceptionThrown') {
      exceptions.push(message.params.exceptionDetails.exception?.description || message.params.exceptionDetails.text);
    }
  });

  await command('Runtime.enable');
  await command('Page.enable');
  await command('Page.reload', { ignoreCache: true });
  await waitFor(`document.readyState === 'complete'`);
  await waitFor(`document.getElementById('voice-ai-status')?.textContent === 'AI 已连接'`);

  await evaluate(`document.getElementById('btn-voice-toggle').click()`);
  await waitFor(`document.getElementById('voice-panel').classList.contains('open')`);
  await evaluate(`document.getElementById('voice-mic-toggle').click()`);
  await waitFor(`document.getElementById('voice-mic-status').textContent === '麦克风就绪'`, 10000);

  await evaluate(`document.getElementById('voice-record-toggle').click()`);
  await waitFor(`document.getElementById('voice-session-status').textContent === '正在录音'`);
  await delay(900);
  await evaluate(`document.getElementById('voice-record-toggle').click()`);
  await waitFor(`document.getElementById('voice-intent-card').hidden === false`, 8000);

  const result = await evaluate(`(() => ({
    panelOpen: document.getElementById('voice-panel').classList.contains('open'),
    aiStatus: document.getElementById('voice-ai-status').textContent,
    micStatus: document.getElementById('voice-mic-status').textContent,
    deviceCount: document.getElementById('voice-device-select').options.length,
    sessionStatus: document.getElementById('voice-session-status').textContent,
    intent: document.getElementById('voice-intent-name').textContent,
    conversation: document.getElementById('voice-conversation').textContent,
    canvasCount: document.querySelectorAll('#three-container canvas').length
  }))()`);

  const screenshot = await command('Page.captureScreenshot', {
    format: 'png',
    captureBeyondViewport: false
  });
  fs.writeFileSync(SCREENSHOT_PATH, Buffer.from(screenshot.result.data, 'base64'));

  await evaluate(`document.getElementById('voice-intent-confirm').click()`);
  await waitFor(`document.getElementById('voice-intent-card').hidden === true`);
  const simulationRendered = await evaluate(
    `document.getElementById('voice-conversation').textContent.includes('模拟执行：打开夹爪')`
  );

  const checks = {
    panelOpen: result.panelOpen === true,
    aiConnected: result.aiStatus === 'AI 已连接',
    microphoneReady: result.micStatus === '麦克风就绪',
    microphoneEnumerated: result.deviceCount > 0,
    awaitingConfirmation: result.sessionStatus === '等待确认',
    candidateRendered: result.intent === '打开夹爪',
    transcriptRendered: result.conversation.includes('打开夹爪'),
    confirmationSimulated: simulationRendered === true,
    modelCanvasCreated: result.canvasCount === 1,
    noRuntimeExceptions: exceptions.length === 0
  };

  for (const [name, passed] of Object.entries(checks)) {
    console.log(`${passed ? 'PASS' : 'FAIL'} ${name}`);
  }
  console.log(`SCREENSHOT ${SCREENSHOT_PATH}`);

  if (Object.values(checks).some(passed => !passed)) {
    if (exceptions.length) console.error(exceptions.join('\n'));
    process.exitCode = 1;
  }

  try { await command('Browser.close'); } catch {}
}

run()
  .catch(error => {
    console.error(`FAIL ${error.message}`);
    process.exitCode = 1;
  })
  .finally(async () => {
    try { cdp?.close(); } catch {}
    if (edge && edge.exitCode === null) edge.kill();
    await delay(300);

    const resolvedProfile = path.resolve(profileDir).toLowerCase();
    if (resolvedProfile.startsWith(safeTempPrefix)) {
      try { fs.rmSync(profileDir, { recursive: true, force: true }); } catch {}
    }
  });
