# Nocturne Website — Web 7 Implementation Report

> **Status:** COMPLETE
>
> **Track:** Nocturne Website
>
> **Milestone:** Web 7 — C++ API Reference
>
> **Branch:** `web-docs-foundation`
>
> **Specification:** `Docs/Web/Nocturne Website — Design & Technical Specification.md`
>
> **Canonical API contract:** `Docs/Development/C++ API Reference.md`
>
> **Validated implementation head before completion docs:** `5d10dac616740cae2e8063c85926052e5c2e6ecf`
>
> **API Docs validation:** `35478825307` — SUCCESS

---

## 1. Objective

Web 7 adds the symbol-level C++ reference that Web 6 deliberately deferred.

The documentation stack now answers three distinct questions:

```text
Docs/ canonical prose
    -> why / ownership / lifecycle / architecture

Doxygen HTML
    -> what C++ symbols and members exist

Doxygen XML
    -> machine-readable symbol structure
```

Generated symbols do not replace canonical architectural documentation.

---

## 2. Book grounding

Jason Gregory, *Game Engine Architecture (3rd Edition)*, §1.7 grounds reliable development tooling and pipelines as part of the engine development ecosystem.

The exact API documentation tool and publication mechanism are not prescribed by the supplied engine books.

Therefore the following are **Design choice (not directly from the book)**:

- Doxygen;
- `EXTRACT_ALL` bootstrap policy;
- private/internal extraction;
- HTML and XML publication routes;
- stable symbol aliases;
- `api_symbols` metadata;
- GitHub-hosted API documentation CI;
- Doxygen visual theming.

---

## 3. Why Doxygen is separate from canonical docs

Canonical Nocturne documents continue to own:

- subsystem responsibilities;
- ownership;
- lifetime;
- initialization/shutdown;
- dependency direction;
- mechanism-vs-policy boundaries;
- rationale;
- source-of-truth precedence.

Doxygen owns generated symbol navigation.

A current internal member appearing in Doxygen is not, by itself, a new architectural contract or a compatibility promise.

---

## 4. Repository-owned Doxygen configuration

Created:

`Docs/API/Doxyfile`

The configuration generates from:

```text
Engine/
Apps/NocturneEditor/
Apps/NocturneHost/
Docs/API/MainPage.dox
```

Header patterns:

```text
*.h
*.hpp
*.inl
```

Current extraction enables:

- undocumented existing entities through `EXTRACT_ALL`;
- private/internal members for engine-maintainer visibility;
- source browsing/cross references;
- alphabetical compound index;
- searchable HTML;
- XML output;
- Doxygen tag file.

No Graphviz dependency is required by the current Web 7 scope.

---

## 5. Deliberate exclusions

Web 7 excludes:

- `ThirdParty/`;
- test directories;
- `Engine/Render/DX12/d3dx12.h`;
- `Apps/NocturneEditor/EditorShell.h`;
- `Apps/NocturneEditor/EditorControls.h`.

The DirectX helper is vendored code and must not look like Nocturne-owned API.

`EditorShellV3` is the current canonical editor shell; historical `EditorShell` / `EditorControls` are not promoted into the current API surface.

---

## 6. Generated human API

Static HTML route:

`/api/index.html`

Created visual override:

`Docs/API/nocturne-doxygen.css`

The reference uses a dark Nocturne-oriented palette while retaining Doxygen's native class/member navigation.

The main API page explicitly links back to the conceptual documentation and states the source-precedence rule.

---

## 7. Generated machine API

Doxygen XML route:

`/api-xml/index.xml`

The complete XML compound files are published under:

`/api-xml/`

Doxygen tag file:

`/api/nocturne.tag`

These outputs provide symbol-level structure for tooling that needs more precision than Markdown concepts alone.

---

## 8. Stable symbol aliases

Created by:

`Website/scripts/prepare-api.ps1`

The script parses Doxygen's generated `index.xml`.

For each class/struct/union/namespace compound, it creates a stable qualified-name redirect.

Examples:

```text
noc::Engine
    -> /api-symbol/noc.Engine.html

noc::ResourceManager
    -> /api-symbol/noc.ResourceManager.html

nocturne::editor::EditorShellV3
    -> /api-symbol/nocturne.editor.EditorShellV3.html
```

Each alias redirects to the actual Doxygen `refid` HTML page.

Therefore conceptual docs do not hardcode Doxygen's internal generated filename scheme.

---

## 9. Symbol machine index

Generated route:

`/api-symbols.json`

Each compound record contains:

- qualified name;
- compound kind;
- Doxygen refid;
- stable alias route;
- direct Doxygen HTML route.

Generated build metadata:

`/api-build.json`

records:

- generator;
- Doxygen version;
- documented-header count;
- compound count;
- HTML/XML/symbol/tag routes.

---

## 10. Canonical api_symbols metadata

Web 7 extends canonical source metadata with:

`api_symbols`

Example:

```yaml
api_symbols: ["noc::Engine","noc::MainLoop","noc::World"]
```

The relationship is authored once.

`Website/scripts/prepare.mjs` then projects it into:

1. the visible conceptual documentation page;
2. `Knowledge/manifest.json -> apiSymbols[]`;
3. AI-normalized canonical metadata.

This prevents separate manually maintained human and machine symbol maps.

---

## 11. Current conceptual symbol entry points

### Runtime

- `noc::Engine`
- `noc::MainLoop`
- `noc::World`

### Resources

- `noc::VirtualFileSystem`
- `noc::ResourceManager`
- `noc::ResourceHandle`

### Rendering

- `noc::RenderSystem`
- `noc::RenderQueue`
- `noc::Dx12Renderer`

### World & ECS

- `noc::World`
- `noc::EntityHandle`
- `noc::EntityRegistry`

### Editor

- `nocturne::editor::EditorShellV3`
- `nocturne::editor::EditorViewportController`
- `nocturne::editor::EditorTheme`

---

## 12. Canonical API documentation contract

Created:

`Docs/Development/C++ API Reference.md`

Stable ID:

`noc.development.cpp-api`

The current canonical knowledge set therefore grows from 10 to 11 documents.

The API page documents:

- generated/reference role;
- extraction scope;
- human/machine routes;
- stable aliases;
- internal/public policy;
- validation;
- conceptual-to-symbol relationship;
- source precedence.

---

## 13. Generator fixes exposed by stale validation

Web 7 exercised the Web 6 stale-output contract and found two documentation-generator defects.

### 13.1 Mapped descriptions

The three root canonical documents already had explicit mapped descriptions, but the sync pass replaced them with generic fallback text when no source frontmatter existed.

Fixed precedence:

```text
source frontmatter description
    ->
mapping description
    ->
historical/canonical fallback
```

### 13.2 Documentation Model API identity

An editing replacement accidentally placed the Resources API symbol example into the `Documentation Model` document's own frontmatter.

The stale manifest gate exposed the mismatch.

The canonical document now correctly has:

`api_symbols: []`

while its YAML example demonstrates the Resource symbols.

This is precisely the kind of drift the Web 6 stale-generation gate was designed to reject.

---

## 14. API generation scripts

Created:

```text
Website/scripts/prepare-api.ps1
Website/scripts/validate-api.ps1
```

`prepare-api.ps1`:

- locates Doxygen;
- clears prior generated output;
- runs the repository Doxyfile;
- verifies HTML/XML/tag output;
- parses XML compounds;
- validates required current symbols;
- creates stable alias redirects;
- writes `api-symbols.json`;
- writes `api-build.json`;
- copies HTML/XML into Website public assets.

`validate-api.ps1`:

- validates required generated files;
- validates stable aliases;
- parses the symbol index structurally;
- checks XML for representative Nocturne systems;
- rejects `EditorControls`;
- rejects vendored `d3dx12`.

---

## 15. Website commands

Added:

```text
npm run api
npm run validate:api
npm run build:api
```

Generated API public assets are ignored by Git.

They are rebuilt from source/header truth.

---

## 16. Dedicated API CI

Created:

`.github/workflows/api-docs-ci.yml`

This workflow uses `windows-latest`.

That does not alter the self-hosted runner policy of engine/Windows/release workflows.

The API workflow is tooling-only and independently installs Doxygen.

Pipeline:

```text
Checkout
  ->
Node
  ->
Doxygen
  ->
npm ci
  ->
generate API
  ->
Astro build
  ->
validate API
  ->
verify conceptual integration
  ->
upload API artifact
```

---

## 17. Validated result

Run:

`35478825307`

Result:

`SUCCESS`

Validated implementation head:

`5d10dac616740cae2e8063c85926052e5c2e6ecf`

Doxygen reported:

```text
Doxygen version: 1.10.0
Documented headers: 97
Indexed compounds: 147
```

Validated pipeline stages:

```text
Generate Doxygen API              SUCCESS
Build website with API assets     SUCCESS
Validate generated API            SUCCESS
Verify API integration            SUCCESS
Upload generated API artifact     SUCCESS
```

Uploaded Actions artifact:

`nocturne-cpp-api`

Artifact ID:

`10595386682`

Artifact size:

`2,879,492 bytes`

The artifact contains the generated HTML/XML/symbol outputs.

---

## 18. Official Doxygen behavior used

Web 7 relies on standard Doxygen capabilities:

- `EXTRACT_ALL` to expose an existing codebase before every entity has authored Doxygen prose;
- HTML generation for human browsing;
- XML generation for tooling.

These are Doxygen tool capabilities, not Nocturne engine architecture rules.

---

## 19. Verification checklist

- [x] repository-owned Doxyfile exists;
- [x] API-specific main page exists;
- [x] Nocturne Doxygen styling exists;
- [x] Engine headers are extracted;
- [x] Editor headers are extracted;
- [x] Host headers are extracted;
- [x] third-party helper code is excluded;
- [x] legacy editor shell surface is excluded;
- [x] HTML output is generated;
- [x] XML output is generated;
- [x] tag file is generated;
- [x] stable symbol aliases are generated;
- [x] symbol JSON index is generated;
- [x] build metadata is generated;
- [x] Runtime conceptual page links to current symbols;
- [x] Resources conceptual page links to current symbols;
- [x] Rendering conceptual page links to current symbols;
- [x] World & ECS conceptual page links to current symbols;
- [x] Editor conceptual page links to current symbols;
- [x] knowledge manifest includes API symbol relationships;
- [x] canonical API contract exists;
- [x] stale machine outputs are byte-for-byte synchronized;
- [x] dedicated API CI passes;
- [x] API artifact upload passes;
- [x] Website remains independent from engine/runtime compilation.

---

## 20. Deliberately deferred

### Authored per-symbol prose

Web 7 creates a useful structural reference immediately.

It does not attempt to add hundreds of superficial Doxygen comments merely to increase documentation percentage.

Subsystem work should add symbol-level comments when ownership/failure/usage semantics need more detail.

### External SDK compatibility surface

The current API includes private/internal data useful to Nocturne maintainers.

It is not a frozen third-party SDK compatibility promise.

A future SDK should define a narrower public policy deliberately.

### Web 8

Web 8 owns the final production quality/deployment layer:

- broken-link validation;
- static-output privacy/secret/path audit;
- accessibility baseline;
- responsive-browser checks;
- performance budget;
- production `site` URL and sitemap;
- deployment;
- final release-page artifact integration.

---

## 21. Completion statement

**Web 7 — C++ API Reference is COMPLETE for its defined scope.**

Nocturne now has conceptual documentation, machine-oriented canonical knowledge and generated C++ symbol documentation as three connected but non-conflicting layers.

The next milestone is **Web 8 — CI, Deployment and Quality Gates**.

---

## 22. Next chat handoff

Say:

> Continue with **Web 8 — CI, Deployment and Quality Gates** on branch `web-docs-foundation`. Read the Website specification and Web 7 implementation report first. Add broken-link validation, static-output privacy/secret/path auditing, accessibility and responsive-browser baselines, performance budgets, production site/sitemap/deployment configuration and final release artifact integration. Preserve the current canonical/AI/API source-precedence model, do not publish fake releases, keep Website outside engine/runtime dependencies, and do not merge draft PR #2 automatically.
