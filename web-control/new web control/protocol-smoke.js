'use strict';

const dgram = require('dgram');
const { WebSocketServer, WebSocket } = require('ws');

const HOST = '127.0.0.1';
const PORT = 3001;
const PATH = '/v1/voice';
const PROTOCOL = 'thirdhand.voice.v1';
const SESSION_ID = 'smoke-v1';
const EXPECTED_EVENTS = [
  'session.ready',
  'transcript.partial',
  'session.processing',
  'transcript.final',
  'intent.candidate',
  'session.completed'
];

const udp = dgram.createSocket('udp4');
let server;
let client;
let finished = false;
const events = [];
const timeout = setTimeout(() => finish(new Error('Timed out after 5 seconds.')), 5000);

function finish(error) {
  if (finished) return;
  finished = true;
  clearTimeout(timeout);
  try { client?.close(); } catch {}
  try { server?.close(); } catch {}
  try { udp.close(); } catch {}

  if (error) {
    console.error(`FAIL ${error.message}`);
    process.exitCode = 1;
  }
}

function message(type, payload = {}) {
  return JSON.stringify({
    v: 1,
    type,
    messageId: `${type}-1`,
    replyTo: null,
    sessionId: SESSION_ID,
    ts: Date.now(),
    payload
  });
}

udp.on('error', finish);
udp.bind(PORT, HOST, () => {
  console.log(`PASS UDP ${PORT} bound alongside the TCP test`);

  server = new WebSocketServer({
    host: HOST,
    port: PORT,
    path: PATH,
    handleProtocols: protocols => protocols.has(PROTOCOL) ? PROTOCOL : false
  });

  server.on('error', finish);
  server.on('connection', socket => {
    let audioBytes = 0;

    socket.on('message', (data, isBinary) => {
      if (isBinary) {
        if (data.subarray(0, 4).toString('ascii') !== 'THV1') {
          finish(new Error('Invalid binary audio header.'));
          return;
        }
        audioBytes += data.length - 12;
        return;
      }

      const incoming = JSON.parse(data.toString());
      if (incoming.type === 'session.start') {
        socket.send(message('session.ready'));
        socket.send(message('transcript.partial', { text: '打开', revision: 1 }));
      }
      if (incoming.type === 'session.stop') {
        if (audioBytes !== 3200) {
          finish(new Error(`Expected 3200 audio bytes, received ${audioBytes}.`));
          return;
        }
        socket.send(message('session.processing'));
        socket.send(message('transcript.final', { text: '打开夹爪', confidence: 0.97 }));
        socket.send(message('intent.candidate', {
          intent: 'gripper.open',
          sourceText: '打开夹爪',
          confidence: 0.95,
          requiresConfirmation: true,
          args: {}
        }));
        socket.send(message('session.completed'));
      }
    });
  });

  server.on('listening', () => {
    client = new WebSocket(`ws://${HOST}:${PORT}${PATH}`, PROTOCOL);
    client.on('error', finish);
    client.on('open', () => {
      client.send(message('session.start', {
        audio: {
          encoding: 'pcm_s16le',
          sampleRate: 16000,
          channels: 1,
          frameMs: 100
        }
      }));

      const audioFrame = Buffer.alloc(12 + 3200);
      audioFrame.write('THV1', 0, 'ascii');
      audioFrame.writeUInt32LE(0, 4);
      audioFrame.writeUInt32LE(0, 8);
      client.send(audioFrame);
      client.send(message('session.stop'));
    });

    client.on('message', data => {
      const incoming = JSON.parse(data.toString());
      events.push(incoming.type);
      console.log(`PASS received ${incoming.type}`);

      if (incoming.type === 'session.completed') {
        const ordered = EXPECTED_EVENTS.every((event, index) => events[index] === event);
        if (!ordered) {
          finish(new Error(`Unexpected event order: ${events.join(', ')}`));
          return;
        }
        console.log('PASS Voice Protocol v1 round trip completed without hardware access');
        finish();
      }
    });
  });
});
