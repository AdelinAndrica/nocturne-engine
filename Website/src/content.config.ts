import { defineCollection } from 'astro:content';
import { z } from 'astro/zod';
import { docsLoader, i18nLoader } from '@astrojs/starlight/loaders';
import { docsSchema, i18nSchema } from '@astrojs/starlight/schema';

const nocturneDocMetadata = z.object({
  id: z.string().regex(/^noc\.[a-z0-9][a-z0-9._-]*$/).optional(),
  doc_type: z.enum([
    'architecture',
    'system',
    'standard',
    'web-spec',
    'historical-phase',
    'reference'
  ]).optional(),
  canonical: z.boolean().optional(),
  status: z.enum([
    'implemented',
    'active',
    'planned',
    'foundation',
    'historical'
  ]).optional(),
  subsystem: z.string().min(1).optional(),
  phase_introduced: z.number().int().positive().optional(),
  source_files: z.array(z.string().min(1)).optional(),
  source_docs: z.array(z.string().min(1)).optional(),
  book_grounding: z.array(z.string().min(1)).optional(),
  aliases: z.array(z.string().min(1)).optional(),
  deprecated_aliases: z.array(z.string().min(1)).optional()
});

export const collections = {
  docs: defineCollection({
    loader: docsLoader(),
    schema: docsSchema({
      extend: nocturneDocMetadata
    })
  }),
  i18n: defineCollection({
    loader: i18nLoader(),
    schema: i18nSchema()
  })
};
