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
