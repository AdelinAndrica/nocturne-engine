# Nocturne Website — Design & Technical Specification

> **Status:** FOUNDATION CONTRACT — Web Track
>
> **Branch:** `web-docs-foundation`
>
> **Product:** Nocturne Engine Website + Documentation Portal
>
> **Runtime relationship:** external tooling/documentation product; never a runtime-engine dependency
>
> **Current code baseline audited:** `master` at `91feb6411910a398827792b20059e7c275b4983c`
>
> **Visual baseline audited:** current `EditorShellV3`, inherited from the validated Phase 13 UI baseline and Phase 14 menu-bar correction
>
> **Primary architectural source:** Jason Gregory, *Game Engine Architecture (3rd Edition)*
>
> **Web implementation status:** Web 7 C++ API Reference implemented on `web-docs-foundation`; repository-owned Doxygen now generates human HTML, tooling XML, stable qualified-name symbol aliases and conceptual-doc → C++ symbol links. Web 6 canonical AI knowledge and Web 5 artifact-driven Downloads remain intact.

---

## 1. Objective

Build a lightweight, modern Nocturne Engine website that is visually recognizable as the web surface of the Nocturne Editor rather than a separate generic documentation theme.

The website has three primary product surfaces:

1. **Landing** — concise product identity, current capabilities and direct routes into downloads/documentation.
2. **Downloads** — release/build discovery driven by generated release metadata, showing only artifacts that actually exist.
3. **Documentation** — searchable, structured technical documentation for humans while preserving a machine-readable source for AI agents and APIs.

The site must work locally first and remain suitable for later static deployment.

Target local routes:

```text
/                     Landing
/download             Downloads
/docs                  Documentation home
/docs/...              Documentation pages
/roadmap               Roadmap view (later Web milestone)
/changelog             Release/change history (later Web milestone)

/llms.txt               AI entry point
/llms-full.txt          Canonical documentation export
/knowledge/...          Machine-readable project knowledge
/schemas/...            Machine-readable format contracts
```

---

## 2. Book grounding

### 2.1 Tools are part of the engine development ecosystem

Jason Gregory, *Game Engine Architecture (3rd Edition)*, §1.7 describes tools and the asset pipeline as part of the broader game-engine development environment and emphasizes that tools need to be usable and reliable for teams to produce polished games efficiently.

Gregory §15.4 describes the game world editor as the gameplay-side tool used to define and populate game worlds. §15.4.1 further identifies common editor facilities including world visualization, navigation, selection, property inspection and object-placement aids, while §15.4.2 discusses integrated asset-management tooling.

These sections ground the importance of coherent, reliable developer tooling and the technical concepts that the website will document.

### 2.2 What the books do not prescribe

**Design choice (not directly from the book):** the website framework, static-site architecture, routes, CSS implementation, responsive behavior, AI metadata formats, `llms.txt`, release manifest, documentation information architecture and reuse of Nocturne Editor visual tokens are project/product decisions.

The website must not be presented as an engine architecture mandated by the books.

---

## 3. Product boundary

The website is an external consumer of Nocturne project information.

It must never become a dependency of:

- `Engine/`;
- `Apps/NocturneEditor/`;
- runtime rendering;
- runtime resources;
- gameplay;
- build-time engine initialization.

The dependency direction is:

```text
Engine / Editor / Tools / Docs / Build outputs
                    |
                    v
               Website build
                    |
                    v
             static web output
```

Never:

```text
Engine --> Website
Editor --> Website runtime
```

The website may consume generated metadata and source documentation. The engine must never require Node.js, Astro, Starlight or browser assets in order to build or run.

This follows the existing Nocturne rule that mechanisms and policies remain separated and that upward dependencies are not allowed.

---

## 4. Website technical architecture

### 4.1 Selected stack

**Design choice (not directly from the book):**

- **Astro** — site/application shell and static generation.
- **Starlight** — documentation surface.
- **Markdown / MDX** — authored documentation representation used by the web build.
- **Pagefind** — local/static full-text documentation search through Starlight.
- **CSS custom properties** — Nocturne web design tokens.
- **Tabler SVG** — same general icon vocabulary as Nocturne Editor.
- **Minimal client JavaScript** — only where interactions require it.
- **Doxygen XML/HTML** — later C++ API reference input/output.
- **JSON/YAML + JSON Schema** — machine-readable project/release/document contracts.

Astro custom routes are used for Landing and Downloads. Starlight owns the documentation experience.

Official implementation references:

- https://starlight.astro.build/getting-started/
- https://starlight.astro.build/guides/pages/
- https://starlight.astro.build/guides/site-search/
- https://starlight.astro.build/reference/overrides/

### 4.2 Static-first rule

**Design choice (not directly from the book):** the initial website is fully static.

No backend is required for:

- documentation;
- Pagefind search;
- landing content;
- release manifest rendering when metadata is available at build time;
- AI-readable files;
- architecture diagrams;
- changelog/roadmap pages.

A backend may be introduced only when a concrete future requirement justifies it.

### 4.3 Proposed repository layout

```text
Nocturne/
├── Engine/
├── Apps/
├── Tools/
├── Data/
│
├── Docs/
│   ├── Architecture/
│   ├── Systems/
│   ├── Editor/
│   ├── Development/
│   ├── ADR/
│   ├── Phases/
│   ├── Reference/
│   └── Web/
│       └── Nocturne Website — Design & Technical Specification.md
│
├── Design/
│   ├── nocturne-theme.json                 # later Web milestone
│   └── generated/
│       ├── nocturne-theme.generated.css
│       └── NocturneTheme.generated.h       # optional future editor integration
│
├── Knowledge/
│   ├── engine.yaml
│   ├── systems.yaml
│   ├── terminology.yaml
│   └── manifest.json
│
├── Schemas/
│   ├── documentation.schema.json
│   ├── release.schema.json
│   └── ...
│
├── Website/
│   ├── src/
│   │   ├── pages/
│   │   │   ├── index.astro
│   │   │   └── download.astro
│   │   ├── components/
│   │   ├── styles/
│   │   └── content/
│   │       └── docs/                       # generated/synchronized input
│   ├── public/
│   ├── scripts/
│   ├── astro.config.mjs
│   ├── package.json
│   └── tsconfig.json
│
├── llms.txt                                # generated
└── llms-full.txt                           # generated
```

This directory structure is a target contract. It is not all created by this specification commit.

---

## 5. Documentation source-of-truth contract

### 5.1 One authored source

The repository must not maintain two manually edited copies of technical documentation.

The contract is:

```text
Docs/*.md
   |
   +--> website documentation
   +--> Pagefind index
   +--> llms exports
   +--> Knowledge manifest references
```

`Docs/` remains the authored documentation source.

### 5.2 Starlight ingestion

**Design choice (not directly from the book):** because Starlight's normal documentation collection lives under `src/content/docs/`, the website build will use a deterministic synchronization/generation step:

```text
Docs/ canonical Markdown
        |
        v
Website/scripts/sync-docs.*
        |
        v
Website/src/content/docs/ generated tree
        |
        v
Astro/Starlight build
```

Rules:

- generated web-doc copies are not edited by hand;
- generated files must clearly state their origin;
- generation must be deterministic;
- broken links/frontmatter fail the web validation step rather than silently producing wrong content;
- Windows development must not depend on administrator-only symbolic-link behavior;
- source Markdown remains readable outside the website toolchain.

### 5.3 Current truth versus historical truth

This distinction is non-negotiable for both humans and AI agents.

```text
Docs/Systems/
    current canonical subsystem behavior

Docs/Architecture/
    current global architectural contract

Docs/ADR/
    why important architectural/product decisions were made

Docs/Phases/
    chronological implementation history
```

Phase documents must not silently outrank current system documentation when they disagree with a later implementation.

Target frontmatter:

```yaml
---
id: noc.resources.manager
title: Resource Manager
type: system
canonical: true
status: implemented
phase_introduced: 4
source_files:
  - Engine/Resources/ResourceManager.h
  - Engine/Resources/ResourceManager.cpp
depends_on:
  - noc.core
  - noc.resources.vfs
related:
  - noc.resources.loaders
---
```

Historical phase pages use:

```yaml
type: historical-phase
canonical: false
```

### 5.4 Migration rule

Existing phase files remain intact until a deliberate documentation-migration pass.

The first website milestone must not destructively reorganize all existing documentation just to make the website work.

The migration order is:

1. make existing docs render;
2. establish canonical Architecture/System pages;
3. add stable metadata;
4. move/alias historical phase content only with validated link migration.

---

## 6. Nocturne visual identity — audited current baseline

### 6.1 Visual source of truth

The current visual reference is:

- `Apps/NocturneEditor/EditorShellV3.*`;
- `Apps/NocturneEditor/EditorTheme.*`;
- `Apps/NocturneEditor/EditorIconRenderer.*`;
- Phase 13 visual completion/fidelity documents;
- Phase 14's final menu-bar correction.

The historical `EditorShell.*` / `EditorControls.*` path is not the design reference for new website work.

### 6.2 Color tokens

The following values are taken directly from the current `EditorTheme.cpp` implementation.

| Token | RGB | Hex | Web role |
|---|---:|---:|---|
| `windowBg` | 10, 15, 22 | `#0A0F16` | page/base background |
| `panelBg` | 16, 23, 32 | `#101720` | primary surfaces/cards/docs panes |
| `panelBgAlt` | 20, 28, 39 | `#141C27` | elevated/header surfaces |
| `viewportBg` | 13, 22, 33 | `#0D1621` | hero/visual canvas accents |
| `toolbarBg` | 14, 21, 30 | `#0E151E` | top navigation/tool strips |
| `inputBg` | 12, 19, 28 | `#0C131C` | inputs/search/code-adjacent surfaces |
| `buttonBg` | 21, 31, 45 | `#151F2D` | neutral button background |
| `buttonHover` | 27, 40, 58 | `#1B283A` | hover surface |
| `border` | 34, 47, 63 | `#222F3F` | subtle borders/dividers |
| `textPrimary` | 222, 230, 240 | `#DEE6F0` | primary text |
| `textMuted` | 132, 149, 171 | `#8495AB` | secondary text |
| `accent` | 25, 132, 236 | `#1984EC` | active/action accent |
| `accentHover` | 42, 148, 250 | `#2A94FA` | accent hover/focus |
| `success` | 78, 205, 112 | `#4ECD70` | success/ready/supported |
| `warning` | 226, 166, 67 | `#E2A643` | warning/experimental |
| `danger` | 232, 86, 103 | `#E85667` | errors/unsupported |

**Design choice (not directly from the book):** the website will reuse these color values as its primary palette instead of creating a visually similar but independent web palette.

### 6.3 Color usage rules

**Design choice (not directly from the book):**

- Default page background is `windowBg`.
- Main docs/content surfaces use `panelBg`.
- Headers, sticky navigation and secondary surfaces use `toolbarBg` / `panelBgAlt`.
- Borders remain low contrast and primarily use `border`.
- Electric blue is reserved for interaction, focus, current navigation state and primary calls to action.
- Green indicates successful/supported/ready state; it must not become a second general accent.
- Warning and danger colors are semantic only.
- Large decorative gradients are not part of the baseline.
- Avoid pure black and pure white when a Nocturne token exists.
- Avoid bright stock browser-control appearance.

### 6.4 Typography

Current editor typography:

- UI: `Segoe UI Variable Text`;
- brand/display: `Segoe UI Variable Display`;
- console: `Cascadia Mono`.

Current effective editor sizes include:

- standard UI: 12 px;
- semibold UI: 12 px;
- top menu: 14 px semibold;
- compact/small text: 11 px;
- console: 11 px;
- editor placeholder brand mark: 42 px semibold.

**Design choice (not directly from the book):** website font stacks are system/local stacks; the repository will not vendor or redistribute Microsoft font files.

Target CSS stacks:

```css
--font-ui:
  "Segoe UI Variable Text",
  "Segoe UI",
  system-ui,
  -apple-system,
  BlinkMacSystemFont,
  sans-serif;

--font-display:
  "Segoe UI Variable Display",
  "Segoe UI",
  system-ui,
  sans-serif;

--font-mono:
  "Cascadia Mono",
  "Cascadia Code",
  "Consolas",
  ui-monospace,
  monospace;
```

For the web surface, body copy may use a slightly larger responsive size than the native 11–12 px editor chrome because long-form documentation must remain comfortable to read.

This is an adaptation of the same typography language, not literal pixel duplication.

### 6.5 Effective UI metrics

Important audit result:

`EditorTheme::Metrics()` still contains older/default values, while the final validated `EditorShellV3::Layout_()` uses newer effective metrics after Phase 14.

Current effective shell baseline:

| Metric | Effective value |
|---|---:|
| top menu height | 36 px |
| toolbar height | 48 px |
| status height | 24 px |
| panel gap | 7 px |
| panel header height | 29 px |
| main toolbar button height | 34 px |
| toolbar ordinary horizontal gap | 4 px |
| extra group separation | +10 px |
| content inner padding | 9 px |
| content search height | 30 px |
| content action button | 30 × 30 px |
| content action gap | 4 px |
| console custom scrollbar | ~8 px |
| button/input radius baseline | ~5–6 px |

Panel proportions in the active shell:

- left/top Scene Hierarchy: 20% width, clamped 250–330 px;
- right Inspector/Build: 22% width, clamped 285–355 px;
- central Viewport gets remaining primary width;
- bottom area targets ~34% height, clamped 205–280 px;
- Content Browser bottom column targets 31% width, clamped 345–495 px;
- Content Browser tree targets ~30% of its area, clamped 110–145 px.

**Design choice (not directly from the book):** web layout will preserve the *density, rhythm and hierarchy* of these metrics rather than force native-editor fixed pixel dimensions onto every browser viewport.

### 6.6 Token drift follow-up

The discrepancy between `EditorTheme::Metrics()` and effective `EditorShellV3::Layout_()` must not be duplicated into the website.

A later Web 1 implementation step should introduce a reviewed canonical design-token representation and then decide whether the C++ editor should consume generated values from that representation.

Do not rewrite the editor theme as part of website scaffolding without a dedicated regression pass.

### 6.7 Icons

Current Nocturne Editor icon contract:

- source vocabulary: **Tabler Icons 3.46.0**;
- license: MIT;
- source assets: `ThirdParty/TablerIcons/`;
- logical SVG geometry: 24 × 24;
- stroke: 2 px, round caps/joins;
- toolbar rendered glyph: 12 px in 16 px slot;
- compact header/tree/table glyph: 11 px.

**Design choice (not directly from the book):** the website will use the same Tabler semantic vocabulary. New generic web icons should first be selected from the vendored Tabler set. Custom art is reserved for Nocturne identity or concepts for which Tabler has no suitable semantic glyph.

Website icon CSS may render larger than 11–12 px where web hit targets require it, but stroke language and semantic mappings remain consistent.

---

## 7. Shared design-token target

### 7.1 Target model

**Design choice (not directly from the book):**

```text
Design/nocturne-theme.json
              |
        +-----+------+
        |            |
        v            v
 Web CSS tokens   C++ generated tokens
        |            |
     Website       Editor
```

The first implementation must treat this as a migration target, not assume it already exists.

### 7.2 Initial JSON shape

Target:

```json
{
  "schemaVersion": 1,
  "colors": {
    "windowBg": "#0A0F16",
    "panelBg": "#101720",
    "panelBgAlt": "#141C27",
    "viewportBg": "#0D1621",
    "toolbarBg": "#0E151E",
    "inputBg": "#0C131C",
    "buttonBg": "#151F2D",
    "buttonHover": "#1B283A",
    "border": "#222F3F",
    "textPrimary": "#DEE6F0",
    "textMuted": "#8495AB",
    "accent": "#1984EC",
    "accentHover": "#2A94FA",
    "success": "#4ECD70",
    "warning": "#E2A643",
    "danger": "#E85667"
  }
}
```

Before the editor consumes generated values, generated output must be compared against the current validated editor rendering behavior.

---

## 8. Global website shell

### 8.1 Product feel

**Design choice (not directly from the book):** the website should feel like the browser-native companion to the Nocturne Editor.

Target qualities:

- dark technical surface;
- thin/subtle borders;
- compact top navigation;
- restrained radius;
- clear hierarchy without oversized SaaS cards;
- sparse use of accent color;
- dense but readable technical content;
- Tabler icon language;
- keyboard-friendly interactions;
- code and diagrams treated as first-class content.

Avoid:

- generic purple SaaS gradients;
- oversized rounded cards everywhere;
- glassmorphism as a primary language;
- decorative motion that competes with technical content;
- marketing-heavy page length;
- visually unrelated light documentation theme.

### 8.2 Top navigation

Target desktop navigation:

```text
[Nocturne mark] NOCTURNE ENGINE      Docs   Download   Roadmap     Search   GitHub
```

Rules:

- top bar maps visually to the editor's top menu/tool strip;
- current route uses `accent`;
- hover uses `buttonHover`;
- navigation remains compact;
- search is available globally;
- docs can add their own secondary/sidebar navigation without duplicating the full site header.

### 8.3 Responsive behavior

**Design choice (not directly from the book):**

- Desktop-first because Nocturne is a desktop Windows engine/tool.
- Documentation still must remain readable on tablet/mobile.
- At narrow widths, sidebars collapse into accessible navigation drawers.
- Tables must scroll or adapt rather than overflow the page.
- Code blocks must remain horizontally scrollable.
- Download architecture cards stack vertically.
- Marketing layout simplifies before typography becomes unreadably small.

---

## 9. Landing page specification

### 9.1 Purpose

The landing page answers, quickly:

- What is Nocturne Engine?
- What platform/technology does it target?
- Where do I download it?
- Where is the documentation?
- What exists today?

### 9.2 Hero

Target copy hierarchy:

```text
NOCTURNE ENGINE

A custom C++ game engine built for first-person survival horror.

[ Download ]   [ Documentation -> ]

Windows · C++20+ · DirectX 12
```

The exact public copy may be revised later without changing the layout contract.

### 9.3 Hero visual

**Design choice (not directly from the book):** use current Nocturne Editor imagery/logo rather than abstract generated graphics.

Preferred visual hierarchy:

1. actual editor screenshot;
2. Nocturne logo/mark;
3. optional subtle technical grid/viewport motif.

Do not fabricate screenshots of functionality that does not exist.

### 9.4 Capability sections

Landing sections should remain few and meaningful.

Candidate current categories:

- Runtime;
- Rendering;
- Resources / Asset Pipeline;
- Editor;
- Documentation / Tooling.

Every capability claim must be backed by current code/completion documentation.

Phase history is not marketing proof of a feature if later code removed/replaced it.

---

## 10. Downloads page specification

### 10.1 Source of truth

**Design choice (not directly from the book):** download availability is data-driven.

The page must not hardcode architectures as if they are supported.

Target data flow:

```text
Build / CI
   |
   +--> release artifacts
   +--> checksums
   +--> release metadata
             |
             v
        releases.json
             |
             v
       /download page
```

### 10.2 Current support rule

The documentation shows Windows x64 build/run validation in existing completed editor phases.

The download page may show a public x64 artifact only when the release pipeline actually produces and publishes that artifact.

ARM64, x86 or other architectures must not be presented as downloadable/supported solely because they are theoretically compilable.

### 10.3 Release manifest target

```json
{
  "schemaVersion": 1,
  "latest": "0.1.0",
  "releases": [
    {
      "version": "0.1.0",
      "channel": "development",
      "publishedAt": "YYYY-MM-DD",
      "notesUrl": "/changelog/0.1.0",
      "builds": [
        {
          "platform": "windows",
          "architecture": "x86_64",
          "configuration": "release",
          "status": "supported",
          "file": "nocturne-engine-0.1.0-windows-x64.zip",
          "bytes": 0,
          "sha256": "..."
        }
      ]
    }
  ]
}
```

The actual schema will be versioned under `Schemas/release.schema.json`.

### 10.4 UI state semantics

- supported/validated: `success`;
- experimental: `warning`;
- unavailable: muted, not a fake download action;
- failed/withdrawn build: `danger` only when showing a historical diagnostic state.

Each artifact should expose:

- version;
- platform;
- architecture;
- channel;
- file size;
- checksum;
- release notes;
- download action.

---

## 11. Documentation page specification

### 11.1 Layout

Desktop target:

```text
+-------------------+--------------------------------------+------------------+
| Docs sidebar      | Main article                         | On this page     |
|                   |                                      |                  |
| Getting Started   | Title                                | Purpose          |
| Architecture      | Summary                              | Architecture     |
| Engine            |                                      | Lifecycle        |
| Editor            | Diagram / prose / code               | Threading        |
| Tools             |                                      | API              |
| Reference         |                                      |                  |
+-------------------+--------------------------------------+------------------+
```

This deliberately echoes the editor's left navigation / dominant center / right inspector composition without literally cloning its native window layout.

### 11.2 Information architecture

Target documentation navigation:

```text
Getting Started
  Introduction
  Building Nocturne
  Repository Structure

Architecture
  Overview
  Dependency Rules
  Runtime Lifecycle
  Architecture Diagram
  Architecture Decisions

Engine
  Core
  Platform
  Runtime
  Resources
  Jobs
  Input
  Rendering
  Scene / World
  Entity / Components

Editor
  Architecture
  Shell
  Viewport
  Scene Editing
  Content Browser
  Console

Tools
  Asset Import
  Cooker
  Packager

API Reference
  C++ API

Development
  Production Engineering Standard
  Testing
  Debugging
  Build Configurations

Development History
  Phase 1
  ...
```

Only sections backed by current documentation are promoted as canonical.

### 11.3 Article structure

Canonical system pages should use a stable structure where relevant:

```text
Purpose
Responsibilities
Non-Responsibilities
Architecture
Ownership
Lifecycle
Threading Model
Dependencies
Data Flow
Public API
Failure Handling
Diagnostics
Performance
Examples
Design Decisions
Source Files
Book Grounding
Related Documentation
```

Not every heading is mandatory for every subsystem. Empty ritual sections are worse than concise accurate pages.

### 11.4 Search

**Design choice (not directly from the book):** use Starlight/Pagefind static full-text search initially.

Desired UX:

- `Ctrl+K` / `Cmd+K` opens search where appropriate;
- modal styling uses Nocturne `inputBg`, `panelBg`, `border`, `accent`;
- results show section/context;
- historical phase results are visually identified as history when metadata permits;
- canonical pages should rank above historical pages when reasonable.

---

## 12. Machine-readable / AI-readable layer

### 12.1 Principle

The web UI is a presentation layer.

AI agents should not need to scrape rendered navigation HTML to understand Nocturne.

Target relationship:

```text
Canonical Markdown -----------------------+
     |                                    |
     +--> Human website                   |
     +--> Search                          |
     +--> llms exports                    |
     +--> manifest -----------------------+--> AI/API consumers
Structured Knowledge YAML/JSON -----------+
Schemas ----------------------------------+
Doxygen XML ------------------------------+
```

### 12.2 Stable identifiers

**Design choice (not directly from the book):** canonical concepts receive stable IDs independent of filenames.

Examples:

```text
noc.core
noc.platform
noc.runtime
noc.resources.vfs
noc.resources.manager
noc.jobs
noc.input
noc.render
noc.render.dx12
noc.world
noc.ecs
noc.editor
noc.editor.viewport
```

Renaming a page must not silently change the semantic ID.

### 12.3 Knowledge manifest

Target:

```json
{
  "schemaVersion": 1,
  "project": "Nocturne Engine",
  "documents": [
    {
      "id": "noc.architecture.overview",
      "path": "Docs/Architecture/Overview.md",
      "type": "architecture",
      "canonical": true
    },
    {
      "id": "noc.phase.14",
      "path": "Docs/Phases/Phase-14.md",
      "type": "historical-phase",
      "canonical": false
    }
  ]
}
```

### 12.4 `llms.txt`

Target responsibilities:

- describe Nocturne briefly;
- identify canonical architecture;
- identify canonical system docs;
- identify historical phase docs;
- tell AI agents which source wins on conflict;
- expose stable machine-readable endpoints;
- state core repository rules.

Minimal policy concept:

```text
Read canonical Architecture and Systems documents before historical Phase documents.
Use Phase documents to understand implementation history, not to override current behavior.
Inspect referenced source files when exact implementation behavior matters.
Do not introduce upward engine dependencies.
Engine provides mechanisms; applications provide policies.
```

### 12.5 `llms-full.txt`

Generated from canonical documentation only by default.

It should not blindly concatenate every historical phase report, log or obsolete implementation note.

### 12.6 Raw-source affordances

Documentation UI should eventually provide:

- **View Markdown**
- **Copy Markdown**
- **Copy for AI**
- **Edit on GitHub** where appropriate

"Copy for AI" should prefer clean canonical Markdown + metadata rather than copied rendered HTML.

---

## 13. C++ API documentation

### 13.1 Separation of concerns

Conceptual documentation and API reference are not the same artifact.

```text
Docs/System page:
  Why does ResourceManager exist?
  Who owns it?
  What is its lifecycle?
  How should systems use it?

API Reference:
  What is the exact signature of Request()?
  What are the parameter/return types?
  Where is the declaration?
```

### 13.2 Doxygen integration target

**Design choice (not directly from the book):**

```text
C++ public/internal documented headers
              |
              v
           Doxygen
          /       \
      HTML         XML
       |            |
       v            v
 Web API pages    AI/tooling index
```

Doxygen integration is a later milestone. It must not block the initial human documentation site.

---

## 14. Architecture diagrams

### 14.1 Existing source

The project already contains the full Nocturne architecture diagram in PDF/VSDX form.

### 14.2 Web target

**Design choice (not directly from the book):** provide two levels:

1. simplified responsive architecture overview in docs;
2. full interactive/detail view for the complete architecture.

Future enhancements may include:

- pan;
- zoom;
- subsystem search;
- click a subsystem to open its documentation;
- highlighting dependency direction.

Diagram data should prefer text/structured representations that can be versioned and indexed, while retaining the authoritative source artifact where necessary.

---

## 15. Accessibility and interaction quality

**Design choice (not directly from the book):** professional web quality includes keyboard and accessibility behavior.

Minimum contract:

- semantic landmarks and heading order;
- visible keyboard focus using Nocturne accent;
- sufficient contrast for text and controls;
- icon-only controls have accessible names;
- hover is never the only indication of state;
- reduced-motion preference is respected;
- navigation/search work by keyboard;
- code copy controls are focusable;
- links are identifiable beyond color alone where context is ambiguous.

The visual target is dark/compact, not low-legibility.

---

## 16. Performance contract

**Design choice (not directly from the book):**

- static HTML wherever possible;
- no large frontend SPA runtime for documentation;
- hydrate only interactive islands that need client state;
- optimize editor screenshots before shipping;
- SVG icons remain vector where practical;
- avoid loading the entire architecture visualization bundle on every docs page;
- keep search static/local initially;
- no third-party analytics in the foundation milestone.

Performance must be measured once representative content exists.

---

## 17. Security / trust boundary

The website is static, but its build still consumes repository data.

Rules:

- never embed GitHub/private build secrets into generated client output;
- sanitize/escape generated release metadata;
- validate release manifests against schema;
- validate paths before copying documentation;
- never execute documentation text as build code merely because it is inside a repository file;
- MDX is allowed only in trusted authored documentation, not arbitrary external content;
- checksum values are generated from release artifacts, not manually invented.

---

## 18. Versioning strategy

### 18.1 Website versioning

The website itself does not need a visible independent product version initially.

### 18.2 Engine documentation versions

**Design choice (not directly from the book):** start with documentation for current `master`.

Do not implement multi-version docs until there are public releases whose older API/documentation must remain accessible.

When introduced, versioning must define:

- current/latest docs;
- archived release docs;
- canonical latest machine-readable manifest;
- links from download release -> matching docs/changelog.

---

## 19. Web implementation milestones

This track is separate from engine Phases 1–31.

### Web 1 — Design contract and token extraction

Scope:

- audit current editor;
- lock current color/type/icon baseline;
- identify effective metric drift;
- write this specification;
- define the shared-token migration target.

**Status:** COMPLETE for the foundation scope. `Design/nocturne-theme.json` now exists and the website generates CSS variables from it. The C++ editor still uses its existing theme implementation; migrating the editor to generated tokens remains deliberately deferred until a dedicated visual-regression pass.

### Web 2 — Astro + Starlight skeleton

Scope:

- create `Website/`;
- pin dependency versions;
- create Astro/Starlight configuration;
- add Nocturne global CSS tokens;
- establish `/`, `/download`, `/docs`;
- add deterministic docs synchronization;
- add local dev/build scripts.

Acceptance:

- [x] dependency installation succeeds in GitHub Actions;
- [x] local-development command path is configured through `npm run dev`;
- [x] static production build succeeds;
- [x] `/`, `/download` and `/docs` static outputs are verified in CI;
- [x] repository-root `Docs/` is synchronized before dev/build;
- [x] the existing editor logo is copied as a generated website asset;
- [x] the Nocturne theme is generated from `Design/nocturne-theme.json`;
- [x] no engine build dependency on Node was introduced.

**Status:** COMPLETE for the skeleton scope. See `Docs/Web/Nocturne Website — Web 2 Implementation Report.md`.

### Web 3 — Documentation foundation

Scope:

- [x] Nocturne Starlight shell;
- [x] curated docs sidebar/TOC;
- [x] Pagefind static search;
- [x] code highlighting retained through Starlight/Expressive Code;
- [x] current repository-root docs ingestion;
- [x] canonical/history visual distinction;
- [x] first canonical Architecture/System pages;
- [x] stable Nocturne document IDs;
- [x] frontmatter/schema validation;
- [x] deterministic target/ID collision validation;
- [x] canonical route/ID CI checks.

**Status:** COMPLETE. See `Docs/Web/Nocturne Website — Web 3 Implementation Report.md`.

### Web 4 — Landing

Scope:

- [x] production landing layout;
- [x] current Nocturne logo and EditorShellV3-derived visual language;
- [x] implemented capability sections linked to canonical system docs;
- [x] responsive behavior for desktop/tablet/mobile breakpoints;
- [x] Documentation and Downloads CTA routes;
- [x] accessibility shell improvements (skip link, focus path, active navigation state);
- [x] shared vendored Tabler icon vocabulary;
- [x] explicit artifact-honest Downloads status;
- [x] no unsupported roadmap feature claims;
- [x] landing-specific CI assertions.

**Status:** COMPLETE. See `Docs/Web/Nocturne Website — Web 4 Implementation Report.md`.

No real editor screenshot asset currently exists in the repository or available Project image context. Web 4 therefore uses the real Nocturne logo plus an explicitly labeled **EditorShellV3 interface schematic** derived from the audited current editor layout. It is not represented as a screenshot.

### Web 5 — Downloads

Scope:

- [x] versioned `Schemas/release.schema.json`;
- [x] executable release-manifest validation before dev/build;
- [x] reviewed `releases.json` promotion workflow;
- [x] artifact-driven architecture/platform rendering;
- [x] exact byte size + SHA-256 metadata;
- [x] development / preview / stable channels;
- [x] release-notes links and source commit identity;
- [x] supported / experimental / withdrawn artifact states;
- [x] Windows Ship packaging workflow;
- [x] x86_64 Ship build/package validation;
- [x] x86 Ship build/package validation;
- [x] tagged GitHub Release publication path;
- [x] CI fixture exercising the non-empty Downloads rendering path;
- [x] honest zero-artifact public state until a real tagged release is promoted.

**Status:** COMPLETE. See `Docs/Web/Nocturne Website — Web 5 Implementation Report.md`.

Current evidence proves that Windows `x86_64` and `x86` can both complete the Web 5 Ship build/package gate. This is **build/package capability**, not yet a claim that either architecture has a public release. `Website/src/data/releases.json` remains empty until a real published artifact is reviewed and promoted.

### Web 6 — AI knowledge layer

Scope:

- [x] stable `noc.*` IDs reused as machine identity;
- [x] generated repository-root `Knowledge/`;
- [x] versioned canonical document manifest;
- [x] generated terminology with aliases/deprecated aliases;
- [x] generated `llms.txt`;
- [x] generated canonical-only `llms-full.txt`;
- [x] per-document `/raw/<noc.id>.md` exports;
- [x] per-document `/ai/<noc.id>.md` normalized exports;
- [x] View Markdown / Copy Markdown / Copy for AI affordances;
- [x] JSON Schema + executable validation;
- [x] repository-level stale generated-output detection;
- [x] canonical-vs-history precedence preserved for machine consumers.

**Status:** COMPLETE. See `Docs/Web/Nocturne Website — Web 6 Implementation Report.md`.

The machine-readable layer is generated from the same canonical `Docs/` metadata pass that feeds Starlight. `Knowledge/*.json` and `llms*.txt` are generated artifacts, not a second manually maintained documentation source.

### Web 7 — C++ API reference

Scope:

- [x] repository-owned Doxygen configuration;
- [x] current Engine / Editor / Host header extraction;
- [x] Nocturne-styled generated HTML at `/api/`;
- [x] Doxygen XML at `/api-xml/`;
- [x] generated Doxygen tag file;
- [x] stable qualified-name redirect routes under `/api-symbol/`;
- [x] generated `/api-symbols.json` symbol index;
- [x] canonical `api_symbols` metadata;
- [x] conceptual system pages → C++ symbol links;
- [x] machine-readable `Knowledge/manifest.json -> apiSymbols`;
- [x] dedicated hosted API-docs CI gate;
- [x] generated API artifact upload.

**Status:** COMPLETE. See `Docs/Web/Nocturne Website — Web 7 Implementation Report.md`.

Validated extraction currently covers **97 Nocturne headers** and indexes **147 Doxygen compounds**. Vendored `d3dx12.h`, test directories and the deprecated `EditorShell` / `EditorControls` surfaces are excluded from the current API reference.

### Web 8 — CI, deployment and quality gates

Scope:

- web build in CI;
- link validation;
- schema validation;
- stale generated-doc detection;
- accessibility baseline;
- performance baseline;
- static deployment target;
- release-page artifact integration.

---

## 20. Verification checklist for the website foundation

Before the Website track can be treated as a durable product surface:

- [x] `Website/` builds independently of engine compilation.
- [x] Engine/editor builds do not depend on the website toolchain.
- [x] `Docs/` remains the human-authored documentation source.
- [x] Generated Starlight docs are deterministic and not manually edited.
- [x] Current canonical docs are distinguishable from phase history.
- [x] Landing, Download and Docs share the same Nocturne visual tokens.
- [x] Website colors match the audited editor palette.
- [x] Typography follows the Nocturne font vocabulary with safe local/system fallbacks.
- [x] Tabler remains the shared generic icon vocabulary.
- [x] Final web design does not expose generic default Starlight styling as the primary brand.
- [x] Documentation search works in the static build.
- [x] Download page never exposes an architecture without a real release artifact.
- [x] Release metadata validates against a schema.
- [x] AI entry points identify canonical versus historical sources.
- [x] Generated C++ API HTML/XML exists and canonical conceptual pages link to validated symbols.
- [ ] No private credentials/paths appear in static output.
- [ ] Keyboard navigation and focus states are functional.
- [ ] Responsive docs remain readable at narrow widths.
- [ ] Broken internal docs links fail validation.
- [x] Build output is static and deployable without a permanent application server.

---

## 21. Common pitfalls

- Copying old `EditorTheme::Metrics()` values without checking effective `EditorShellV3` layout.
- Making website colors "close enough" instead of deriving them from the editor baseline.
- Editing generated `Website/src/content/docs/` copies by hand.
- Letting Phase documents become the current architecture source by accident.
- Building a SPA when static HTML is enough.
- Hardcoding x64/ARM64 cards without release artifacts.
- Treating Doxygen output as a substitute for conceptual documentation.
- Creating a second icon vocabulary for the website.
- Vendoring proprietary font files instead of using local/system font stacks.
- Copying the native editor's tiny 11–12 px text literally into long-form browser reading.
- Pulling Node/Astro dependencies into C++ engine build targets.
- Generating `llms-full.txt` from every historical file without canonical filtering.
- Allowing machine-readable metadata and Markdown to become two manually maintained truths.
- Redesigning the editor while attempting to build the website.
- Publishing claims about planned engine systems as if they are already implemented.

---

## 22. Decisions locked by this specification

Unless deliberately revised in a later Web ADR/spec:

1. **`Docs/` remains the authored documentation source.**
2. **The website is an external static consumer, never an engine runtime dependency.**
3. **Astro + Starlight is the initial website/docs implementation stack.**
4. **Pagefind is the initial static documentation search implementation.**
5. **Landing and Downloads are custom Astro pages.**
6. **Documentation uses a customized Starlight surface.**
7. **Nocturne Editor's audited palette is the website palette baseline.**
8. **Tabler is the shared generic icon vocabulary.**
9. **Current Architecture/System docs outrank historical Phase docs for current behavior.**
10. **Download architecture support is generated from actual release artifacts.**
11. **AI consumers receive raw/structured sources rather than being expected to scrape rendered HTML.**
12. **A future shared design-token source must eliminate, not multiply, token drift.**

All twelve items above are **Design choice (not directly from the book)** except where they preserve an already locked Nocturne dependency/source-of-truth rule grounded elsewhere in the project architecture.

---

## 23. Next implementation handoff

Start Web 8 with:

> Implement **Web 8 — CI, Deployment and Quality Gates** on branch `web-docs-foundation` on top of the validated Web 7 API reference, Web 6 AI knowledge layer and Web 5 release/download contract. Consolidate the website's production gates: broken internal-link validation, static-output privacy/secret/path auditing, automated accessibility baseline, responsive-browser checks, performance budget, production `site`/sitemap configuration, static deployment target and final release-page artifact integration. Preserve the current Nocturne visual/product contract, canonical-vs-history precedence and artifact honesty. Do not publish a fake engine release, do not make Website an engine/runtime dependency, and do not merge draft PR #2 automatically.

Bring:

- this specification;
- `Docs/Web/Nocturne Website — Web 7 Implementation Report.md`;
- `Docs/Development/Documentation Model.md`;
- `Docs/Development/C++ API Reference.md`;
- `Docs/Development/Release Process.md`;
- current Website / API Docs / Windows / Release workflows;
- current `Website/dist` route contract;
- current `Knowledge/`, `Schemas/` and Doxygen outputs;
- `Docs/Production Engineering Standard.md`.

