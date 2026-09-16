# Phase 13 — UI Fidelity Pass 3: Icons, Toolbar Rhythm & Content Browser

> **Status:** LOCKED FOR IMPLEMENTATION on `phase-13-editor-framework`.
>
> **Scope:** continuation of Phase 13 editor-shell polish only. No Phase 14 viewport rendering, gizmos, picking, scene editing, ECS authoring, serialization or PIE behavior is introduced.

## 1. Objective

Bring the current Nocturne Editor shell closer to the approved visual target by removing the remaining generic-control feel and improving visual hierarchy in the toolbar and Content Browser.

## 2. Required changes

### 2.1 Icon system

**Design choice (not directly from the book):** editor icons are lightweight GDI vector primitives drawn by Nocturne tooling code so they scale cleanly with DPI and do not depend on Unicode glyph fallback or raster assets.

Introduce an editor-only icon vocabulary for:

- New / document
- Open / folder
- Save
- Undo / Redo
- Select / cursor
- Move
- Rotate
- Scale
- Play
- Stop
- Build / cube
- List view
- Grid view
- Settings
- panel-header categories
- common content asset categories

Button labels remain real text; icons are not encoded into strings.

### 2.2 Toolbar rhythm

- Reduce toolbar height while retaining comfortable hit targets.
- Use small inter-button gaps and larger group gaps.
- Group commands visually as File, History, Transform and Run/Build.
- Idle borders must be extremely subtle.
- Hover, active and keyboard-focus states remain custom and theme-controlled.
- No stock/native Win32 focus or default-button chrome may be visible.
- Menu items remain text-first and visually flat when idle.

### 2.3 Content Browser composition

- Folder tree target width: roughly 30% of the Content Browser body, clamped to a compact range.
- Asset table receives the majority of horizontal space.
- Search row becomes `Search | List | Grid | Settings`.
- List mode is the active Phase 13 presentation; Grid and Settings remain explicit tooling-shell stubs.
- Table header and rows remain fully custom painted.
- Asset table must avoid native ListView headers, borders and horizontal scrollbars.
- Asset rows gain small category icons where practical.
- Selection is a restrained accent tint, not a full saturated block.

### 2.4 Hierarchy and panel chrome

- Replace generic colored squares in panel headers with small semantic vector icons.
- Scene Hierarchy keeps custom disclosure arrows and compact rows.
- Hierarchy item icons are desirable where they do not complicate the Phase 13 shell.

### 2.5 Content-root robustness

**Design choice (not directly from the book):** the editor must resolve a relative content root robustly when launched from `Build/bin`, rather than silently depending on process working directory.

For the current repository layout, relative `Data` content should resolve against the repository/project root discovered from the executable path when necessary.

## 3. Visual metrics target

- Menu height: ~26 px
- Toolbar height: ~48 px
- Main toolbar buttons: ~34 px high
- Ordinary button gap: ~4 px
- Toolbar group gap: ~14 px
- Content search/input height: ~30 px
- Content action buttons: ~30×30 px
- Content tree target width: ~30%, clamped around 110–145 px at typical editor widths
- Table header: ~26 px
- Table rows: ~25 px

## 4. Acceptance checklist

- [ ] Button text remains visible immediately on startup.
- [ ] Initial frame has no white/unpainted panel gaps; resize is not required to clean the UI.
- [ ] Toolbar has semantic icons separated from labels.
- [ ] Toolbar groups read clearly without bright separator lines.
- [ ] Menu strip is flat when idle.
- [ ] Content Browser gives more width to the asset table than the folder tree.
- [ ] Search row includes compact list/grid/settings controls.
- [ ] Asset table remains fully dark/custom and has no native ListView chrome.
- [ ] Panel headers use semantic icons instead of generic square markers.
- [ ] Content Browser resolves `Data` when editor is launched from the built executable location.
- [ ] Build remains Debug x64 clean.
- [ ] No Phase 14 functionality is introduced.

## 5. Book grounding

The editor/world-tooling role remains grounded in Jason Gregory, *Game Engine Architecture (3rd Edition)*, Chapter 15.4 and its discussion of game-world editors and integrated asset/tool workflows.

All iconography, layout density, color treatment, custom GDI control rendering and content-browser proportions in this document are **Design choice (not directly from the book)**.
