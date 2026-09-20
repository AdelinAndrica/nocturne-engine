import http from 'node:http';
import { readFile, stat } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const websiteRoot = path.resolve(scriptDir, '..');
const distRoot = path.join(websiteRoot, 'dist');
const host = process.env.NOCTURNE_PREVIEW_HOST || '127.0.0.1';
const port = Number(process.env.NOCTURNE_PREVIEW_PORT || '4322');

const mime = new Map([
  ['.html', 'text/html; charset=utf-8'],
  ['.css', 'text/css; charset=utf-8'],
  ['.js', 'text/javascript; charset=utf-8'],
  ['.json', 'application/json; charset=utf-8'],
  ['.xml', 'application/xml; charset=utf-8'],
  ['.txt', 'text/plain; charset=utf-8'],
  ['.svg', 'image/svg+xml'],
  ['.png', 'image/png'],
  ['.jpg', 'image/jpeg'],
  ['.jpeg', 'image/jpeg'],
  ['.webp', 'image/webp'],
  ['.woff', 'font/woff'],
  ['.woff2', 'font/woff2'],
  ['.tag', 'application/xml; charset=utf-8']
]);

async function resolveFile(pathname) {
  let relative = decodeURIComponent(pathname).replace(/^\/+/, '');
  if (!relative || relative.endsWith('/')) relative += 'index.html';

  let candidate = path.resolve(distRoot, relative);
  if (!candidate.startsWith(distRoot)) return null;

  try {
    const info = await stat(candidate);
    if (info.isDirectory()) candidate = path.join(candidate, 'index.html');
    else if (!info.isFile()) return null;
    await stat(candidate);
    return candidate;
  } catch {
    if (!path.extname(relative)) {
      const indexCandidate = path.resolve(distRoot, relative, 'index.html');
      if (!indexCandidate.startsWith(distRoot)) return null;
      try {
        if ((await stat(indexCandidate)).isFile()) return indexCandidate;
      } catch {}
    }
    return null;
  }
}

const server = http.createServer(async (request, response) => {
  try {
    const url = new URL(request.url || '/', `http://${host}:${port}`);
    let file = await resolveFile(url.pathname);
    let status = 200;

    if (!file) {
      file = path.join(distRoot, '404.html');
      status = 404;
    }

    const body = await readFile(file);
    response.writeHead(status, {
      'Content-Type': mime.get(path.extname(file).toLowerCase()) || 'application/octet-stream',
      'Cache-Control': 'no-store'
    });

    if (request.method === 'HEAD') response.end();
    else response.end(body);
  } catch (error) {
    response.writeHead(500, { 'Content-Type': 'text/plain; charset=utf-8' });
    response.end(String(error));
  }
});

server.listen(port, host, () => {
  console.log(`Nocturne static preview listening at http://${host}:${port}`);
});
