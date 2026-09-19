import { readFile, access } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  knowledgeManifestSchema,
  terminologySchema
} from '../src/data/knowledge-schema.mjs';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const websiteRoot = path.resolve(scriptDir, '..');
const repoRoot = path.resolve(websiteRoot, '..');

const manifestPath = path.join(repoRoot, 'Knowledge', 'manifest.json');
const terminologyPath = path.join(repoRoot, 'Knowledge', 'terminology.json');
const llmsPath = path.join(repoRoot, 'llms.txt');
const llmsFullPath = path.join(repoRoot, 'llms-full.txt');

const manifest = knowledgeManifestSchema.parse(
  JSON.parse(await readFile(manifestPath, 'utf8'))
);
const terminology = terminologySchema.parse(
  JSON.parse(await readFile(terminologyPath, 'utf8'))
);
const llms = await readFile(llmsPath, 'utf8');
const llmsFull = await readFile(llmsFullPath, 'utf8');

const terminologyIds = new Set(terminology.terms.map((term) => term.documentId));

for (const document of manifest.documents) {
  if (!terminologyIds.has(document.id)) {
    throw new Error(`Missing terminology entry for canonical document ${document.id}`);
  }

  if (!llms.includes(document.id)) {
    throw new Error(`llms.txt does not reference canonical id ${document.id}`);
  }

  if (!llmsFull.includes(`Document ID: ${document.id}`)) {
    throw new Error(`llms-full.txt does not contain canonical document ${document.id}`);
  }

  await access(path.join(websiteRoot, 'public', document.rawPath.slice(1)));
  await access(path.join(websiteRoot, 'public', document.aiPath.slice(1)));
}

for (const required of [
  'knowledge/manifest.json',
  'knowledge/terminology.json',
  'schemas/knowledge-manifest.schema.json',
  'schemas/terminology.schema.json',
  'llms.txt',
  'llms-full.txt'
]) {
  await access(path.join(websiteRoot, 'public', required));
}

console.log(
  `Knowledge layer valid: ${manifest.documents.length} canonical documents, ${terminology.terms.length} terminology entries.`
);
