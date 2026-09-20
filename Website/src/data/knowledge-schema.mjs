import { z } from 'astro/zod';

const id = z.string().regex(/^noc\.[a-z0-9][a-z0-9._-]*$/);
const nonEmpty = z.string().min(1);
const uniqueStrings = z.array(nonEmpty).superRefine((values, ctx) => {
  if (new Set(values).size !== values.length) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      message: 'Array values must be unique.'
    });
  }
});

export const knowledgeDocumentSchema = z.object({
  id,
  title: nonEmpty,
  docType: z.enum(['architecture','system','standard','web-spec','reference']),
  canonical: z.literal(true),
  status: z.enum(['implemented','active','planned','foundation']),
  subsystem: nonEmpty.optional(),
  phaseIntroduced: z.number().int().positive().optional(),
  sourcePath: z.string().regex(/^Docs\//),
  webPath: z.string().regex(/^\/docs\//),
  rawPath: z.string().regex(/^\/raw\/noc\./),
  aiPath: z.string().regex(/^\/ai\/noc\./),
  description: nonEmpty,
  sourceFiles: z.array(nonEmpty),
  sourceDocs: z.array(nonEmpty),
  bookGrounding: z.array(nonEmpty),
  aliases: uniqueStrings,
  deprecatedAliases: uniqueStrings,
  apiSymbols: z.array(z.object({
    name: nonEmpty,
    href: z.string().regex(/^\/api-symbol\//)
  }).strict())
}).strict();

export const knowledgeManifestSchema = z.object({
  schemaVersion: z.literal(1),
  project: z.object({
    id: z.literal('nocturne-engine'),
    name: z.literal('Nocturne Engine'),
    platform: z.literal('Windows'),
    language: z.literal('C++20+'),
    target: z.literal('first-person survival horror'),
    canonicalDocsRoot: z.literal('Docs/')
  }).strict(),
  sourcePrecedence: z.array(nonEmpty).min(4),
  documents: z.array(knowledgeDocumentSchema).min(1)
}).strict().superRefine((manifest, ctx) => {
  const ids = new Set();
  const sources = new Set();

  for (const [index, document] of manifest.documents.entries()) {
    if (ids.has(document.id)) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['documents', index, 'id'],
        message: `Duplicate document id: ${document.id}`
      });
    }
    ids.add(document.id);

    if (sources.has(document.sourcePath)) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['documents', index, 'sourcePath'],
        message: `Duplicate canonical source path: ${document.sourcePath}`
      });
    }
    sources.add(document.sourcePath);
  }
});

export const terminologySchema = z.object({
  schemaVersion: z.literal(1),
  project: z.literal('nocturne-engine'),
  terms: z.array(z.object({
    term: nonEmpty,
    documentId: id,
    definition: nonEmpty,
    aliases: uniqueStrings,
    deprecatedAliases: uniqueStrings
  }).strict()).min(1)
}).strict().superRefine((value, ctx) => {
  const terms = new Set();

  for (const [index, term] of value.terms.entries()) {
    const key = term.term.toLocaleLowerCase('en');
    if (terms.has(key)) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['terms', index, 'term'],
        message: `Duplicate terminology term: ${term.term}`
      });
    }
    terms.add(key);
  }
});
