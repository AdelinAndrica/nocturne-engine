import { promises as fs } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  knowledgeManifestSchema,
  terminologySchema
} from '../src/data/knowledge-schema.mjs';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const websiteRoot = path.resolve(scriptDir, '..');
const repoRoot = path.resolve(websiteRoot, '..');
const docsRoot = path.join(repoRoot, 'Docs');
const contentRoot = path.join(websiteRoot, 'src', 'content', 'docs');
const themePath = path.join(repoRoot, 'Design', 'nocturne-theme.json');
const generatedThemeDir = path.join(websiteRoot, 'src', 'styles', 'generated');
const generatedThemePath = path.join(generatedThemeDir, 'nocturne-theme.css');
const manifestPath = path.join(websiteRoot, '.generated-docs.json');
const knowledgeRoot = path.join(repoRoot, 'Knowledge');
const knowledgeManifestPath = path.join(knowledgeRoot, 'manifest.json');
const terminologyPath = path.join(knowledgeRoot, 'terminology.json');
const llmsPath = path.join(repoRoot, 'llms.txt');
const llmsFullPath = path.join(repoRoot, 'llms-full.txt');
const publicRoot = path.join(websiteRoot, 'public');

const allowedSourceMetadata = new Set([
  'id',
  'doc_type',
  'canonical',
  'status',
  'subsystem',
  'phase_introduced',
  'description',
  'source_files',
  'source_docs',
  'book_grounding',
  'aliases',
  'deprecated_aliases',
  'api_symbols'
]);

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

function yamlValue(value) {
  return JSON.stringify(value);
}

function parseFlatValue(raw) {
  const value = raw.trim();

  if (value === '') return '';
  if (value === 'true') return true;
  if (value === 'false') return false;
  if (value === 'null') return null;

  if (/^-?\d+(?:\.\d+)?$/.test(value)) {
    return Number(value);
  }

  if (
    value.startsWith('"') ||
    value.startsWith('[') ||
    value.startsWith('{')
  ) {
    return JSON.parse(value);
  }

  return value;
}

function parseSourceDocument(markdown, relativeSource) {
  const match = markdown.match(/^---\r?\n([\s\S]*?)\r?\n---\r?\n?/);
  if (!match) {
    return { metadata: {}, body: markdown };
  }

  const rawLines = match[1].split(/\r?\n/);
  const looksLikeFrontmatter = rawLines.some((line) =>
    /^(?:id|doc_type|canonical|status|description|subsystem|phase_introduced|source_files|source_docs|book_grounding|aliases|deprecated_aliases|api_symbols):/.test(
      line.trim()
    )
  );

  if (!looksLikeFrontmatter) {
    return { metadata: {}, body: markdown };
  }

  const metadata = {};

  for (const rawLine of rawLines) {
    const line = rawLine.trim();
    if (!line || line.startsWith('#')) continue;

    const field = line.match(/^([a-zA-Z0-9_]+):\s*(.*)$/);
    if (!field) {
      throw new Error(
        `Unsupported multiline/nested source frontmatter in Docs/${relativeSource}: ${rawLine}`
      );
    }

    const key = field[1];
    if (!allowedSourceMetadata.has(key)) {
      throw new Error(
        `Unsupported source frontmatter field "${key}" in Docs/${relativeSource}`
      );
    }

    try {
      metadata[key] = parseFlatValue(field[2]);
    } catch (error) {
      throw new Error(
        `Invalid source frontmatter value for "${key}" in Docs/${relativeSource}: ${error.message}`
      );
    }
  }

  return {
    metadata,
    body: markdown.slice(match[0].length)
  };
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

function canonicalDirectoryTarget(normalized, sourcePrefix, targetPrefix) {
  if (!normalized.startsWith(sourcePrefix)) return null;

  const relative = normalized.slice(sourcePrefix.length);
  const basename = path.posix.basename(relative, path.posix.extname(relative));
  return `${targetPrefix}/${slugify(basename)}.md`;
}

function mapDoc(relativeSource) {
  const normalized = toPosix(relativeSource);

  if (normalized === 'nocturne_engine_architecture.md') {
    return {
      target: 'docs/architecture/overview.md',
      metadata: {
        id: 'noc.architecture.overview',
        doc_type: 'architecture',
        canonical: true,
        status: 'active',
        subsystem: 'Architecture',
        description: 'Canonical layered/subsystem architecture, dependency direction, ownership and engine/application policy boundary.',
        aliases: ['Nocturne Engine', 'Architecture Overview', 'Engine Architecture'],
        deprecated_aliases: []
      }
    };
  }

  if (normalized === 'Production Engineering Standard.md') {
    return {
      target: 'docs/development/production-engineering-standard.md',
      metadata: {
        id: 'noc.development.production-standard',
        doc_type: 'standard',
        canonical: true,
        status: 'active',
        subsystem: 'Development',
        description: 'Canonical engineering quality, CI, validation and production-readiness standard for Nocturne Engine.',
        aliases: ['Production Engineering Standard', 'Production Standard'],
        deprecated_aliases: []
      }
    };
  }

  if (normalized === 'Web/Nocturne Website — Design & Technical Specification.md') {
    return {
      target: 'docs/development/website-specification.md',
      metadata: {
        id: 'noc.web.spec',
        doc_type: 'web-spec',
        canonical: true,
        status: 'active',
        subsystem: 'Website',
        description: 'Canonical product and technical specification for the Nocturne Engine website and documentation surface.',
        aliases: ['Website Specification', 'Nocturne Website'],
        deprecated_aliases: []
      }
    };
  }

  const systemTarget = canonicalDirectoryTarget(
    normalized,
    'Systems/',
    'docs/systems'
  );
  if (systemTarget) {
    return { target: systemTarget, metadata: {} };
  }

  const architectureTarget = canonicalDirectoryTarget(
    normalized,
    'Architecture/',
    'docs/architecture'
  );
  if (architectureTarget) {
    return { target: architectureTarget, metadata: {} };
  }

  const developmentTarget = canonicalDirectoryTarget(
    normalized,
    'Development/',
    'docs/development'
  );
  if (developmentTarget) {
    return { target: developmentTarget, metadata: {} };
  }

  const basename = path.basename(normalized, path.extname(normalized));
  if (/^combined/i.test(basename)) return null;

  return {
    target: `docs/history/${slugify(basename)}.md`,
    metadata: {
      doc_type: 'historical-phase',
      canonical: false,
      status: 'historical'
    }
  };
}

function validateCanonicalMetadata(metadata, relativeSource) {
  if (metadata.canonical !== true) return;

  for (const required of ['id', 'doc_type', 'status']) {
    if (metadata[required] === undefined || metadata[required] === '') {
      throw new Error(
        `Canonical document Docs/${relativeSource} is missing required metadata field "${required}".`
      );
    }
  }

  if (metadata.doc_type === 'system' && !metadata.subsystem) {
    throw new Error(
      `Canonical system document Docs/${relativeSource} must declare "subsystem".`
    );
  }
}

function renderGeneratedFrontmatter(metadata, historical) {
  const orderedKeys = [
    'title',
    'description',
    'id',
    'doc_type',
    'canonical',
    'status',
    'subsystem',
    'phase_introduced',
    'source_files',
    'source_docs',
    'book_grounding',
    'aliases',
    'deprecated_aliases',
    'api_symbols'
  ];

  const lines = ['---'];

  for (const key of orderedKeys) {
    if (metadata[key] === undefined) continue;
    lines.push(`${key}: ${yamlValue(metadata[key])}`);
  }

  if (historical) {
    lines.push(
      'sidebar:',
      '  badge:',
      '    text: History',
      '    variant: default'
    );
  }

  lines.push('---', '');
  return lines.join('\n');
}

async function cleanPreviousGeneratedDocs() {
  try {
    const raw = await fs.readFile(manifestPath, 'utf8');
    const previous = JSON.parse(raw);

    for (const relative of previous.files ?? []) {
      const absolute = path.resolve(contentRoot, relative);

      if (!absolute.startsWith(contentRoot + path.sep)) {
        throw new Error(
          `Refusing to delete path outside content root: ${absolute}`
        );
      }

      await fs.rm(absolute, { force: true });
    }
  } catch (error) {
    if (error?.code !== 'ENOENT') throw error;
  }
}


function apiSymbolHref(symbol) {
  return `/api-symbol/${symbol.replaceAll('::', '.')}.html`;
}

function renderApiReference(metadata) {
  const symbols = metadataArray(metadata, 'api_symbols');
  if (symbols.length === 0) return '';

  return [
    '',
    '## C++ API reference',
    '',
    '> Generated links into the Doxygen symbol reference. Canonical documentation remains the authority for architecture, ownership and subsystem contracts.',
    '',
    ...symbols.map((symbol) => '- [`' + symbol + '`](' + apiSymbolHref(symbol) + ')'),
    ''
  ].join('\n');
}

function webPathForTarget(relativeTarget) {
  const normalized = toPosix(relativeTarget)
    .replace(/\.md$/i, '')
    .replace(/^docs\//, '');
  return `/docs/${normalized}/`;
}

function metadataArray(metadata, key) {
  return Array.isArray(metadata[key]) ? metadata[key] : [];
}

function renderResolvedMetadataFrontmatter(document) {
  const metadata = document.metadata;
  const fields = [
    ['id', metadata.id],
    ['title', document.title],
    ['doc_type', metadata.doc_type],
    ['canonical', true],
    ['status', metadata.status],
    ['subsystem', metadata.subsystem],
    ['phase_introduced', metadata.phase_introduced],
    ['description', metadata.description],
    ['source_path', `Docs/${document.normalizedSource}`],
    ['source_files', metadataArray(metadata, 'source_files')],
    ['source_docs', metadataArray(metadata, 'source_docs')],
    ['book_grounding', metadataArray(metadata, 'book_grounding')],
    ['aliases', metadataArray(metadata, 'aliases')],
    ['deprecated_aliases', metadataArray(metadata, 'deprecated_aliases')],
    ['api_symbols', metadataArray(metadata, 'api_symbols')]
  ];

  const lines = ['---'];

  for (const [key, value] of fields) {
    if (value === undefined) continue;
    lines.push(`${key}: ${yamlValue(value)}`);
  }

  lines.push('---', '');
  return lines.join('\n');
}

function renderAiDocument(document) {
  return [
    renderResolvedMetadataFrontmatter(document).trimEnd(),
    '',
    '> **Canonical AI context.** This export represents current Nocturne documentation. Canonical Architecture/System/Development contracts outrank historical Phase documents for current behavior. Inspect the referenced source files when exact implementation behavior matters.',
    '',
    document.body.trim(),
    ''
  ].join('\n');
}

async function generateKnowledge(canonicalDocuments) {
  const documents = [...canonicalDocuments].sort((a, b) =>
    a.metadata.id.localeCompare(b.metadata.id, 'en')
  );

  const manifest = knowledgeManifestSchema.parse({
    schemaVersion: 1,
    project: {
      id: 'nocturne-engine',
      name: 'Nocturne Engine',
      platform: 'Windows',
      language: 'C++20+',
      target: 'first-person survival horror',
      canonicalDocsRoot: 'Docs/'
    },
    sourcePrecedence: [
      'Current canonical Architecture / Systems / Development contracts.',
      'Current source code for exact implementation behavior.',
      'Completion and implementation reports as evidence of delivered work.',
      'Historical Phase documents for chronology and earlier design context.'
    ],
    documents: documents.map((document) => ({
      id: document.metadata.id,
      title: document.title,
      docType: document.metadata.doc_type,
      canonical: true,
      status: document.metadata.status,
      ...(document.metadata.subsystem
        ? { subsystem: document.metadata.subsystem }
        : {}),
      ...(Number.isInteger(document.metadata.phase_introduced)
        ? { phaseIntroduced: document.metadata.phase_introduced }
        : {}),
      sourcePath: `Docs/${document.normalizedSource}`,
      webPath: webPathForTarget(document.relativeTarget),
      rawPath: `/raw/${document.metadata.id}.md`,
      aiPath: `/ai/${document.metadata.id}.md`,
      description: document.metadata.description,
      sourceFiles: metadataArray(document.metadata, 'source_files'),
      sourceDocs: metadataArray(document.metadata, 'source_docs'),
      bookGrounding: metadataArray(document.metadata, 'book_grounding'),
      aliases: metadataArray(document.metadata, 'aliases'),
      deprecatedAliases: metadataArray(document.metadata, 'deprecated_aliases'),
      apiSymbols: metadataArray(document.metadata, 'api_symbols').map((name) => ({
        name,
        href: apiSymbolHref(name)
      }))
    }))
  });

  const terminology = terminologySchema.parse({
    schemaVersion: 1,
    project: 'nocturne-engine',
    terms: documents.map((document) => ({
      term: document.title,
      documentId: document.metadata.id,
      definition: document.metadata.description,
      aliases: metadataArray(document.metadata, 'aliases'),
      deprecatedAliases: metadataArray(document.metadata, 'deprecated_aliases')
    }))
  });

  const llms = [
    '# Nocturne Engine',
    '',
    '> Custom Windows game engine written in C++20+, targeting a first-person survival horror game.',
    '',
    '## Source precedence',
    '',
    '1. Current canonical Architecture / Systems / Development documentation.',
    '2. Current source code for exact implementation behavior.',
    '3. Completion/implementation reports as delivered-work evidence.',
    '4. Historical Phase documents for chronology only.',
    '',
    'Historical Phase documents must not override current canonical behavior.',
    '',
    '## Canonical documentation',
    '',
    ...manifest.documents.map(
      (document) =>
        `- [${document.title}](${document.webPath}) — ${document.description} [${document.id}] — Source: ${document.sourcePath}`
    ),
    '',
    '## Machine-readable entry points',
    '',
    '- Knowledge/manifest.json — stable canonical document index and source relationships.',
    '- Knowledge/terminology.json — canonical terms, aliases and deprecated aliases.',
    '- Schemas/knowledge-manifest.schema.json — manifest contract.',
    '- Schemas/terminology.schema.json — terminology contract.',
    '- llms-full.txt — generated canonical-only documentation export.',
    '',
    '## Agent rules',
    '',
    '- Prefer canonical documents over Phase/history documents for current behavior.',
    '- Inspect referenced source files when exact implementation detail matters.',
    '- Do not introduce upward engine dependencies.',
    '- Engine provides mechanisms; applications provide policies.',
    '- Engine namespace: noc::.',
    '- Game/application namespace: game::.',
    '- Do not treat roadmap scope as proof that a feature is implemented.',
    ''
  ].join('\n');

  const llmsFull = [
    '# Nocturne Engine — Canonical Documentation Export',
    '',
    '> Generated from current canonical Docs sources only. Historical Phase/completion documents are intentionally excluded from this aggregate.',
    '',
    'Source precedence: canonical documentation -> current source code -> completion evidence -> historical Phase documents.',
    '',
    ...documents.flatMap((document) => [
      '---',
      '',
      `## ${document.title}`,
      '',
      `Document ID: ${document.metadata.id}`,
      `Canonical source: Docs/${document.normalizedSource}`,
      `Web route: ${webPathForTarget(document.relativeTarget)}`,
      '',
      renderAiDocument(document).trim(),
      ''
    ])
  ].join('\n');

  await fs.mkdir(knowledgeRoot, { recursive: true });
  await fs.writeFile(
    knowledgeManifestPath,
    JSON.stringify(manifest, null, 2) + '\n',
    'utf8'
  );
  await fs.writeFile(
    terminologyPath,
    JSON.stringify(terminology, null, 2) + '\n',
    'utf8'
  );
  await fs.writeFile(llmsPath, llms, 'utf8');
  await fs.writeFile(llmsFullPath, llmsFull, 'utf8');

  const publicKnowledge = path.join(publicRoot, 'knowledge');
  const publicSchemas = path.join(publicRoot, 'schemas');
  const publicRaw = path.join(publicRoot, 'raw');
  const publicAi = path.join(publicRoot, 'ai');

  await fs.rm(publicKnowledge, { recursive: true, force: true });
  await fs.rm(publicSchemas, { recursive: true, force: true });
  await fs.rm(publicRaw, { recursive: true, force: true });
  await fs.rm(publicAi, { recursive: true, force: true });

  await fs.mkdir(publicKnowledge, { recursive: true });
  await fs.mkdir(publicSchemas, { recursive: true });
  await fs.mkdir(publicRaw, { recursive: true });
  await fs.mkdir(publicAi, { recursive: true });

  await fs.copyFile(
    knowledgeManifestPath,
    path.join(publicKnowledge, 'manifest.json')
  );
  await fs.copyFile(
    terminologyPath,
    path.join(publicKnowledge, 'terminology.json')
  );
  await fs.copyFile(llmsPath, path.join(publicRoot, 'llms.txt'));
  await fs.copyFile(llmsFullPath, path.join(publicRoot, 'llms-full.txt'));

  const schemaFiles = (await fs.readdir(path.join(repoRoot, 'Schemas')))
    .filter((name) => name.toLowerCase().endsWith('.json'))
    .sort((a, b) => a.localeCompare(b, 'en'));

  for (const schema of schemaFiles) {
    await fs.copyFile(
      path.join(repoRoot, 'Schemas', schema),
      path.join(publicSchemas, schema)
    );
  }

  for (const document of documents) {
    await fs.writeFile(
      path.join(publicRaw, `${document.metadata.id}.md`),
      document.raw.endsWith('\n') ? document.raw : document.raw + '\n',
      'utf8'
    );
    await fs.writeFile(
      path.join(publicAi, `${document.metadata.id}.md`),
      renderAiDocument(document),
      'utf8'
    );
  }

  return {
    documentCount: documents.length,
    terminologyCount: terminology.terms.length,
    schemaCount: schemaFiles.length
  };
}

async function syncDocs() {
  await cleanPreviousGeneratedDocs();

  const sourceFiles = await walkMarkdown(docsRoot);
  const generated = [];
  const claimedTargets = new Map();
  const claimedIds = new Map();
  const historicalPages = [];
  const canonicalDocuments = [];

  for (const source of sourceFiles) {
    const relativeSource = path.relative(docsRoot, source);
    const normalizedSource = toPosix(relativeSource);
    const mapping = mapDoc(relativeSource);

    if (!mapping) continue;

    const relativeTarget = mapping.target;
    const existingTarget = claimedTargets.get(relativeTarget);

    if (existingTarget) {
      throw new Error(
        `Generated documentation collision: ${existingTarget} and ${normalizedSource} both map to ${relativeTarget}`
      );
    }

    claimedTargets.set(relativeTarget, normalizedSource);

    const raw = await fs.readFile(source, 'utf8');
    const sourceDoc = parseSourceDocument(raw, normalizedSource);
    const sourceMetadata = sourceDoc.metadata;

    const fallback = path.basename(source, '.md');
    const title = extractTitle(sourceDoc.body, fallback);
    const historical = mapping.metadata.doc_type === 'historical-phase';

    const metadata = {
      ...mapping.metadata,
      ...sourceMetadata,
      title,
      description:
        sourceMetadata.description ??
        mapping.metadata.description ??
        (historical
          ? `Historical implementation record from Docs/${normalizedSource}. Prefer canonical Architecture/System documentation for current behavior.`
          : `Canonical Nocturne documentation synchronized from Docs/${normalizedSource}.`)
    };

    validateCanonicalMetadata(metadata, normalizedSource);

    if (metadata.canonical === true) {
      canonicalDocuments.push({
        metadata,
        title,
        normalizedSource,
        relativeTarget,
        raw,
        body: sourceDoc.body
      });
    }

    if (metadata.id) {
      const existingId = claimedIds.get(metadata.id);
      if (existingId) {
        throw new Error(
          `Duplicate documentation id "${metadata.id}" in Docs/${existingId} and Docs/${normalizedSource}`
        );
      }
      claimedIds.set(metadata.id, normalizedSource);
    }

    const target = path.join(contentRoot, relativeTarget);
    const body = removeFirstH1(sourceDoc.body).trimStart();
    const sourceNote = historical
      ? '> **Historical record.** For current behavior, prefer canonical Architecture and System documentation.\n\n'
      : '';

    await fs.mkdir(path.dirname(target), { recursive: true });
    await fs.writeFile(
      target,
      renderGeneratedFrontmatter(metadata, historical) +
        sourceNote +
        body +
        renderApiReference(metadata) +
        '\n',
      'utf8'
    );

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
    'title: "Development History"',
    'description: "Historical phase and implementation documents synchronized from the Nocturne repository."',
    'doc_type: "historical-phase"',
    'canonical: false',
    'status: "historical"',
    'tableOfContents: false',
    '---',
    '',
    'These pages preserve the implementation history of Nocturne Engine.',
    '',
    '> **Historical record.** These documents do not override current canonical Architecture/System documentation.',
    '',
    '## Synchronized documents',
    '',
    ...historicalPages.map(
      (page) => `- [${page.title}](/docs/history/${page.slug}/)`
    ),
    ''
  ].join('\n');

  await fs.mkdir(path.dirname(historyIndex), { recursive: true });
  await fs.writeFile(historyIndex, historyBody, 'utf8');
  generated.push(historyIndexRelative);

  await fs.writeFile(
    manifestPath,
    JSON.stringify(
      {
        schemaVersion: 2,
        generatedAtBuildTime: true,
        canonicalIds: [...claimedIds.keys()].sort(),
        files: generated.sort()
      },
      null,
      2
    ) + '\n',
    'utf8'
  );

  const knowledge = await generateKnowledge(canonicalDocuments);

  return {
    generatedCount: generated.length,
    canonicalCount: claimedIds.size,
    knowledge
  };
}

function cssList(values) {
  return values
    .map((value) => (value.includes(' ') ? `"${value}"` : value))
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

async function syncBrandAssets() {
  const logoSource = path.join(
    repoRoot,
    'Apps',
    'NocturneEditor',
    'Resources',
    'NocturneEngine-Logo.png'
  );
  const logoTarget = path.join(publicRoot, 'nocturne-logo.png');

  const tablerSourceRoot = path.join(
    repoRoot,
    'ThirdParty',
    'TablerIcons',
    'icons',
    'outline'
  );
  const iconTargetRoot = path.join(publicRoot, 'icons');
  const icons = [
    'box.svg',
    'device-desktop.svg',
    'world.svg',
    'hierarchy-2.svg',
    'layout-grid.svg',
    'terminal-2.svg',
    'file-text.svg'
  ];

  await fs.mkdir(publicRoot, { recursive: true });
  await fs.mkdir(iconTargetRoot, { recursive: true });

  let copiedLogo = false;

  try {
    await fs.copyFile(logoSource, logoTarget);
    copiedLogo = true;
  } catch (error) {
    if (error?.code !== 'ENOENT') throw error;
  }

  for (const icon of icons) {
    const source = path.join(tablerSourceRoot, icon);
    const target = path.join(iconTargetRoot, icon);

    try {
      await fs.copyFile(source, target);
    } catch (error) {
      if (error?.code === 'ENOENT') {
        throw new Error(`Required Tabler icon is missing: ${toPosix(path.relative(repoRoot, source))}`);
      }
      throw error;
    }
  }

  return {
    copiedLogo,
    copiedIcons: icons.length
  };
}

await generateTheme();
const docs = await syncDocs();
const brand = await syncBrandAssets();

console.log(
  `Prepared Nocturne website: ${docs.generatedCount} docs synchronized (${docs.canonicalCount} canonical IDs); ${docs.knowledge.documentCount} AI knowledge documents; ${docs.knowledge.terminologyCount} terminology entries; ${docs.knowledge.schemaCount} schemas; logo ${brand.copiedLogo ? 'copied' : 'not found'}; ${brand.copiedIcons} Tabler icons copied.`
);
