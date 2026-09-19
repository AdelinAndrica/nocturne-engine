# Nocturne Engine Website

Local/static website for Nocturne Engine.

> **Design choice (not directly from the book):** Astro + Starlight is the selected website/documentation stack.

## Requirements

- Node.js 22.12.0 or newer, using an even-numbered supported Node release.
- npm.

## Local development

```powershell
cd Website
npm install
npm run dev
```

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
