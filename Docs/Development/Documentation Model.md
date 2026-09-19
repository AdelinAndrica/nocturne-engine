---
id: "noc.documentation.model"
doc_type: "standard"
canonical: true
status: "active"
subsystem: "Documentation"
phase_introduced: 16
description: "Canonical source-of-truth, metadata, precedence and generated-documentation rules for Nocturne technical documentation."
source_files: ["Website/scripts/prepare.mjs","Website/src/content.config.ts","Website/astro.config.mjs"]
source_docs: ["Docs/nocturne_engine_architecture.md","Docs/Production Engineering Standard.md","Docs/Web/Nocturne Website — Design & Technical Specification.md"]
book_grounding: ["Jason Gregory — Game Engine Architecture (3rd ed.), §1.7 Tools and the Asset Pipeline"]
aliases: ["Documentation Model","Canonical Documentation","Docs Model"]
deprecated_aliases: []
---

# Documentation Model

## Purpose

Nocturne documentation must answer two different questions without mixing them:

1. **How does the engine work now?**
2. **How was the engine implemented over time?**

Canonical documentation answers the first question.

Phase/completion/history documents answer the second.

The distinction is required for human readers, future maintainers and AI agents.

## Source precedence

When sources disagree, use this order for current behavior:

1. current canonical Architecture / Systems / Development contracts;
2. current source code for exact implementation behavior;
3. completion/implementation reports as evidence of what a phase delivered;
4. historical Phase documents for chronology and earlier design context.

Roadmap text describes assigned scope. It does not prove that a future feature is already implemented.

A historical Phase document must never silently override a later canonical system page.

## Authored source

Repository-root `Docs/` remains the authored Markdown source.

The Starlight tree under `Website/src/content/docs/` is generated or website-shell content.

Generated synchronized documents are not edited by hand.

```text
Docs/*.md
   |
   v
Website/scripts/prepare.mjs
   |
   +--> validated Starlight content
   +--> canonical/history metadata
   +--> static search index
```

## Canonical directories

Current canonical authored areas are:

```text
Docs/Architecture/
Docs/Systems/
Docs/Development/
```

The existing root `Docs/nocturne_engine_architecture.md` remains the top-level architecture contract and is mapped to the canonical Architecture route.

Web implementation reports and Phase documents are historical records even when they are recent.

## Metadata contract

Canonical documents use flat source frontmatter.

Required canonical identity fields:

| Field | Meaning |
|---|---|
| `id` | stable semantic identifier, independent of filename |
| `doc_type` | architecture, system, standard, web-spec, etc. |
| `canonical` | whether the page is current authority |
| `status` | implemented, active, planned, foundation, historical |
| `description` | concise purpose for UI/search |
| `source_files` | current implementation files relevant to the contract |
| `source_docs` | supporting authored documentation |
| `book_grounding` | book sections that ground concepts/terminology |
| `aliases` | current alternate names useful to humans/search/AI |
| `deprecated_aliases` | legacy names that must not be treated as the current canonical surface |

System pages additionally declare:

- `subsystem`;
- `phase_introduced` where useful.

Stable IDs use the `noc.*` namespace.

Examples:

```text
noc.runtime
noc.resources
noc.render
noc.world
noc.editor
noc.documentation.model
```

Renaming a Markdown file must not change its semantic ID.

## Frontmatter shape

The source synchronizer intentionally accepts a constrained flat frontmatter format.

Arrays are authored inline, including terminology aliases:

```yaml
---
id: "noc.resources"
doc_type: "system"
canonical: true
status: "implemented"
subsystem: "Resources"
phase_introduced: 4
source_files: ["Engine/Resources/ResourceManager.h"]
source_docs: ["Docs/Phase 4 — Resource Manager.md"]
book_grounding: ["Jason Gregory — Game Engine Architecture (3rd ed.), §7.2"]
aliases: ["Resources","Resource Manager","VFS"]
deprecated_aliases: []
---
```

This constrained form keeps synchronization deterministic and avoids adding a second general-purpose YAML transformation layer merely for generated copies.

**Design choice (not directly from the book):** the exact metadata schema and flat-frontmatter parser are Nocturne documentation-tooling policy.

## Historical documents

Documents not explicitly mapped to canonical Architecture/System/Development surfaces are synchronized into Development History.

Generated historical pages receive:

```text
doc_type = historical-phase
canonical = false
status = historical
```

Their description and page title treatment identify them as history.

They remain searchable because implementation history is still useful.

## Validation

The website content collection extends Starlight's schema with Nocturne metadata.

The preparation step also validates:

- canonical IDs are present;
- canonical IDs are unique;
- canonical pages have a recognized document type/status;
- system pages declare a subsystem;
- generated target paths do not collide;
- source frontmatter uses the supported flat syntax.

Validation failures stop the website build.

## AI / machine-readable knowledge layer

Web 6 extends the same canonical documentation pass with machine-readable outputs.

```text
Docs/*.md
   |
   v
Website/scripts/prepare.mjs
   |
   +--> Starlight canonical/history pages
   +--> Knowledge/manifest.json
   +--> Knowledge/terminology.json
   +--> llms.txt
   +--> llms-full.txt
   +--> Website/public/raw/<noc.id>.md
   +--> Website/public/ai/<noc.id>.md
   +--> Website/public/knowledge/*
   +--> Website/public/schemas/*
```

`Docs/` remains the authored source.

`Knowledge/manifest.json`, `Knowledge/terminology.json`, `llms.txt` and `llms-full.txt` are generated artifacts. Do not hand-edit them as an alternate documentation source.

### Knowledge manifest

`Knowledge/manifest.json` contains only documents whose resolved metadata has:

```text
canonical = true
id = noc.*
```

Each record exposes:

- stable ID;
- title;
- document type/status/subsystem;
- canonical authored source path;
- website route;
- raw Markdown route;
- AI-normalized Markdown route;
- description;
- source files/docs;
- book grounding;
- aliases and deprecated aliases.

Historical Phase/completion documents are intentionally absent as canonical manifest entries.

### Terminology

`Knowledge/terminology.json` is generated from canonical document titles/descriptions plus authored `aliases` / `deprecated_aliases`.

This keeps terminology close to the document that owns the concept.

For example, the current Editor page authors:

```text
aliases:
  Editor
  Nocturne Editor
  EditorShellV3

deprecated aliases:
  EditorShell
  EditorControls
```

The generated terminology file therefore does not need a second manually maintained glossary for these names.

### LLM entry points

`llms.txt` is the compact project entry point.

It identifies:

- canonical source precedence;
- all current canonical documents;
- stable `noc.*` IDs;
- machine-readable entry points;
- repository rules relevant to agents.

`llms-full.txt` is a generated canonical-only aggregate.

It does **not** concatenate historical Phase/completion documents as standalone truth.

Historical paths may still appear inside canonical text/metadata as supporting references. That is not the same as including the historical document as a canonical export.

### Raw vs AI-normalized Markdown

For each canonical ID:

```text
/raw/<noc.id>.md
/ai/<noc.id>.md
```

The raw route contains the authored repository Markdown.

The AI route contains resolved canonical metadata plus an explicit source-precedence notice before the canonical document body.

Canonical documentation pages expose:

- View Markdown;
- Copy Markdown;
- Copy for AI.

Historical pages do not receive the canonical Copy-for-AI affordance automatically.

### Schema and stale-output validation

Machine-readable contracts are versioned under `Schemas/`.

Current Web 6 schemas:

- `Schemas/knowledge-manifest.schema.json`;
- `Schemas/terminology.schema.json`.

The website build also validates the generated files through `Website/src/data/knowledge-schema.mjs`.

CI regenerates knowledge and fails when tracked generated outputs differ from the committed versions.

Therefore a canonical `Docs/` change cannot silently leave repository-level AI exports stale.

**Design choice (not directly from the book):** the knowledge manifest format, terminology format, `llms.txt`, canonical-only aggregate, raw/AI endpoints and stale-generation policy are Nocturne tooling decisions.

## Search policy

Pagefind indexes current and historical documentation.

Canonical pages are the preferred current reference because their titles, descriptions and page metadata clearly identify them as current authority.

Historical pages remain available for chronology and debugging regressions.

A later AI knowledge milestone may create a canonical-only aggregate for machine consumption.

## Book grounding

Gregory §1.7 emphasizes that engine/tool pipelines need reliable processes and data flow.

The documentation precedence model, stable semantic IDs, Starlight generation and AI-oriented metadata are **Design choice (not directly from the book)**.

## Related documentation

- [Architecture Overview](/docs/architecture/overview/)
- [Production Engineering Standard](/docs/development/production-engineering-standard/)
- [Development History](/docs/history/)
