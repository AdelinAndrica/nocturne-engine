# Nocturne Website — Web 8 Implementation Report

> **Status:** COMPLETE — repository implementation
>
> **Track:** Nocturne Website
>
> **Milestone:** Web 8 — CI, Deployment and Quality Gates
>
> **Branch:** `web-docs-foundation`
>
> **Validated implementation head:** `445d1e3aa8c03d17ae8909a6f3a9d947f690a320`
>
> **Web Quality run:** `35479615524` — SUCCESS
>
> **Public deployment:** intentionally not activated; a real root-hosted production URL/custom domain has not been supplied.

---

## 1. Objective

Web 8 closes the Website track by converting the static site from a buildable surface into a validated production artifact with explicit publication rules.

The milestone adds:

- internal-link integrity;
- static privacy/secret scanning;
- performance regression budgets;
- rendered-browser accessibility checks;
- responsive checks;
- sitemap/robots generation;
- guarded deployment;
- release-to-Downloads automation.

---

## 2. Book grounding

Jason Gregory, *Game Engine Architecture (3rd Edition)*, §1.7 grounds disciplined tooling and content pipelines as part of engine production.

The web QA/deployment mechanics are not prescribed by the engine books.

Therefore the validators, Playwright usage, thresholds, GitHub Pages deployment, sitemap rules and reviewed manifest PR are **Design choice (not directly from the book)**.

---

## 3. Full static quality workflow

Created:

`.github/workflows/web-quality.yml`

Runner:

`windows-latest`

The workflow is independent of the self-hosted Windows engine runner.

It builds:

- Astro/Starlight;
- canonical/history docs;
- AI knowledge endpoints;
- Doxygen HTML/XML;
- API aliases;
- sitemap/robots.

It then validates the complete artifact.

---

## 4. Internal-link validation

Created:

`Website/scripts/validate-static.mjs`

The validator walks generated HTML and resolves local `href` / `src` references against actual files in `dist`.

The first real run found two issues:

1. Starlight's default `/favicon.svg` target did not exist.
2. the Doxygen symbol alias generator exposed `std`, even though that non-Nocturne namespace did not have the corresponding generated HTML page.

Fixes:

- Starlight favicon now uses the existing `/nocturne-logo.png`;
- stable Doxygen aliases are generated only for `noc` / `nocturne` namespaces.

The successful run validated:

`39,338 internal references`

across:

`798 generated HTML files`.

---

## 5. Static privacy / credential gate

The static validator scans publishable textual artifacts for:

- concrete local Nocturne paths;
- Windows user paths;
- CI workspace paths;
- Unix home paths;
- private key blocks;
- GitHub-token shapes;
- AWS access-key shapes.

The first pre-Web-8 audit had identified:

`D:/Projects/Nocturne/Data`

inside historical documentation and a source comment.

Changes:

- generated history sanitizes the known local workspace root to `<repo>`;
- user-home patterns are sanitized in generated history;
- the AssetImportPipeline historical comment now uses `<repo>/Data`.

Successful Web 8 result:

`0 private path/credential findings`.

Authored historical source remains preserved in `Docs/`; the sanitization is applied to the public historical copy.

---

## 6. Performance budgets

Web 8 defines deterministic route-size guardrails:

```text
HTML                    800 KiB
Referenced route assets 2.5 MiB
JavaScript              1.5 MiB
CSS                     1.5 MiB
Single raster image     2 MiB
```

Coverage:

- landing;
- Downloads;
- Docs Home;
- Runtime;
- Editor.

The budgets guard against static regressions without pretending to guarantee wall-clock load times for every client/network.

---

## 7. Browser quality gate

Created:

`Website/scripts/browser-quality.mjs`

Pinned CI tooling:

`Playwright 1.63.0`

Validated browser:

`Chrome for Testing 153.0.8010.12`

Browser routes:

```text
/
/download/
/docs/
/docs/systems/runtime/
/docs/systems/editor/
```

Viewport profiles:

```text
375 × 812
768 × 1024
1440 × 900
```

---

## 8. Accessibility baseline

Validated assertions include:

- HTML language;
- page title;
- one main landmark;
- H1 presence;
- image alt attributes;
- no positive tabindex;
- duplicate-ID rejection;
- visible interactive names;
- keyboard traversal;
- visible focus treatment;
- functional landing Skip-to-content link.

This is a regression baseline, not a claim of formal accessibility certification.

---

## 9. Responsive baseline

All five representative routes pass at mobile, tablet and desktop viewport profiles.

The gate rejects document-level horizontal overflow and unexpectedly collapsed main content.

Successful result:

`5 routes × 3 viewport profiles`.

---

## 10. Static preview server

Created:

`Website/scripts/serve-static.mjs`

Default:

`http://127.0.0.1:4322`

The server exists only for validation/local inspection of `Website/dist`.

It is not a production application server.

---

## 11. Production URL and canonical metadata

`Website/astro.config.mjs` now accepts:

`NOCTURNE_SITE_URL`

when a production origin is supplied.

The custom site layout emits a canonical link when Astro has that production `site` value.

No production origin is invented when the variable is absent.

---

## 12. Sitemap and robots

Created:

`Website/scripts/postbuild.mjs`

With a production-style site URL it writes:

- `dist/sitemap.xml`;
- `dist/robots.txt`.

Validated Web 8 result:

`53 sitemap routes`.

Doxygen generated symbol/detail pages and stable redirect pages are intentionally excluded from the product/docs sitemap.

---

## 13. Deployment target

Created:

`.github/workflows/web-deploy.yml`

Deployment target:

GitHub Pages custom workflow.

The workflow follows:

```text
build validated static artifact
        |
        v
upload Pages artifact
        |
        v
github-pages protected environment
        |
        v
deploy-pages
```

---

## 14. Root-hosting guard

Created:

`Website/scripts/validate-deployment-target.mjs`

The current site contains absolute root routes.

Therefore production deployment currently requires a root-hosted HTTPS origin.

Rejected targets include:

- non-HTTPS;
- path/query/fragment URLs;
- placeholder hosts;
- the default GitHub Pages project-subpath shape for this repository.

A custom/root-hosted production domain must be configured before activation.

No Pages deployment was performed during Web 8.

That is an intentional truthfulness/safety gate, not missing implementation.

---

## 15. Release → Downloads integration

Extended:

`.github/workflows/release-package.yml`

New tag-only job:

`Prepare Downloads manifest PR`

After real release assets are published it:

1. downloads both release records;
2. checks out `master`;
3. promotes records with the existing validated promotion script;
4. validates release metadata;
5. builds the website;
6. uploads the promoted manifest as evidence;
7. creates/updates a release-manifest branch;
8. opens a PR against `master`.

It does not auto-merge.

Ordinary pull requests do not execute this publication job.

---

## 16. Release honesty

At Web 8 completion:

```json
{
  "schemaVersion": 1,
  "latest": null,
  "releases": []
}
```

remains the public manifest.

No architecture is shown as downloadable merely because a build configuration exists.

No fake release was added to complete the Website track.

---

## 17. Successful validation evidence

Web Quality:

`35479615524` — **SUCCESS**

Validated:

```text
Generate C++ API                         SUCCESS
Build complete static site               SUCCESS
Validate generated C++ API               SUCCESS
Links/privacy/performance                 SUCCESS
Generated knowledge stale-check          SUCCESS
Browser accessibility/responsive         SUCCESS
Quality evidence upload                  SUCCESS
```

Observed counts:

```text
Doxygen headers                         97
Nocturne compounds                     146
Sitemap routes                          53
HTML files                              798
Internal references                 39,338
Privacy/credential findings              0
Browser routes                            5
Viewport profiles                         3
```

Quality artifact:

`nocturne-web-quality`

Artifact ID:

`10595860455`

---

## 18. Regression gates

On the validated head:

- Nocturne Web Quality — SUCCESS;
- Nocturne Website CI — SUCCESS;
- Nocturne C++ API Docs — SUCCESS.

Windows CI and Release Packaging continue to use the repository's self-hosted Windows/X64 infrastructure and may queue independently of the hosted website gates.

Web 8 does not alter engine/runtime semantics.

---

## 19. Files introduced or materially changed

### Quality scripts

```text
Website/scripts/validate-static.mjs
Website/scripts/browser-quality.mjs
Website/scripts/serve-static.mjs
Website/scripts/postbuild.mjs
Website/scripts/validate-deployment-target.mjs
```

### Workflows

```text
.github/workflows/web-quality.yml
.github/workflows/web-deploy.yml
.github/workflows/release-package.yml
```

### Web integration

```text
Website/package.json
Website/astro.config.mjs
Website/src/layouts/SiteLayout.astro
Website/scripts/prepare.mjs
```

### Privacy cleanup

`Engine/Assets/AssetImportPipeline.cpp`

Comment-only path cleanup; runtime behavior is unchanged.

---

## 20. Verification checklist

- [x] complete static site can be built independently;
- [x] Doxygen is included in production-quality validation;
- [x] internal broken links fail CI;
- [x] favicon target is real;
- [x] symbol aliases expose only Nocturne namespaces;
- [x] local/private paths fail static publication;
- [x] credential signatures fail static publication;
- [x] known historical local path is sanitized publicly;
- [x] static performance budgets exist;
- [x] keyboard navigation baseline passes;
- [x] visible focus baseline passes;
- [x] Skip-to-content passes;
- [x] mobile responsive baseline passes;
- [x] tablet responsive baseline passes;
- [x] desktop responsive baseline passes;
- [x] sitemap is generated when final site URL exists;
- [x] robots file is generated with sitemap reference;
- [x] production URL can feed Astro `site`;
- [x] root-hosting assumptions are validated explicitly;
- [x] GitHub Pages deployment workflow exists;
- [x] deployment refuses silent project-subpath assumptions;
- [x] release tags can prepare a reviewed Downloads manifest PR;
- [x] release manifest PR is never auto-merged;
- [x] current public Downloads manifest remains truthful and empty;
- [x] Website remains outside engine/runtime dependencies.

---

## 21. Deferred operational activation

The repository-side Website track is complete.

The only intentionally unperformed operation is **public deployment**, because it requires a real production URL/custom domain owned/configured by the project.

Once that exists:

1. configure the GitHub Pages custom domain;
2. set repository variable `NOCTURNE_SITE_URL` to the root HTTPS origin;
3. run/merge through the normal production workflow;
4. verify the deployed Pages environment URL and DNS.

Do not substitute an invented domain merely to make CI green.

---

## 22. Completion statement

**Web 8 — CI, Deployment and Quality Gates is COMPLETE for repository implementation.**

**The Nocturne Website track, Web 1 through Web 8, is complete.**

The next engineering work should return to the Nocturne engine roadmap.

---

## 23. Next chat handoff

Say:

> Continue with **Phase 16 — Editor Scene Editing**. Study every existing Phase file first, especially the Phase 15 completion/CI reports and all current Phase 16 architecture, reflection, handoff and implementation-checklist documents. Preserve the production engineering standard, `EditorShellV3`, the Phase 15 World/ECS ownership and handle contracts, and the canonical documentation model established by the Website track.
