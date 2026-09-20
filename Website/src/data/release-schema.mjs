import { z } from 'astro/zod';

const semverPattern = /^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?$/;
const isoDatePattern = /^[0-9]{4}-[0-9]{2}-[0-9]{2}$/;
const commitPattern = /^[0-9a-f]{40}$/;
const sha256Pattern = /^[0-9a-f]{64}$/;
const httpsPattern = /^https:\/\//;

export const releaseBuildSchema = z.object({
  platform: z.literal('windows'),
  architecture: z.enum(['x86_64', 'x86', 'arm64']),
  configuration: z.literal('ship'),
  status: z.enum(['supported', 'experimental', 'withdrawn']),
  file: z.string().min(1),
  bytes: z.number().int().positive(),
  sha256: z.string().regex(sha256Pattern),
  url: z.string().regex(httpsPattern)
}).strict();

export const releaseSchema = z.object({
  version: z.string().regex(semverPattern),
  channel: z.enum(['development', 'preview', 'stable']),
  publishedAt: z.string().regex(isoDatePattern),
  notesUrl: z.string().min(1),
  sourceCommit: z.string().regex(commitPattern),
  builds: z.array(releaseBuildSchema).min(1)
}).strict().superRefine((release, ctx) => {
  const keys = new Set();

  for (const build of release.builds) {
    const key = `${build.platform}:${build.architecture}:${build.configuration}`;
    if (keys.has(key)) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['builds'],
        message: `Duplicate build tuple: ${key}`
      });
    }
    keys.add(key);
  }
});

export const releaseManifestSchema = z.object({
  schemaVersion: z.literal(1),
  latest: z.string().regex(semverPattern).nullable(),
  releases: z.array(releaseSchema)
}).strict().superRefine((manifest, ctx) => {
  const versions = new Set();

  for (const release of manifest.releases) {
    if (versions.has(release.version)) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['releases'],
        message: `Duplicate release version: ${release.version}`
      });
    }
    versions.add(release.version);
  }

  if (manifest.releases.length === 0) {
    if (manifest.latest !== null) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['latest'],
        message: 'latest must be null when releases is empty'
      });
    }
    return;
  }

  if (manifest.latest === null || !versions.has(manifest.latest)) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['latest'],
      message: 'latest must reference a version present in releases'
    });
  }

  if (manifest.latest !== manifest.releases[0]?.version) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['latest'],
      message: 'latest must equal the first release entry'
    });
  }

  for (let i = 1; i < manifest.releases.length; ++i) {
    if (manifest.releases[i - 1].publishedAt < manifest.releases[i].publishedAt) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['releases', i],
        message: 'releases must be ordered newest first by publishedAt'
      });
    }
  }
});

export function formatBytes(bytes) {
  if (!Number.isFinite(bytes) || bytes <= 0) return '—';

  const units = ['B', 'KB', 'MB', 'GB'];
  let value = bytes;
  let unit = 0;

  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit += 1;
  }

  const precision = value >= 100 || unit === 0 ? 0 : value >= 10 ? 1 : 2;
  return `${value.toFixed(precision)} ${units[unit]}`;
}

export function shortChecksum(sha256) {
  return `${sha256.slice(0, 12)}…${sha256.slice(-8)}`;
}
