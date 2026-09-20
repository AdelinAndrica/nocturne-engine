import { readdir, readFile, writeFile, stat } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const websiteRoot = path.resolve(scriptDir, '..');
const distRoot = path.join(websiteRoot, 'dist');
const rawSite = process.env.NOCTURNE_SITE_URL?.trim();

async function walk(directory) {
  const entries = await readdir(directory, { withFileTypes: true });
  const files = [];

  for (const entry of entries) {
    const full = path.join(directory, entry.name);
    if (entry.isDirectory()) files.push(...await walk(full));
    else if (entry.isFile()) files.push(full);
  }

  return files;
}

function toPosix(value) {
  return value.split(path.sep).join('/');
}

function xmlEscape(value) {
  return value
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&apos;');
}

function validateSiteUrl(raw) {
  const url = new URL(raw);

  if (url.protocol !== 'https:') {
    throw new Error('NOCTURNE_SITE_URL must use https.');
  }
  if (url.pathname !== '/' || url.search || url.hash) {
    throw new Error(
      'NOCTURNE_SITE_URL must be a root-hosted origin with no path, query or fragment.'
    );
  }

  return url;
}

if (!rawSite) {
  console.log('NOCTURNE_SITE_URL not set; sitemap/robots generation skipped.');
  process.exit(0);
}

const site = validateSiteUrl(rawSite);
const files = await walk(distRoot);
const routes = files
  .map((file) => toPosix(path.relative(distRoot, file)))
  .filter((relative) => relative.endsWith('.html'))
  .filter((relative) => relative !== '404.html')
  .filter((relative) => !relative.startsWith('api/'))
  .filter((relative) => !relative.startsWith('api-symbol/'))
  .filter((relative) => !relative.startsWith('pagefind/'))
  .map((relative) => {
    if (relative === 'index.html') return '/';
    if (relative.endsWith('/index.html')) {
      return '/' + relative.slice(0, -'index.html'.length);
    }
    return '/' + relative;
  })
  .sort((a, b) => a.localeCompare(b, 'en'));

const uniqueRoutes = [...new Set(routes)];
const urlset = [
  '<?xml version="1.0" encoding="UTF-8"?>',
  '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">',
  ...uniqueRoutes.map((route) => {
    const absolute = new URL(route, site).href;
    return `  <url><loc>${xmlEscape(absolute)}</loc></url>`;
  }),
  '</urlset>',
  ''
].join('\n');

await writeFile(path.join(distRoot, 'sitemap.xml'), urlset, 'utf8');
await writeFile(
  path.join(distRoot, 'robots.txt'),
  [
    'User-agent: *',
    'Allow: /',
    `Sitemap: ${new URL('/sitemap.xml', site).href}`,
    ''
  ].join('\n'),
  'utf8'
);

const sitemapBytes = (await stat(path.join(distRoot, 'sitemap.xml'))).size;
console.log(
  `Generated sitemap.xml with ${uniqueRoutes.length} route(s) (${sitemapBytes} bytes) for ${site.origin}.`
);
