# Nocturne Engine Website

Local/static website for Nocturne Engine.

> **Design choice (not directly from the book):** Astro + Starlight is the selected website/documentation stack.

## Requirements

- Node.js 22.12.0 or newer, using an even-numbered supported Node release.
- npm.

## Local development

```powershell
cd Website
npm ci
npm run dev
```

The committed `package-lock.json` is authoritative for website dependency resolution.

The `predev` hook runs `scripts/prepare.mjs`, which:

1. generates CSS variables from `../Design/nocturne-theme.json`;
2. synchronizes repository-root `../Docs/` Markdown into the Starlight content tree;
3. copies the existing Nocturne logo from the editor resources into the generated public website assets.

Open the URL printed by Astro (normally `http://localhost:4321`).

Primary routes:

- `/`
- `/download`
- `/docs`

## Production build

```powershell
npm ci
npm run build
npm run preview
```

The production output is static and is written to `Website/dist/`.

## Source-of-truth rules

- Edit documentation in repository-root `Docs/`.
- Do not edit generated synchronized docs under `Website/src/content/docs/docs/history/`.
- Do not edit `Website/src/styles/generated/nocturne-theme.css`.
- Update `Design/nocturne-theme.json` only through a deliberate visual-contract change.
- The website must never become an engine/runtime build dependency.

See:

`Docs/Web/Nocturne Website — Design & Technical Specification.md`


## Release metadata

Validate the current public Downloads manifest:

```powershell
npm run validate:releases
```

Release candidate jobs produce one `release-record-<architecture>.json` file per architecture.

After the matching GitHub Release assets exist, promote the reviewed records into the website manifest with:

```powershell
npm run promote:release -- <record-x86_64.json> <record-x86.json>
npm run validate:releases
npm run build
```

The promotion command:

- requires all records to describe the same release;
- rejects duplicate platform/architecture/configuration tuples;
- rejects an already-published version;
- validates the final manifest before writing it;
- sets the promoted release as `latest`.

Do not add an architecture manually merely because a Visual Studio configuration exists.

Canonical process:

`Docs/Development/Release Process.md`


## AI / machine-readable documentation

The authored source remains repository-root `Docs/`.

Run:

```powershell
npm run sync
```

This regenerates and validates:

```text
../Knowledge/manifest.json
../Knowledge/terminology.json
../llms.txt
../llms-full.txt

public/knowledge/...
public/schemas/...
public/raw/<noc.id>.md
public/ai/<noc.id>.md
```

Public static entry points after build:

```text
/llms.txt
/llms-full.txt
/knowledge/manifest.json
/knowledge/terminology.json
/schemas/knowledge-manifest.schema.json
/schemas/terminology.schema.json
/raw/noc.runtime.md
/ai/noc.runtime.md
...
```

Do not hand-edit `Knowledge/*.json` or `llms*.txt`.

CI regenerates them and fails if the committed repository-level outputs are stale relative to canonical `Docs/`.

Canonical documentation pages expose **View Markdown**, **Copy Markdown** and **Copy for AI**. Historical Phase pages remain searchable but are not exported as canonical AI truth.


## C++ API reference

The generated symbol layer uses the repository-owned configuration:

`Docs/API/Doxyfile`

For a local Windows API build, install Doxygen and run:

```powershell
cd Website
npm run api
npm run validate:api
npm run build
```

Or run the combined command:

```powershell
npm run build:api
```

Validated CI currently uses Doxygen 1.10.0.

Generated public routes:

```text
/api/index.html
/api/nocturne.tag
/api-xml/index.xml
/api-symbols.json
/api-symbol/noc.Engine.html
/api-symbol/nocturne.editor.EditorShellV3.html
```

Generated API files under `Website/public/api*` are ignored by Git and recreated from current headers.

Canonical conceptual pages author `api_symbols` in repository-root `Docs/`; the website generator turns those names into stable `/api-symbol/` links and exposes the same relationships through `Knowledge/manifest.json`.

The Doxygen layer is structural/source reference. It does not replace canonical Architecture/System/Development documentation.
