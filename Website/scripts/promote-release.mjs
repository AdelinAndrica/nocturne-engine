import { readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  releaseSchema,
  releaseManifestSchema
} from '../src/data/release-schema.mjs';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const websiteRoot = path.resolve(scriptDir, '..');
const manifestPath = path.join(websiteRoot, 'src', 'data', 'releases.json');
const recordPaths = process.argv.slice(2);

if (recordPaths.length === 0) {
  console.error(
    'Usage: npm run promote:release -- <release-record-architecture.json> [...]'
  );
  process.exit(2);
}

const records = [];

for (const recordPath of recordPaths) {
  const absolute = path.resolve(process.cwd(), recordPath);
  const raw = JSON.parse(await readFile(absolute, 'utf8'));
  records.push(releaseSchema.parse(raw));
}

const first = records[0];

for (const record of records.slice(1)) {
  for (const field of [
    'version',
    'channel',
    'publishedAt',
    'notesUrl',
    'sourceCommit'
  ]) {
    if (record[field] !== first[field]) {
      throw new Error(
        `Release records disagree on ${field}: ${first[field]} vs ${record[field]}`
      );
    }
  }
}

const builds = records.flatMap((record) => record.builds);
const tuples = new Set();

for (const build of builds) {
  const tuple = `${build.platform}:${build.architecture}:${build.configuration}`;

  if (tuples.has(tuple)) {
    throw new Error(`Duplicate release build tuple: ${tuple}`);
  }

  tuples.add(tuple);
}

builds.sort((a, b) =>
  `${a.platform}:${a.architecture}`.localeCompare(
    `${b.platform}:${b.architecture}`,
    'en'
  )
);

const manifest = JSON.parse(await readFile(manifestPath, 'utf8'));
const parsedManifest = releaseManifestSchema.parse(manifest);

if (parsedManifest.releases.some((release) => release.version === first.version)) {
  throw new Error(
    `Release ${first.version} already exists in Website/src/data/releases.json`
  );
}

const promotedRelease = {
  version: first.version,
  channel: first.channel,
  publishedAt: first.publishedAt,
  notesUrl: first.notesUrl,
  sourceCommit: first.sourceCommit,
  builds
};

const nextManifest = releaseManifestSchema.parse({
  schemaVersion: parsedManifest.schemaVersion,
  latest: promotedRelease.version,
  releases: [
    promotedRelease,
    ...parsedManifest.releases
  ]
});

await writeFile(
  manifestPath,
  JSON.stringify(nextManifest, null, 2) + '\n',
  'utf8'
);

console.log(
  `Promoted Nocturne ${promotedRelease.version} with ${builds.length} published build(s).`
);
