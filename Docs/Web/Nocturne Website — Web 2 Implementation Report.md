# Nocturne Website — Web 2 Implementation Report

> **Status:** COMPLETE
>
> **Track:** Nocturne Website
>
> **Milestone:** Web 2 — Astro + Starlight Skeleton
>
> **Branch:** `web-docs-foundation`
>
> **Base audited:** `master` @ `91feb6411910a398827792b20059e7c275b4983c`
>
> **Specification:** `Docs/Web/Nocturne Website — Design & Technical Specification.md`

---

## 1. Objective

Web 2 establishes the production-capable static foundation for the Nocturne Engine website without introducing any dependency from the C++ engine/editor back into the web toolchain.

The implemented primary routes are:

```text
/           Landing
/download   Downloads
/docs       Documentation
```

The milestone also establishes the first shared Nocturne web design-token source and deterministic synchronization from repository-root `Docs/` into the Starlight content collection.

---

## 2. Book grounding

Jason Gregory, *Game Engine Architecture (3rd Edition)*, §1.7 treats tools and the asset pipeline as an important part of the game-engine development environment. Gregory §15.4 and §15.4.1 describe the game world editor and standard editor facilities used to inspect, navigate and manipulate the game world.

Those sections ground the importance of coherent and reliable engine tooling.

The following Web 2 decisions are **Design choice (not directly from the book)**:

- Astro as the static site framework;
- Starlight as the documentation UI;
- Pagefind as the documentation search implementation;
- the exact route structure;
- CSS custom properties;
- repository-to-Starlight synchronization;
- the web design-token JSON;
- responsive browser behavior;
- npm/Node dependency management;
- GitHub Actions web CI.

---

## 3. Implemented architecture

### 3.1 Repository boundary

Implemented:

```text
Engine / Editor / Tools / Docs
              |
              v
          Website build
              |
              v
         static output
```

No C++ project, runtime target or editor target depends on:

- Node.js;
- npm;
- Astro;
- Starlight;
- Pagefind;
- generated website assets.

This preserves the existing Nocturne dependency-direction contract.

### 3.2 Website project

Created:

```text
Website/
├── astro.config.mjs
├── package.json
├── package-lock.json
├── tsconfig.json
├── README.md
├── scripts/
│   └── prepare.mjs
├── src/
│   ├── content.config.ts
│   ├── content/
│   │   └── docs/
│   ├── data/
│   │   └── releases.json
│   ├── layouts/
│   │   └── SiteLayout.astro
│   ├── pages/
│   │   ├── index.astro
│   │   └── download.astro
│   └── styles/
│       └── nocturne.css
└── public/
    └── nocturne-logo.png        # generated during prepare
```

The generated logo and synchronized documentation are intentionally not separate authored sources.

---

## 4. Dependency versions

Direct dependencies are exactly pinned:

```json
{
  "@astrojs/starlight": "0.42.2",
  "astro": "7.3.3"
}
```

The committed `Website/package-lock.json` is npm lockfile version 3.

Node contract:

```text
>= 22.12.0
```

Local and CI installation now uses:

```text
npm ci
```

rather than unconstrained dependency resolution on every build.

---

## 5. Nocturne design-token foundation

Created:

`Design/nocturne-theme.json`

The file records the audited current Nocturne Editor baseline and is used to generate website CSS variables.

Current palette:

| Token | Value |
|---|---|
| `windowBg` | `#0A0F16` |
| `panelBg` | `#101720` |
| `panelBgAlt` | `#141C27` |
| `viewportBg` | `#0D1621` |
| `toolbarBg` | `#0E151E` |
| `inputBg` | `#0C131C` |
| `buttonBg` | `#151F2D` |
| `buttonHover` | `#1B283A` |
| `border` | `#222F3F` |
| `textPrimary` | `#DEE6F0` |
| `textMuted` | `#8495AB` |
| `accent` | `#1984EC` |
| `accentHover` | `#2A94FA` |
| `success` | `#4ECD70` |
| `warning` | `#E2A643` |
| `danger` | `#E85667` |

Typography vocabulary is also preserved through system/local font stacks:

- `Segoe UI Variable Text`;
- `Segoe UI Variable Display`;
- `Cascadia Mono`.

No font files were copied or vendored into the website.

Tabler remains the generic icon vocabulary contract.

### 5.1 Metric drift handling

The audit identified that old/default `EditorTheme::Metrics()` values do not fully represent the final accepted `EditorShellV3` layout.

Therefore `Design/nocturne-theme.json` records the effective current shell metrics, including:

- menu: 36 px;
- toolbar: 48 px;
- status: 24 px;
- panel gap: 7 px;
- panel header: 29 px;
- toolbar button: 34 px.

The editor itself has not yet been migrated to generated shared tokens.

That migration remains deferred until a dedicated visual-regression pass.

---

## 6. Deterministic documentation synchronization

Implemented:

`Website/scripts/prepare.mjs`

The script runs before both development and production builds.

Responsibilities:

1. read `Design/nocturne-theme.json`;
2. generate `Website/src/styles/generated/nocturne-theme.css`;
3. enumerate repository-root `Docs/**/*.md` deterministically;
4. map selected current documents to canonical routes;
5. map remaining phase/implementation documents into historical routes;
6. prevent two source files from silently generating the same destination;
7. generate a Development History index;
8. generate Starlight-compatible frontmatter;
9. mark historical pages as historical;
10. copy the existing editor logo from `Apps/NocturneEditor/Resources/NocturneEngine-Logo.png`;
11. record generated files so only generated content is removed on the next run.

Current explicitly canonical mappings include:

```text
Docs/nocturne_engine_architecture.md
  -> /docs/architecture/overview/

Docs/Production Engineering Standard.md
  -> /docs/development/production-engineering-standard/

Docs/Web/Nocturne Website — Design & Technical Specification.md
  -> /docs/development/website-specification/
```

Other current phase/implementation Markdown is presented under:

```text
/docs/history/...
```

This is an interim ingestion model. Web 3 owns the first purpose-built canonical System documentation pages.

### 6.1 Synchronization invariant

The source of truth remains:

```text
Docs/*.md
```

Generated copies under the website content tree must never be manually edited.

---

## 7. Landing page

Implemented a custom Astro landing route at:

`/`

The initial page includes:

- Nocturne brand/navigation shell;
- existing Nocturne logo asset;
- primary Documentation and Downloads actions;
- Windows / C++20+ / DirectX 12 technology line;
- an editor-inspired technical hero surface;
- current-foundation capability sections for Runtime, Rendering, Resources and Editor;
- a Documentation call-to-action.

The page deliberately avoids claiming planned roadmap systems as shipped features.

The current landing is a functional Web 2 foundation, not the final Web 4 polish pass.

---

## 8. Downloads page

Implemented:

`/download`

Source:

`Website/src/data/releases.json`

The initial release manifest is intentionally empty:

```json
{
  "schemaVersion": 1,
  "latest": null,
  "releases": []
}
```

The UI therefore reports that there is currently no public release artifact instead of presenting a fake download.

The page may state the current development validation target:

```text
Windows · x86_64
```

but clearly distinguishes internal validation from a public downloadable release.

ARM64, x86 and other architectures are not exposed until an actual release pipeline produces and validates those artifacts.

Web 5 owns the release schema and build/release integration.

---

## 9. Documentation route

Implemented:

`/docs`

The current Starlight surface provides:

- docs home;
- synchronized architecture documentation;
- production engineering standard;
- website specification;
- generated historical documentation section;
- Starlight navigation;
- Pagefind-capable static documentation build;
- Nocturne palette/typography override.

Web 3 owns deeper Nocturne-specific documentation UX, canonical subsystem pages and metadata schema.

---

## 10. CI validation

Created:

`.github/workflows/web-ci.yml`

Final workflow contract:

- `permissions: contents: read`;
- `actions/checkout@v7`;
- `actions/setup-node@v7`;
- Node 22;
- npm cache keyed from `Website/package-lock.json`;
- `npm ci --no-audit --no-fund`;
- `npm run build`;
- static route existence checks.

Verified files:

```text
Website/dist/index.html
Website/dist/download/index.html
Website/dist/docs/index.html
```

### 10.1 CI issue found and repaired

The first Starlight build exposed an API/configuration incompatibility:

```text
Support for autogenerated sidebar groups was removed in Starlight v0.39.0.
```

The configuration was corrected from the obsolete top-level `label + autogenerate` form to the current group form:

```js
{
  label: 'Documentation',
  items: [
    { autogenerate: { directory: 'docs', collapsed: true } }
  ]
}
```

This is why the website CI gate was introduced before calling Web 2 complete.

### 10.2 Validated runs

Successful locked-install validation:

- push run `35460865821` — success;
- pull-request run `35460868302` — success.

Both validated commit:

`712a11e7a243192a0f92fe1d1496646285a60a56`

The earlier transition run that generated and committed the dependency lock was temporary. The final workflow no longer has write permission.

---

## 11. Verification checklist

- [x] Dedicated `Website/` project exists.
- [x] Website builds without compiling the C++ engine.
- [x] Engine/editor projects have no website dependency.
- [x] Astro and Starlight direct versions are pinned.
- [x] `package-lock.json` is committed.
- [x] CI uses `npm ci`.
- [x] CI token is read-only in the final workflow.
- [x] Nocturne palette is derived from the audited editor baseline.
- [x] Existing Nocturne logo is reused instead of inventing a parallel logo.
- [x] Proprietary font files are not copied.
- [x] `Docs/` remains the authored documentation source.
- [x] Documentation synchronization is deterministic.
- [x] Generated-target collisions fail explicitly.
- [x] Historical documents are visually identified as historical.
- [x] `/` exists.
- [x] `/download` exists.
- [x] `/docs` exists.
- [x] Download UI does not fabricate unsupported/public artifacts.
- [x] Static production build succeeds in CI.
- [x] Primary generated routes are checked in CI.
- [x] Push CI is green.
- [x] Pull-request CI is green.

---

## 12. Deferred scope

The following work was deliberately not pulled into Web 2.

### Web 3 — Documentation Foundation

- purpose-built canonical system pages;
- stronger canonical-vs-history navigation;
- structured frontmatter validation;
- Nocturne-specific Starlight header/sidebar/search treatment;
- current subsystem documentation IA;
- link validation improvements.

### Web 4 — Landing

- final visual polish;
- real curated editor screenshot(s);
- final product copy;
- responsive polish;
- accessibility/visual QA pass.

### Web 5 — Downloads

- release JSON Schema;
- CI/release artifact generation;
- checksums;
- channels;
- real architecture matrix;
- release-note integration.

### Web 6 — AI Knowledge Layer

- stable document IDs;
- `Knowledge/`;
- `llms.txt`;
- `llms-full.txt`;
- terminology manifest;
- Copy/View Markdown affordances.

### Web 7 — C++ API Reference

- Doxygen generation;
- XML tooling output;
- conceptual-doc/API cross-links.

### Web 8 — Deployment and Quality Gates

- selected static hosting/deployment;
- broken-link gate;
- accessibility baseline;
- web performance budget;
- production release integration.

---

## 13. Common pitfalls confirmed during implementation

- Starlight configuration syntax can change between versions; CI must validate pinned versions.
- Editor theme constants cannot be treated as current layout truth without checking `EditorShellV3`.
- Generated documentation can silently collide if source filenames are normalized without collision checks.
- A Downloads UI can imply support that does not exist if architecture cards are hand-authored.
- Exact direct dependency pins are insufficient for full transitive reproducibility without the lockfile.
- Generated site content must never become a second manually maintained documentation source.

---

## 14. Completion statement

**Web 2 — Astro + Starlight Skeleton is COMPLETE for its defined scope.**

The repository now has a static, independently buildable Nocturne web surface with:

- a custom Nocturne landing page;
- an artifact-honest Downloads page;
- a Starlight documentation route;
- deterministic Markdown synchronization;
- editor-derived design tokens;
- reproducible locked npm dependencies;
- static CI validation.

The next milestone is **Web 3 — Documentation Foundation**.

---

## 15. Next chat handoff

Say:

> Continue with **Web 3 — Documentation Foundation** on branch `web-docs-foundation`. Read `Docs/Web/Nocturne Website — Design & Technical Specification.md` and `Docs/Web/Nocturne Website — Web 2 Implementation Report.md` first. Preserve repository-root `Docs/` as the authored source of truth. Build the Nocturne-specific Starlight documentation experience, canonical-vs-history navigation and metadata model, and the first canonical Architecture/System pages. Do not merge the draft PR yet and do not reorganize historical Phase documents destructively.
