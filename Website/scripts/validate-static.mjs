import { readdir, readFile, stat, writeFile, access } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const websiteRoot = path.resolve(scriptDir, '..');
const distRoot = path.join(websiteRoot, 'dist');
const args = new Set(process.argv.slice(2));
const requireApi = args.has('--require-api');
const requireSitemap = args.has('--require-sitemap');

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

function decodeHtml(value) {
  return value
    .replaceAll('&amp;', '&')
    .replaceAll('&quot;', '"')
    .replaceAll('&#39;', "'")
    .replaceAll('&lt;', '<')
    .replaceAll('&gt;', '>');
}

function safeDecode(value) {
  try {
    return decodeURIComponent(value);
  } catch {
    return value;
  }
}

function isExternalReference(value) {
  return /^(?:[a-z][a-z0-9+.-]*:|\/\/)/i.test(value);
}

const allFiles = await walk(distRoot);
const relativeFiles = allFiles.map((file) => toPosix(path.relative(distRoot, file)));
const fileSet = new Set(relativeFiles);
const htmlFiles = relativeFiles.filter((file) => file.endsWith('.html'));

const requiredFiles = [
  'index.html',
  'download/index.html',
  'docs/index.html',
  'docs/systems/runtime/index.html',
  'docs/systems/editor/index.html',
  'llms.txt',
  'knowledge/manifest.json'
];

if (requireApi) {
  requiredFiles.push(
    'api/index.html',
    'api-xml/index.xml',
    'api-symbols.json',
    'api-symbol/noc.Engine.html'
  );
}

if (requireSitemap) {
  requiredFiles.push('sitemap.xml', 'robots.txt');
}

for (const required of requiredFiles) {
  if (!fileSet.has(required)) {
    throw new Error(`Missing required static output: ${required}`);
  }
}

function resolveTarget(sourceRelative, reference) {
  const decoded = decodeHtml(reference.trim());

  if (!decoded || decoded.startsWith('#') || isExternalReference(decoded)) {
    return null;
  }

  const withoutHash = decoded.split('#', 1)[0];
  const pathname = safeDecode(withoutHash.split('?', 1)[0]);

  if (!pathname) return sourceRelative;

  let relative = pathname.startsWith('/')
    ? pathname.slice(1)
    : path.posix.normalize(path.posix.join(path.posix.dirname(sourceRelative), pathname));

  relative = relative.replace(/^\.\//, '');

  if (!relative || relative === '.') return 'index.html';

  const candidates = [];
  if (relative.endsWith('/')) {
    candidates.push(relative + 'index.html');
  } else {
    candidates.push(relative);
    if (!path.posix.extname(relative)) {
      candidates.push(relative + '/index.html');
      candidates.push(relative + '.html');
    }
  }

  return candidates.find((candidate) => fileSet.has(candidate)) ?? candidates[0];
}

const brokenLinks = [];
let internalReferenceCount = 0;

for (const html of htmlFiles) {
  const content = await readFile(path.join(distRoot, html), 'utf8');
  const attributePattern = /\b(?:href|src)=["']([^"']+)["']/gi;

  for (const match of content.matchAll(attributePattern)) {
    const raw = match[1].trim();
    if (
      !raw ||
      raw.startsWith('#') ||
      /^(?:mailto:|tel:|javascript:|data:|blob:)/i.test(raw) ||
      /^https?:\/\//i.test(raw) ||
      raw.startsWith('//')
    ) {
      continue;
    }

    const target = resolveTarget(html, raw);
    if (!target) continue;

    internalReferenceCount += 1;
    if (!fileSet.has(target)) {
      brokenLinks.push({ source: html, reference: raw, resolved: target });
    }
  }
}

if (brokenLinks.length > 0) {
  const preview = brokenLinks
    .slice(0, 80)
    .map((item) => `${item.source}: ${item.reference} -> ${item.resolved}`)
    .join('\n');
  throw new Error(
    `Broken internal static references: ${brokenLinks.length}\n${preview}`
  );
}

const privacyPatterns = [
  {
    name: 'local Nocturne workspace path',
    regex: /\b[A-Za-z]:[\\/]Projects[\\/]Nocturne\b/gi
  },
  {
    name: 'Windows user profile path',
    regex: /\b[A-Za-z]:[\\/]Users[\\/][A-Za-z0-9._ -]+/g
  },
  {
    name: 'GitHub Windows runner workspace path',
    regex: /\b[A-Za-z]:[\\/]a[\\/]nocturne-engine\b/gi
  },
  {
    name: 'Unix user home path',
    regex: /\/(?:home|Users)\/[A-Za-z0-9._-]+\//g
  },
  {
    name: 'private key material',
    regex: /-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----/g
  },
  {
    name: 'GitHub token',
    regex: /\bgh[pousr]_[A-Za-z0-9]{20,}\b/g
  },
  {
    name: 'AWS access key',
    regex: /\bAKIA[0-9A-Z]{16}\b/g
  }
];

const scanExtensions = new Set([
  '.html', '.xml', '.json', '.txt', '.md', '.js', '.css', '.svg', '.tag'
]);
const privacyFindings = [];

for (const relative of relativeFiles) {
  if (!scanExtensions.has(path.extname(relative).toLowerCase())) continue;
  const content = await readFile(path.join(distRoot, relative), 'utf8');

  for (const rule of privacyPatterns) {
    rule.regex.lastIndex = 0;
    const match = rule.regex.exec(content);
    if (match) {
      privacyFindings.push({
        file: relative,
        rule: rule.name,
        sample: match[0].slice(0, 120)
      });
    }
  }
}

if (privacyFindings.length > 0) {
  const preview = privacyFindings
    .slice(0, 80)
    .map((item) => `${item.file}: ${item.rule}: ${item.sample}`)
    .join('\n');
  throw new Error(
    `Private path/credential material found in static output: ${privacyFindings.length}\n${preview}`
  );
}

const performanceBudgets = {
  maxHtmlBytes: 800 * 1024,
  maxRouteAssetBytes: 2.5 * 1024 * 1024,
  maxRouteJsBytes: 1.5 * 1024 * 1024,
  maxRouteCssBytes: 1.5 * 1024 * 1024,
  maxSingleImageBytes: 2 * 1024 * 1024
};

const keyPages = [
  'index.html',
  'download/index.html',
  'docs/index.html',
  'docs/systems/runtime/index.html',
  'docs/systems/editor/index.html'
];

const performance = {};

for (const pageFile of keyPages) {
  const absolute = path.join(distRoot, pageFile);
  const html = await readFile(absolute, 'utf8');
  const htmlBytes = (await stat(absolute)).size;

  if (htmlBytes > performanceBudgets.maxHtmlBytes) {
    throw new Error(
      `${pageFile} exceeds HTML budget: ${htmlBytes} > ${performanceBudgets.maxHtmlBytes}`
    );
  }

  const assets = new Set();
  for (const match of html.matchAll(/\b(?:href|src)=["']([^"']+)["']/gi)) {
    const target = resolveTarget(pageFile, match[1]);
    if (!target || !fileSet.has(target)) continue;
    const ext = path.extname(target).toLowerCase();
    if (['.js', '.css', '.png', '.jpg', '.jpeg', '.webp', '.svg', '.woff', '.woff2'].includes(ext)) {
      assets.add(target);
    }
  }

  let assetBytes = 0;
  let jsBytes = 0;
  let cssBytes = 0;

  for (const asset of assets) {
    const bytes = (await stat(path.join(distRoot, asset))).size;
    const ext = path.extname(asset).toLowerCase();
    assetBytes += bytes;
    if (ext === '.js') jsBytes += bytes;
    if (ext === '.css') cssBytes += bytes;
    if (['.png', '.jpg', '.jpeg', '.webp'].includes(ext) &&
        bytes > performanceBudgets.maxSingleImageBytes) {
      throw new Error(
        `${pageFile} references oversized image ${asset}: ${bytes} bytes`
      );
    }
  }

  if (assetBytes > performanceBudgets.maxRouteAssetBytes) {
    throw new Error(`${pageFile} exceeds route asset budget: ${assetBytes}`);
  }
  if (jsBytes > performanceBudgets.maxRouteJsBytes) {
    throw new Error(`${pageFile} exceeds JS budget: ${jsBytes}`);
  }
  if (cssBytes > performanceBudgets.maxRouteCssBytes) {
    throw new Error(`${pageFile} exceeds CSS budget: ${cssBytes}`);
  }

  performance[pageFile] = {
    htmlBytes,
    referencedAssetBytes: assetBytes,
    jsBytes,
    cssBytes,
    referencedAssetCount: assets.size
  };
}

const report = {
  schemaVersion: 1,
  htmlFiles: htmlFiles.length,
  staticFiles: relativeFiles.length,
  internalReferencesChecked: internalReferenceCount,
  privacyRulesChecked: privacyPatterns.map((rule) => rule.name),
  performanceBudgets,
  performance
};

await writeFile(
  path.join(distRoot, 'quality-report.json'),
  JSON.stringify(report, null, 2) + '\n',
  'utf8'
);

console.log(
  `Static quality passed: ${htmlFiles.length} HTML files, ${internalReferenceCount} internal references, no private path/credential findings.`
);
