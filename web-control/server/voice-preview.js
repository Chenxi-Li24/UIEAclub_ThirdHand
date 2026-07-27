'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');

const HOST = process.env.WEB_HOST || '127.0.0.1';
const PORT = Number(process.env.WEB_PORT || 3000);
const WEB_ROOT = path.resolve(__dirname, '..', 'web');
const MODEL_ROOT = path.resolve(__dirname, '..', '..', '3d-models', 'models');

const MIME_TYPES = {
  '.css': 'text/css; charset=utf-8',
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.stl': 'model/stl',
  '.urdf': 'application/xml; charset=utf-8',
  '.xml': 'application/xml; charset=utf-8'
};

function resolveRequestPath(requestUrl) {
  const url = new URL(requestUrl, `http://${HOST}:${PORT}`);
  const pathname = decodeURIComponent(url.pathname === '/' ? '/index.html' : url.pathname);
  const root = pathname.startsWith('/models/') ? MODEL_ROOT : WEB_ROOT;
  const relativePath = pathname.startsWith('/models/')
    ? pathname.slice('/models'.length)
    : pathname;
  const candidate = path.resolve(root, `.${relativePath}`);
  return candidate === root || candidate.startsWith(`${root}${path.sep}`) ? candidate : null;
}

const server = http.createServer((req, res) => {
  if (req.method !== 'GET' && req.method !== 'HEAD') {
    res.writeHead(405, { Allow: 'GET, HEAD' });
    res.end('Method Not Allowed');
    return;
  }

  const filePath = resolveRequestPath(req.url);
  if (!filePath) {
    res.writeHead(403);
    res.end('Forbidden');
    return;
  }

  fs.stat(filePath, (statError, stats) => {
    if (statError || !stats.isFile()) {
      res.writeHead(404);
      res.end('Not Found');
      return;
    }

    const contentType = MIME_TYPES[path.extname(filePath).toLowerCase()] || 'application/octet-stream';
    res.writeHead(200, {
      'Content-Type': contentType,
      'Cache-Control': 'no-store',
      'X-Content-Type-Options': 'nosniff'
    });

    if (req.method === 'HEAD') {
      res.end();
      return;
    }

    fs.createReadStream(filePath)
      .on('error', () => {
        if (!res.headersSent) res.writeHead(500);
        res.end('Internal Server Error');
      })
      .pipe(res);
  });
});

server.listen(PORT, HOST, () => {
  console.log('');
  console.log('ThirdHand safe local preview');
  console.log(`  Page:  http://${HOST}:${PORT}/`);
  console.log('  Robot: disabled (this server has no hardware transport)');
  console.log('');
});

process.on('SIGINT', () => server.close(() => process.exit(0)));
