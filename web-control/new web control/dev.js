'use strict';

const { spawn } = require('child_process');
const path = require('path');

const children = [
  spawn(process.execPath, [path.join(__dirname, 'server.js')], {
    cwd: __dirname,
    stdio: 'inherit'
  }),
  spawn(process.execPath, [path.join(__dirname, 'voice-mock.js')], {
    cwd: __dirname,
    stdio: 'inherit'
  })
];

let stopping = false;

function stop(exitCode = 0) {
  if (stopping) return;
  stopping = true;
  for (const child of children) {
    if (!child.killed) child.kill('SIGINT');
  }
  setTimeout(() => process.exit(exitCode), 250);
}

for (const child of children) {
  child.on('exit', code => {
    if (!stopping && code !== 0) stop(code || 1);
  });
}

process.on('SIGINT', () => stop(0));
process.on('SIGTERM', () => stop(0));
