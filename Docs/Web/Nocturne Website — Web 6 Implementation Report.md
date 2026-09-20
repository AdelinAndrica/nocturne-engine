# Nocturne Website — Web 6 Implementation Report

> **Status:** COMPLETE
>
> **Track:** Nocturne Website
>
> **Milestone:** Web 6 — AI Knowledge Layer
>
> **Branch:** `web-docs-foundation`
>
> **Specification:** `Docs/Web/Nocturne Website — Design & Technical Specification.md`
>
> **Canonical documentation contract:** `Docs/Development/Documentation Model.md`

---

## 1. Objective

Web 6 makes Nocturne documentation directly consumable by AI agents, APIs and other tooling without requiring them to scrape rendered website navigation.

The implementation keeps one authored documentation source:

```text
Docs/*.md
   |
   v
Website/scripts/prepare.mjs
   |
   +--> human Starlight pages
   +--> Knowledge/manifest.json
   +--> Knowledge/terminology.json
   +--> llms.txt
   +--> llms-full.txt
   +--> /raw/<noc.id>.md
   +--> /ai/<noc.id>.md
   +--> /knowledge/*
   +--> /schemas/*
```

`Docs/` remains authored truth.

`Knowledge/*.json` and `llms*.txt` are generated artifacts.

---

## 2. Book grounding

Jason Gregory, *Game Engine Architecture (3rd Edition)*, §1.7 grounds the importance of reliable tools and content/data pipelines in the broader engine development environment.

The AI-oriented formats and web affordances are not prescribed by the engine books.

Therefore the following are **Design choice (not directly from the book)**:

- stable machine-readable `noc.*` identity usage;
- JSON knowledge manifest;
- terminology JSON;
- `llms.txt`;
- `llms-full.txt`;
- per-document raw/AI Markdown endpoints;
- Copy-for-AI UI;
- JSON Schema contracts;
- stale generated-output detection.

---

## 3. Canonical source precedence

Web 6 preserves the existing canonical documentation order.

For current behavior:

1. current canonical Architecture / Systems / Development contracts;
2. current source code for exact implementation behavior;
3. completion/implementation reports as delivered-work evidence;
4. historical Phase documents for chronology and earlier design context.

Historical Phase files remain available and searchable.

They are not promoted into the canonical machine-readable document manifest.

---

## 4. Current canonical machine set

Web 6 currently exports ten canonical IDs:

```text
noc.architecture.overview
noc.development.production-standard
noc.development.release-process
noc.documentation.model
noc.editor
noc.render
noc.resources
noc.runtime
noc.web.spec
noc.world
```

The identity is independent of filename.

Renaming a Markdown file does not intentionally rename the concept ID.

---

## 5. Canonical metadata extension

Canonical source frontmatter now supports:

```text
aliases
deprecated_aliases
```

These fields remain authored in `Docs/`.

Example — Editor:

```yaml
aliases: ["Editor","Nocturne Editor","EditorShellV3"]
deprecated_aliases: ["EditorShell","EditorControls"]
```

This lets machine consumers distinguish the current editor shell from historical names without maintaining a second terminology database by hand.

---

## 6. Knowledge manifest

Created:

`Knowledge/manifest.json`

Schema:

`Schemas/knowledge-manifest.schema.json`

Executable validation:

`Website/src/data/knowledge-schema.mjs`

The manifest identifies:

- project identity;
- source precedence;
- stable document ID;
- title;
- document type;
- canonical state;
- implementation/status state;
- subsystem;
- introduction phase where available;
- canonical repository source path;
- website route;
- raw Markdown route;
- AI-normalized route;
- description;
- referenced source files;
- supporting source documents;
- book grounding;
- aliases;
- deprecated aliases.

Only resolved canonical documents enter the manifest.

---

## 7. Terminology

Created:

`Knowledge/terminology.json`

Schema:

`Schemas/terminology.schema.json`

Terminology is generated from:

- canonical page title;
- canonical description;
- authored `aliases`;
- authored `deprecated_aliases`.

This makes terminology ownership follow the canonical document that owns the concept.

No separate manually maintained terminology prose database was introduced.

---

## 8. llms.txt

Created:

`llms.txt`

Static website route:

`/llms.txt`

The file is intentionally compact.

It exposes:

- project identity;
- source-precedence rules;
- all canonical documents;
- stable IDs;
- repository-relative source paths;
- machine-readable entry points;
- core agent rules.

Important rules include:

```text
Prefer canonical documents over Phase/history documents.
Inspect current source files for exact implementation detail.
Do not introduce upward engine dependencies.
Engine provides mechanisms; applications provide policies.
Do not treat roadmap scope as proof of implementation.
```

---

## 9. llms-full.txt

Created:

`llms-full.txt`

Static website route:

`/llms-full.txt`

This is a generated canonical-only aggregate.

At Web 6 completion it is approximately 126k characters.

The aggregate includes current canonical document bodies plus resolved machine metadata.

It does not concatenate historical Phase/completion files as standalone current truth.

A canonical document may reference a historical file in `source_docs` or prose. Such a reference does not promote that historical file into the canonical aggregate.

---

## 10. Raw Markdown exports

Each canonical ID receives:

```text
/raw/<noc.id>.md
```

Example:

`/raw/noc.runtime.md`

The raw endpoint contains the authored repository Markdown.

Its purpose is direct source inspection/copying without rendered HTML.

---

## 11. AI-normalized exports

Each canonical ID also receives:

```text
/ai/<noc.id>.md
```

Example:

`/ai/noc.runtime.md`

The AI export contains:

- resolved stable ID;
- resolved title/type/status/subsystem;
- canonical source path;
- source files/docs;
- book grounding;
- aliases/deprecated aliases;
- an explicit canonical-precedence notice;
- canonical Markdown body.

This is the preferred single-document copy surface for AI contexts.

---

## 12. Documentation UI affordances

Updated:

`Website/src/components/docs/NocturnePageTitle.astro`

Canonical pages now expose:

- **View Markdown**
- **Copy Markdown**
- **Copy for AI**

Copy actions fetch the generated static Markdown endpoint and place its exact text on the clipboard.

An accessible `aria-live` status announces copy success/failure.

Historical pages do not receive the canonical Copy-for-AI affordance automatically.

This avoids making an old Phase document look equivalent to current system authority.

---

## 13. Static machine-readable routes

Web 6 publishes:

```text
/llms.txt
/llms-full.txt

/knowledge/manifest.json
/knowledge/terminology.json

/schemas/knowledge-manifest.schema.json
/schemas/terminology.schema.json
/schemas/release.schema.json

/raw/<noc.id>.md
/ai/<noc.id>.md
```

Schemas remain version-controlled under repository-root `Schemas/`.

---

## 14. Single-generator architecture

The same:

`Website/scripts/prepare.mjs`

pass now resolves canonical metadata for both:

- human Starlight documentation;
- machine-readable outputs.

This avoids two separate mapping/precedence implementations.

The generator also copies machine-readable outputs into `Website/public/` for the static site.

---

## 15. Executable validation

Created:

`Website/scripts/validate-knowledge.mjs`

Command:

```powershell
cd Website
npm run validate:knowledge
```

The website lifecycle now runs:

```text
validate release manifest
prepare Nocturne docs/knowledge
validate knowledge
Astro build
```

The knowledge validator checks:

- manifest schema;
- terminology schema;
- terminology coverage for every canonical document;
- canonical ID presence in `llms.txt`;
- canonical document presence in `llms-full.txt`;
- raw Markdown file presence;
- AI Markdown file presence;
- public manifest/terminology/schema/llms endpoints.

---

## 16. Stale-output protection

Website CI now regenerates machine-readable knowledge and runs:

```text
git diff --exit-code -- Knowledge llms.txt llms-full.txt
```

Therefore:

```text
canonical Docs change
       |
       v
generated Knowledge/llms changes
       |
       +--> committed output matches -> CI continues
       |
       +--> committed output stale   -> CI fails
```

This makes generated repository-level AI entry points reviewable while preventing them from silently drifting from authored documentation.

---

## 17. Docs Home integration

The documentation home now exposes direct machine entry points:

- `/llms.txt`;
- `/llms-full.txt`;
- knowledge manifest;
- terminology;
- manifest schema;
- terminology schema.

This makes the machine layer discoverable without hiding it behind implementation knowledge.

---

## 18. Source-of-truth guarantees

Web 6 intentionally does **not** create:

- a second hand-authored copy of system documentation;
- a vector database;
- an external hosted RAG service;
- inferred dependency relationships not present in canonical documentation;
- canonical entries for Phase files;
- generated claims that future roadmap items are already implemented.

Machine output is generated only from metadata/documentation already designated canonical.

---

## 19. Files introduced or materially changed

### New schemas

```text
Schemas/knowledge-manifest.schema.json
Schemas/terminology.schema.json
Website/src/data/knowledge-schema.mjs
```

### New generated repository entry points

```text
Knowledge/manifest.json
Knowledge/terminology.json
llms.txt
llms-full.txt
```

### New validation

```text
Website/scripts/validate-knowledge.mjs
```

### Generation / UI

```text
Website/scripts/prepare.mjs
Website/src/components/docs/NocturnePageTitle.astro
Website/src/content.config.ts
Website/src/content/docs/docs/index.md
Website/package.json
.github/workflows/web-ci.yml
```

### Canonical metadata

```text
Docs/Systems/Runtime.md
Docs/Systems/Resources.md
Docs/Systems/Rendering.md
Docs/Systems/World & ECS.md
Docs/Systems/Editor.md
Docs/Development/Documentation Model.md
Docs/Development/Release Process.md
```

---

## 20. Verification checklist

- [x] stable `noc.*` IDs feed machine outputs;
- [x] repository-root knowledge manifest exists;
- [x] terminology exists;
- [x] aliases come from canonical authored metadata;
- [x] deprecated aliases are distinguishable;
- [x] `llms.txt` exists;
- [x] `llms-full.txt` is canonical-only;
- [x] historical docs are not canonical manifest entries;
- [x] raw per-document Markdown endpoints exist;
- [x] AI-normalized per-document endpoints exist;
- [x] canonical docs expose View Markdown;
- [x] canonical docs expose Copy Markdown;
- [x] canonical docs expose Copy for AI;
- [x] knowledge JSON Schemas exist;
- [x] executable schema validation exists;
- [x] generated root outputs are stale-checked by CI;
- [x] human website and machine layer share the same canonical metadata pass;
- [x] Website remains independent of engine/runtime dependencies.

---

## 21. Deferred scope

### Doxygen / C++ symbol graph

Exact C++ API documentation is Web 7.

Web 6 can point to `source_files`, but it does not invent a symbol graph from source code.

### RAG / embeddings / vector search

No external RAG/vector service is required for the current static documentation problem.

A future requirement may justify one, but Web 6 deliberately keeps the first machine interface deterministic and repository-native.

### Additional structured dependency relationships

Fields such as `depends_on`, `implements` or `supersedes` should only be introduced when authored canonical contracts can supply them reliably.

Web 6 does not infer them from filenames/history.

---

## 22. Completion statement

**Web 6 — AI Knowledge Layer is COMPLETE for its defined implementation scope.**

Nocturne now exposes its current canonical documentation in both human-readable and deterministic machine-readable forms while preserving a single authored source and explicit source precedence.

The next milestone is **Web 7 — C++ API Reference**.

---

## 23. Next chat handoff

Say:

> Continue with **Web 7 — C++ API Reference** on branch `web-docs-foundation`. Read the Website specification, `Docs/Development/Documentation Model.md`, and the Web 6 implementation report first. Add repository-owned Doxygen configuration, generate human-readable API HTML and tooling XML from current C++ headers, integrate API navigation into the Nocturne docs, and connect conceptual canonical system pages to relevant C++ symbols without replacing the existing architecture/system explanations. Preserve canonical-vs-history precedence, keep Website independent from engine/runtime dependencies, and do not merge draft PR #2 automatically.
