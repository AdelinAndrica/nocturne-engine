---
title: Nocturne Engine Documentation
description: Architecture, canonical systems, development standards and implementation history for Nocturne Engine.
tableOfContents: false
---

Nocturne Engine targets **Windows**, uses **Modern C++ (C++20+)**, and is being developed for a **first-person survival horror** game.

This portal separates **current canonical behavior** from **implementation history**.

## Start here

| Area | Current reference |
|---|---|
| Architecture | [Architecture Overview](/docs/architecture/overview/) |
| Runtime | [Runtime](/docs/systems/runtime/) |
| Resources | [Resources](/docs/systems/resources/) |
| Rendering | [Rendering](/docs/systems/rendering/) |
| World model | [World & ECS](/docs/systems/world-ecs/) |
| Editor | [Editor](/docs/systems/editor/) |
| Engineering quality | [Production Engineering Standard](/docs/development/production-engineering-standard/) |
| Documentation rules | [Documentation Model](/docs/development/documentation-model/) |
| C++ symbols | [C++ API Reference](/docs/development/c-api-reference/) · [Generated API](/api/index.html) |

## How to read these docs

**Canonical** pages describe how Nocturne works now.

**History** pages preserve phase plans, implementation reports and completion evidence. They remain searchable, but they do not override later canonical contracts.

> For an exact current implementation detail, follow the source-file references on the canonical page and inspect the code.

## Source of truth

The authored Markdown remains in repository-root `Docs/`.

The website synchronizes that content into Starlight before development and production builds. Generated synchronized copies are never edited by hand.

See [Documentation Model](/docs/development/documentation-model/) for precedence, metadata and stable-ID rules.


## AI / API entry points

Nocturne exposes generated machine-readable views of the same canonical documentation:

| Entry point | Purpose |
|---|---|
| [`/llms.txt`](/llms.txt) | Compact project orientation and canonical source precedence |
| [`/llms-full.txt`](/llms-full.txt) | Canonical-only documentation aggregate |
| [`/knowledge/manifest.json`](/knowledge/manifest.json) | Stable `noc.*` document index, metadata and source relationships |
| [`/knowledge/terminology.json`](/knowledge/terminology.json) | Canonical terms, aliases and deprecated aliases |
| [Knowledge manifest schema](/schemas/knowledge-manifest.schema.json) | Machine-readable manifest contract |
| [Terminology schema](/schemas/terminology.schema.json) | Machine-readable terminology contract |
| [`/api/index.html`](/api/index.html) | Generated human-readable C++ symbol reference |
| [`/api-xml/index.xml`](/api-xml/index.xml) | Doxygen XML for symbol-level tooling |
| [`/api-symbols.json`](/api-symbols.json) | Generated qualified-name → Doxygen symbol map |

Canonical pages also expose **View Markdown**, **Copy Markdown** and **Copy for AI** actions.

The AI export preserves the same precedence rule as the human documentation: canonical Architecture/System/Development contracts describe current behavior; historical Phase documents remain chronology and evidence, not current authority.
