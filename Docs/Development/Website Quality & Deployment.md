---
id: "noc.web.quality"
doc_type: "standard"
canonical: true
status: "active"
subsystem: "Website"
phase_introduced: 16
description: "Canonical build-quality, privacy, browser validation, deployment and release-to-Downloads publication contract for the Nocturne website."
source_files: ["Website/scripts/validate-static.mjs","Website/scripts/browser-quality.mjs","Website/scripts/postbuild.mjs","Website/scripts/validate-deployment-target.mjs",".github/workflows/web-quality.yml",".github/workflows/web-deploy.yml",".github/workflows/release-package.yml"]
source_docs: ["Docs/Web/Nocturne Website — Design & Technical Specification.md","Docs/Development/Documentation Model.md","Docs/Development/Release Process.md","Docs/Development/C++ API Reference.md"]
book_grounding: ["Jason Gregory — Game Engine Architecture (3rd ed.), §1.7 Tools and the Asset Pipeline"]
aliases: ["Website Quality","Web Quality Gates","Website Deployment","Web Deployment"]
deprecated_aliases: []
api_symbols: []
---

# Website Quality & Deployment

## Purpose

This document defines the production-quality and publication contract for the Nocturne website.

The website is a static tooling/product surface.

It must remain independent from engine/runtime compilation and it must never create a second source of truth for engine architecture or release support.

## Quality pipeline

The hosted quality workflow is:

`.github/workflows/web-quality.yml`

It runs independently of the self-hosted engine toolchain on `windows-latest`.

The pipeline builds the complete static surface:

```text
canonical Docs / Knowledge
        +
Astro / Starlight
        +
Doxygen HTML / XML
        |
        v
complete Website/dist
        |
        +--> link validation
        +--> privacy / credential scan
        +--> performance budgets
        +--> stale knowledge validation
        +--> browser accessibility baseline
        +--> responsive baseline
        +--> quality evidence artifact
```

**Design choice (not directly from the book):** the exact CI runner, validators, Playwright browser matrix and thresholds are Nocturne web-tooling policy.

## Static link validation

`Website/scripts/validate-static.mjs` scans generated HTML and validates internal `href` / `src` targets against the actual static artifact.

The gate covers:

- landing;
- Downloads;
- Starlight canonical docs;
- historical docs;
- generated Doxygen HTML;
- stable API-symbol redirects;
- static assets.

External URLs are not treated as local files.

A broken internal path fails CI.

## Privacy and credential audit

The same static validator scans publishable text artifacts for high-risk patterns including:

- concrete local Nocturne workspace paths;
- Windows user-profile paths;
- CI runner workspace paths;
- Unix user-home paths;
- private-key blocks;
- GitHub token shapes;
- AWS access-key shapes.

Historical Markdown remains authored historical evidence in `Docs/`.

For public history pages, `Website/scripts/prepare.mjs` sanitizes known concrete local workspace/home paths from the generated copy.

Canonical source documents are not rewritten by that history-only publication sanitizer.

A historical code comment in `AssetImportPipeline.cpp` was also changed from the machine-specific `<local-workspace>/Data` example to `<repo>/Data`.

## Performance baseline

Web 8 establishes deterministic static budgets rather than network-latency promises.

Current key-route limits:

```text
HTML per key route          <= 800 KiB
Referenced assets per route <= 2.5 MiB
JavaScript per route        <= 1.5 MiB
CSS per route               <= 1.5 MiB
Single raster image         <= 2 MiB
```

Key routes include:

- landing;
- Downloads;
- Docs Home;
- Runtime;
- Editor.

These are regression guardrails.

They are not claims that every browser/network combination will load within a particular wall-clock time.

**Design choice (not directly from the book):** the current budget values are Nocturne baseline thresholds and may be tightened using measured production telemetry later.

## Browser accessibility baseline

Web 8 uses Playwright Chromium for rendered-browser validation.

The baseline verifies:

- one main landmark;
- at least one H1;
- non-empty document title;
- document language;
- all images have an `alt` attribute;
- no positive `tabindex`;
- no duplicate element IDs;
- visible links/buttons have an accessible name;
- keyboard Tab reaches multiple visible controls;
- a visible keyboard focus indicator exists;
- the landing Skip-to-content link is the first keyboard target and reaches main content.

This is a baseline, not a certification of full WCAG conformance.

**Design choice (not directly from the book):** Playwright is the Nocturne rendered-browser regression tool.

## Responsive baseline

The same browser gate checks:

```text
375 × 812   mobile
768 × 1024  tablet
1440 × 900  desktop
```

Current routes under browser coverage:

- `/`;
- `/download/`;
- `/docs/`;
- `/docs/systems/runtime/`;
- `/docs/systems/editor/`.

The generated document must not create page-level horizontal overflow at those profiles.

## Production site metadata

`Website/astro.config.mjs` reads:

`NOCTURNE_SITE_URL`

when a final production origin is known.

Astro then has an explicit final site URL for canonical URL generation.

`Website/scripts/postbuild.mjs` generates:

- `sitemap.xml`;
- `robots.txt`.

The sitemap intentionally excludes generated Doxygen symbol pages and stable redirect pages from the product/documentation route list.

## Deployment contract

Deployment workflow:

`.github/workflows/web-deploy.yml`

Target:

GitHub Pages through the official custom-workflow artifact/deploy flow.

The current Nocturne site uses absolute root routes such as:

```text
/docs/
/download/
/api/
```

Therefore Web 8 requires a root-hosted production origin.

`Website/scripts/validate-deployment-target.mjs` rejects:

- non-HTTPS production URLs;
- URL paths/query/fragment;
- placeholder/localhost hosts;
- the repository's default `<owner>.github.io/<project>` subpath model.

A root-hosted custom domain must be configured before public activation.

No public deployment was performed as part of Web 8 because no real production URL/domain was supplied.

This is deliberate: deployment configuration must not manufacture a domain assumption.

## GitHub Pages publication

The deploy workflow uses:

- GitHub Pages configuration;
- a fully validated `Website/dist` artifact;
- Pages artifact upload;
- the protected `github-pages` deployment environment;
- Pages deployment.

A push to `master` only enters the production build when repository variable `NOCTURNE_SITE_URL` is configured.

A manual dispatch may supply the production URL explicitly.

## Release to Downloads integration

The release pipeline remains artifact-driven.

For a real version tag:

```text
Ship x86_64 / x86
      |
      v
package + SHA-256 + release records
      |
      v
publish GitHub Release assets
      |
      v
promote validated records into a manifest candidate
      |
      v
build + validate Downloads
      |
      v
open/update reviewed PR against master
```

The integration job is:

`Prepare Downloads manifest PR`

in:

`.github/workflows/release-package.yml`

It runs only after a tag-triggered release has been packaged and published.

It does not run for ordinary pull requests.

It does not auto-merge.

The current public release manifest remains empty until a real release follows this flow.

## Validation evidence

Web 8 quality run:

`35479615524`

Validated implementation head:

`445d1e3aa8c03d17ae8909a6f3a9d947f690a320`

Result:

`SUCCESS`

Observed evidence:

```text
Doxygen headers                         97
Nocturne Doxygen compounds             146
Generated sitemap routes               53
Generated HTML files scanned           798
Internal references validated          39,338
Private path/credential findings       0
Browser routes                         5
Viewport profiles                      3
```

Browser tooling:

```text
Playwright 1.63.0
Chrome for Testing 153.0.8010.12
```

Quality evidence artifact:

`nocturne-web-quality`

Artifact ID:

`10595860455`

## Commands

Static production-style build:

```powershell
cd Website
$env:NOCTURNE_SITE_URL="https://your-root-host.example"
npm run api
npm run build
npm run validate:api
npm run validate:static -- --require-api --require-sitemap
```

Browser baseline requires Playwright Chromium:

```powershell
npm install --no-save --package-lock=false playwright@1.63.0
npx playwright install chromium
npm run serve:dist
```

Run in a second shell:

```powershell
cd Website
$env:NOCTURNE_PREVIEW_URL="http://127.0.0.1:4322"
npm run validate:browser
```

## Source precedence

For engine behavior, this website-quality contract does not supersede canonical engine Architecture/System documentation.

For website build/deployment behavior, this document and the current scripts/workflows are the active contract.

## Book grounding

Gregory §1.7 supports disciplined tools/content pipelines as part of engine production.

The website validators, Playwright baseline, static budgets, sitemap policy, deployment target and reviewed manifest-PR flow are **Design choice (not directly from the book)**.

## Related documentation

- [Website Specification](/docs/development/website-specification/)
- [Documentation Model](/docs/development/documentation-model/)
- [C++ API Reference](/docs/development/c-api-reference/)
- [Release Process](/docs/development/release-process/)
