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

Arrays are authored inline:

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
