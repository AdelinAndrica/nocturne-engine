import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { releaseManifestSchema } from '../src/data/release-schema.mjs';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const websiteRoot = path.resolve(scriptDir, '..');
const manifestPath = path.join(websiteRoot, 'src', 'data', 'releases.json');

const raw = JSON.parse(await readFile(manifestPath, 'utf8'));
const result = releaseManifestSchema.safeParse(raw);

if (!result.success) {
  console.error('Invalid Nocturne release manifest:');
  console.error(result.error.issues);
  process.exit(1);
}

const publishedBuilds = result.data.releases.reduce(
  (sum, release) => sum + release.builds.length,
  0
);

console.log(
  `Release manifest valid: ${result.data.releases.length} releases, ${publishedBuilds} published builds.`
);
