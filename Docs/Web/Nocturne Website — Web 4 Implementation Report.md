# Nocturne Website — Web 4 Implementation Report

> **Status:** COMPLETE
>
> **Track:** Nocturne Website
>
> **Milestone:** Web 4 — Landing
>
> **Branch:** `web-docs-foundation`
>
> **Specification:** `Docs/Web/Nocturne Website — Design & Technical Specification.md`
>
> **Implementation validation head before report:** `e8e811bf06fc939aac41fdffdd9312d0c6553bbf`
>
> **Validated push run:** `35467671724` — SUCCESS
>
> **Validated pull-request run:** `35467674642` — SUCCESS

---

## 1. Objective

Web 4 turns the functional Web 2 landing page into the public product surface for Nocturne Engine.

The landing now communicates:

- what Nocturne currently is;
- which engine systems are actually implemented;
- how the high-level runtime/data flow is structured;
- where canonical documentation lives;
- what the current public-download state is;
- how the product visual language relates to the native editor.

The page deliberately avoids presenting future roadmap systems as shipped features.

---

## 2. Source grounding

Landing claims were checked against:

- `Docs/nocturne_engine_architecture.md`;
- the canonical Web 3 system pages:
  - `Docs/Systems/Runtime.md`;
  - `Docs/Systems/Resources.md`;
  - `Docs/Systems/Rendering.md`;
  - `Docs/Systems/World & ECS.md`;
  - `Docs/Systems/Editor.md`;
- Phase 14/15 completion state already incorporated into those canonical pages;
- current `Apps/NocturneEditor/EditorShellV3.cpp`;
- current `Apps/NocturneEditor/EditorTheme.cpp`;
- `Design/nocturne-theme.json`.

No landing copy claims Physics, Animation, Audio, Lighting/PBR, Scripting, AI/navigation, PIE or shipping/release systems as already implemented.

Those remain roadmap work.

---

## 3. Book grounding

The website presentation itself remains **Design choice (not directly from the book)**.

The engine concepts shown by the landing are grounded by the same supplied sources used by the canonical system documentation, especially Jason Gregory's discussions of:

- runtime/game-loop subsystem servicing;
- resource management;
- rendering pipelines;
- world/game-object architecture;
- game world editors and tooling.

The landing does not create new engine architecture. It presents existing Nocturne contracts.

---

## 4. Production landing structure

`Website/src/pages/index.astro` now provides five major surfaces.

### 4.1 Hero

The hero communicates:

```text
Windows
C++20+
DirectX 12
first-person survival horror target
```

Primary actions:

- Documentation;
- Downloads.

The headline remains:

`Built for the dark.`

### 4.2 Current development status

A narrow product-status strip exposes:

- Phases 1–15 as the implemented foundation;
- current scene-editing/runtime-reflection work as active engine development;
- direct link to the canonical roadmap.

This reflects the current architecture document rather than deriving state from old phase history.

### 4.3 Implemented-system cards

The landing exposes the five current canonical Web 3 systems:

```text
Runtime       noc.runtime
Resources     noc.resources
Rendering     noc.render
World & ECS   noc.world
Editor        noc.editor
```

Every card links directly to its current canonical documentation page.

The cards are labeled `Implemented`, because their current canonical docs and completion evidence support that status.

### 4.4 Architecture flow

The landing provides a concise visual flow:

```text
Application policy
       ↓
Engine Runtime
       ↓
World & ECS
       ↓
Render Queue
       ↓
DirectX 12
```

A secondary service rail identifies:

- Resources / VFS;
- Jobs;
- Input;
- Editor client.

This is a presentation of existing dependency/ownership rules, not a new engine subsystem diagram.

### 4.5 Documentation and downloads

A Nocturne Docs preview demonstrates:

- canonical authority badges;
- stable `noc.*` IDs;
- system navigation;
- search affordance.

The Downloads section remains artifact-honest:

```text
Current development target: Windows · x86_64
Public release: Not published
```

No downloadable artifact or additional architecture is fabricated.

---

## 5. EditorShellV3-derived visual

The old simplified hero mock was replaced with a substantially more faithful EditorShellV3 interface schematic.

The schematic includes the current editor vocabulary:

- Nocturne menu bar;
- 48 px-style toolbar region;
- selection/move/rotate/scale tools;
- Play control;
- Scene Hierarchy;
- Viewport;
- Inspector;
- Content Browser;
- Console;
- 24 px-style status region;
- validation entities;
- selected object/gizmo representation.

The schematic uses the audited Nocturne product tokens rather than generic website colors.

### 5.1 Screenshot integrity

No real Nocturne Editor screenshot asset was found in:

- the repository;
- available Project image search context.

Web 4 does **not** fabricate an AI-generated screenshot and present it as the product.

Instead, the landing explicitly labels the visual:

`Interface schematic · derived from the current editor layout`

The actual `NocturneEngine-Logo.png` remains the real branded image asset.

This is an intentional truthfulness constraint.

---

## 6. Shared Tabler icon vocabulary

Web 4 reuses the Tabler icon source already vendored by Nocturne.

`Website/scripts/prepare.mjs` now copies the required landing subset from:

`ThirdParty/TablerIcons/icons/outline/`

Generated website icons:

```text
box.svg
device-desktop.svg
world.svg
hierarchy-2.svg
layout-grid.svg
terminal-2.svg
file-text.svg
```

The generated output lives in:

`Website/public/icons/`

and is ignored by Git because the repository source remains `ThirdParty/TablerIcons`.

No second generic icon library was introduced.

---

## 7. Public-site accessibility shell

`Website/src/layouts/SiteLayout.astro` now includes:

- a keyboard-visible `Skip to content` link;
- `main#main-content`;
- explicit focus target behavior;
- active navigation state via `aria-current`;
- dark color-scheme metadata;
- existing global focus-visible styling.

These changes apply to Landing, Downloads and the custom public 404 shell.

A more comprehensive automated accessibility audit remains Web 8 scope.

---

## 8. Responsive behavior

The landing defines explicit layout changes for:

- wide desktop;
- standard desktop/tablet;
- narrow tablet;
- mobile;
- small mobile.

Representative behavior:

### Wide desktop

- split hero;
- full EditorShellV3 schematic;
- multi-column capability grid;
- horizontal architecture flow;
- side-by-side Docs and Downloads sections.

### Tablet

- hero stacks;
- capability cards move to two columns;
- architecture flow becomes vertical;
- docs preview stacks below copy.

### Mobile

- nonessential editor schematic panels are progressively hidden;
- Viewport remains the central visual;
- primary actions become full-width on small screens;
- capability cards become one column;
- documentation sidebar preview is omitted;
- release metadata becomes vertically stacked.

No JavaScript is required for layout adaptation.

---

## 9. Motion policy

The landing relies primarily on static layout and native hover/focus changes.

The global and landing CSS retain `prefers-reduced-motion` handling.

No decorative animation framework was introduced.

---

## 10. CI landing contract

The Web CI gate was expanded beyond route existence.

It now verifies that the generated static build contains the seven Tabler assets.

It also verifies the landing HTML contains:

```text
EditorShellV3
Interface schematic
Phases 1–15 implemented
noc.runtime
noc.resources
noc.render
noc.world
noc.editor
Skip to content
```

This gives the landing a small but explicit generated-output contract.

---

## 11. Validation

Final pre-report Web 4 validation:

### Push

Run:

`35467671724`

Result:

`SUCCESS`

### Pull request

Run:

`35467674642`

Result:

`SUCCESS`

Validated implementation head:

`e8e811bf06fc939aac41fdffdd9312d0c6553bbf`

The build logged:

```text
44 docs synchronized
9 canonical IDs
logo copied
7 Tabler icons copied
```

Pagefind built its static documentation search index successfully.

---

## 12. Known non-blocking warning

The existing sitemap warning remains:

```text
[@astrojs/sitemap] The Sitemap integration requires the site astro.config option. Skipping.
```

A fake public URL is still not introduced merely to remove this warning.

Production URL/sitemap configuration remains deployment scope.

---

## 13. Verification checklist

- [x] production landing layout exists;
- [x] page uses the real Nocturne logo;
- [x] editor visual derives from current EditorShellV3/EditorTheme;
- [x] schematic is explicitly labeled and not represented as a screenshot;
- [x] current implemented systems are linked to canonical docs;
- [x] stable canonical IDs appear on the landing;
- [x] architecture flow reflects current ownership/data boundaries;
- [x] no future roadmap systems are presented as completed features;
- [x] current Downloads state is honest;
- [x] Tabler remains the generic icon source;
- [x] generated icon assets are ignored in Git;
- [x] keyboard skip-link exists;
- [x] active public navigation exposes `aria-current`;
- [x] responsive CSS covers desktop/tablet/mobile layouts;
- [x] reduced-motion handling remains present;
- [x] static build passes;
- [x] landing-specific CI assertions pass;
- [x] push CI passes;
- [x] PR CI passes.

---

## 14. Deliberately deferred scope

### Real editor screenshot

A real current editor capture can replace or supplement the schematic once a curated screenshot is added as an intentional product asset.

No fabricated image is used in its place.

### Web 5 — Downloads

Owns:

- release metadata JSON Schema;
- version/channel model;
- published artifact records;
- checksums;
- real architecture cards;
- release-note links;
- build/release integration.

### Web 6 — AI Knowledge Layer

Owns:

- AI entry points;
- canonical knowledge manifest;
- terminology;
- `llms.txt`;
- `llms-full.txt`;
- raw/copy-for-AI affordances.

### Web 7 — API Reference

Owns Doxygen and symbol-level reference generation.

### Web 8 — Deployment/quality

Owns:

- production domain;
- sitemap;
- deployment;
- broken-link validation;
- automated accessibility baseline;
- web performance budget.

---

## 15. Completion statement

**Web 4 — Landing is COMPLETE for its defined scope.**

The Nocturne public home is now an engine-specific product surface rather than a generic placeholder:

- it uses the actual Nocturne visual vocabulary;
- it presents the current editor honestly;
- it routes directly into canonical documentation;
- it distinguishes implemented engine systems from future roadmap work;
- it preserves artifact honesty for Downloads;
- it has static CI assertions for its key product contract.

The next milestone is **Web 5 — Downloads**.

---

## 16. Next chat handoff

Say:

> Continue with **Web 5 — Downloads** on branch `web-docs-foundation`. Read the Website specification plus the Web 3 and Web 4 implementation reports first. Preserve the canonical documentation model and Web 4 product shell. Define the release metadata schema, render only real published artifacts, add version/channel/checksum/release-note metadata, and integrate it with the current Windows build/release workflow. Keep the empty state until a real public artifact exists. Do not fabricate ARM64/x86 support and do not merge draft PR #2 automatically.
