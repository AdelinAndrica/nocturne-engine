import { promises as fs } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const websiteRoot = path.resolve(scriptDir, '..');
const repoRoot = path.resolve(websiteRoot, '..');
const docsRoot = path.join(repoRoot, 'Docs');
const contentRoot = path.join(websiteRoot, 'src', 'content', 'docs');
const themePath = path.join(repoRoot, 'Design', 'nocturne-theme.json');
const generatedThemeDir = path.join(websiteRoot, 'src', 'styles', 'generated');
const generatedThemePath = path.join(generatedThemeDir, 'nocturne-theme.css');
const manifestPath = path.join(websiteRoot, '.generated-docs.json');

function toPosix(value) {
  return value.split(path.sep).join('/');
}

function slugify(value) {
  return value
    .normalize('NFKD')
    .replace(/[\u0300-\u036f]/g, '')
    .replace(/[^a-zA-Z0-9]+/g, '-')
    .replace(/^-+|-+$/g, '')
    .toLowerCase();
}

function yamlString(value) {
  return JSON.stringify(value);
}

function stripFrontmatter(markdown) {
  const match = markdown.match(/^---\r?\n[\s\S]*?\r?\n---\r?\n?/);
  return match ? markdown.slice(match[0].length) : markdown;
}

function extractTitle(markdown, fallback) {
  const match = markdown.match(/^#\s+(.+)$/m);
  return match ? match[1].trim() : fallback;
}

function removeFirstH1(markdown) {
  return markdown.replace(/^#\s+.+\r?\n(?:\r?\n)?/m, '');
}

async function walkMarkdown(directory) {
  const result = [];
  const entries = (await fs.readdir(directory, { withFileTypes: true }))
    .sort((a, b) => a.name.localeCompare(b.name, 'en'));

  for (const entry of entries) {
    const absolute = path.join(directory, entry.name);
    if (entry.isDirectory()) {
      result.push(...await walkMarkdown(absolute));
      continue;
    }

    if (entry.isFile() && entry.name.toLowerCase().endsWith('.md')) {
      result.push(absolute);
    }
  }

  return result;
}

function mapDoc(relativeSource) {
  const normalized = toPosix(relativeSource);

  if (normalized === 'nocturne_engine_architecture.md') {
    return 'docs/architecture/overview.md';
  }

  if (normalized === 'Production Engineering Standard.md') {
    return 'docs/development/production-engineering-standard.md';
  }

  if (normalized === 'Web/Nocturne Website — Design & Technical Specification.md') {
    return 'docs/development/website-specification.md';
  }

  const basename = path.basename(normalized, path.extname(normalized));
  if (/^combined/i.test(basename)) return null;

  return `docs/history/${slugify(basename)}.md`;
}

async function cleanPreviousGeneratedDocs() {
  try {
    const raw = await fs.readFile(manifestPath, 'utf8');
    const previous = JSON.parse(raw);

    for (const relative of previous.files ?? []) {
      const absolute = path.resolve(contentRoot, relative);
      if (!absolute.startsWith(contentRoot + path.sep)) {
        throw new Error(`Refusing to delete path outside content root: ${absolute}`);
      }
      await fs.rm(absolute, { force: true });
    }
  } catch (error) {
    if (error?.code !== 'ENOENT') throw error;
  }
}

async function syncDocs() {
  await cleanPreviousGeneratedDocs();

  const sourceFiles = await walkMarkdown(docsRoot);
  const generated = [];
  const claimedTargets = new Map();
  const historicalPages = [];

  for (const source of sourceFiles) {
    const relativeSource = path.relative(docsRoot, source);
    const relativeTarget = mapDoc(relativeSource);
    if (!relativeTarget) continue;

    const existing = claimedTargets.get(relativeTarget);
    if (existing) {
      throw new Error(
        `Generated documentation collision: ${existing} and ${toPosix(relativeSource)} both map to ${relativeTarget}`
      );
    }
    claimedTargets.set(relativeTarget, toPosix(relativeSource));

    const target = path.join(contentRoot, relativeTarget);
    const raw = await fs.readFile(source, 'utf8');
    const withoutFrontmatter = stripFrontmatter(raw);
    const fallback = path.basename(source, '.md');
    const title = extractTitle(withoutFrontmatter, fallback);
    const body = removeFirstH1(withoutFrontmatter).trimStart();
    const historical = relativeTarget.startsWith('docs/history/');

    const frontmatter = [
      '---',
      `title: ${yamlString(title)}`,
      `description: ${yamlString(`Synchronized from Docs/${toPosix(relativeSource)}`)}`,
      historical ? 'badge:' : null,
      historical ? '  text: Historical' : null,
      historical ? '  variant: default' : null,
      '---',
      ''
    ].filter(Boolean).join('\n');

    const sourceNote = historical
      ? '> Historical implementation record. For current behavior, prefer canonical Architecture and System documentation.\n\n'
      : '';

    await fs.mkdir(path.dirname(target), { recursive: true });
    await fs.writeFile(target, frontmatter + sourceNote + body + '\n', 'utf8');
    generated.push(toPosix(path.relative(contentRoot, target)));

    if (historical) {
      historicalPages.push({
        title,
        slug: path.basename(relativeTarget, '.md')
      });
    }
  }

  historicalPages.sort((a, b) => a.title.localeCompare(b.title, 'en'));

  const historyIndexRelative = 'docs/history/index.md';
  const historyIndex = path.join(contentRoot, historyIndexRelative);
  const historyBody = [
    '---',
    'title: Development History',
    'description: Historical phase and implementation documents synchronized from the Nocturne repository.',
    'tableOfContents: false',
    '---',
    '',
    'These pages preserve the implementation history of Nocturne Engine.',
    '',
    '> Historical documents do not override current canonical Architecture/System documentation.',
    '',
    '## Synchronized documents',
    '',
    ...historicalPages.map((page) => `- [${page.title}](/docs/history/${page.slug}/)`),
    ''
  ].join('\n');

  await fs.mkdir(path.dirname(historyIndex), { recursive: true });
  await fs.writeFile(historyIndex, historyBody, 'utf8');
  generated.push(historyIndexRelative);

  await fs.writeFile(
    manifestPath,
    JSON.stringify({ schemaVersion: 1, files: generated.sort() }, null, 2) + '\n',
    'utf8'
  );

  return generated.length;
}

function cssList(values) {
  return values
    .map((value) => value.includes(' ') ? `"${value}"` : value)
    .join(', ');
}

async function generateTheme() {
  const theme = JSON.parse(await fs.readFile(themePath, 'utf8'));
  const c = theme.colors;
  const m = theme.metrics;

  const css = `/* GENERATED from Design/nocturne-theme.json. Do not edit by hand. */
:root {
  --noc-window-bg: ${c.windowBg};
  --noc-panel-bg: ${c.panelBg};
  --noc-panel-bg-alt: ${c.panelBgAlt};
  --noc-viewport-bg: ${c.viewportBg};
  --noc-toolbar-bg: ${c.toolbarBg};
  --noc-input-bg: ${c.inputBg};
  --noc-button-bg: ${c.buttonBg};
  --noc-button-hover: ${c.buttonHover};
  --noc-border: ${c.border};
  --noc-text-primary: ${c.textPrimary};
  --noc-text-muted: ${c.textMuted};
  --noc-accent: ${c.accent};
  --noc-accent-hover: ${c.accentHover};
  --noc-success: ${c.success};
  --noc-warning: ${c.warning};
  --noc-danger: ${c.danger};

  --noc-font-ui: ${cssList(theme.typography.ui)};
  --noc-font-display: ${cssList(theme.typography.display)};
  --noc-font-mono: ${cssList(theme.typography.mono)};

  --noc-menu-height: ${m.menuHeightPx}px;
  --noc-toolbar-height: ${m.toolbarHeightPx}px;
  --noc-status-height: ${m.statusHeightPx}px;
  --noc-panel-gap: ${m.panelGapPx}px;
  --noc-panel-header-height: ${m.panelHeaderHeightPx}px;
  --noc-radius-sm: ${m.radiusSmallPx}px;
  --noc-radius-md: ${m.radiusMediumPx}px;
}
`;

  await fs.mkdir(generatedThemeDir, { recursive: true });
  await fs.writeFile(generatedThemePath, css, 'utf8');
}

async function syncBrandAsset() {
  const source = path.join(
    repoRoot,
    'Apps',
    'NocturneEditor',
    'Resources',
    'NocturneEngine-Logo.png'
  );
  const target = path.join(websiteRoot, 'public', 'nocturne-logo.png');

  try {
    await fs.mkdir(path.dirname(target), { recursive: true });
    await fs.copyFile(source, target);
    return true;
  } catch (error) {
    if (error?.code === 'ENOENT') return false;
    throw error;
  }
}

await generateTheme();
const count = await syncDocs();
const copiedLogo = await syncBrandAsset();

console.log(
  `Prepared Nocturne website: ${count} docs synchronized; logo ${copiedLogo ? 'copied' : 'not found'}.`
);
