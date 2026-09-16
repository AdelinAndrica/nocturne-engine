# Phase 13 — UI Fidelity Pass 3: Icons, Toolbar Rhythm & Content Browser

> **Status:** IMPLEMENTED ON `phase-13-editor-framework`; local Windows build/visual verification required.
>
> **Scope:** continuation of Phase 13 editor-shell polish only. No Phase 14 viewport rendering, gizmos, picking, scene editing, ECS authoring, serialization or PIE behavior is introduced.

## 1. Objective

Bring the current Nocturne Editor shell closer to the approved visual target by removing the remaining generic-control feel and improving visual hierarchy in the toolbar and Content Browser.

## 2. Implemented changes

### 2.1 Icon system

**Design choice (not directly from the book):** editor icons are lightweight GDI vector primitives drawn by Nocturne tooling code so they scale cleanly with DPI and do not depend on Unicode glyph fallback or raster assets.

Implemented an editor-only icon vocabulary for:

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
- Scene Hierarchy categories
- common Content Browser asset categories

Button labels remain real text and are stored separately from icon state.

### 2.2 Toolbar rhythm

- Menu height reduced to ~26 px.
- Toolbar reduced to ~48 px with ~34 px button hit targets.
- Ordinary button gap reduced to ~4 px.
- File, History, Transform and Run/Build groups receive larger spacing without bright separator lines.
- Idle borders are intentionally low contrast.
- Hover, active and keyboard-focus states remain custom and theme-controlled.
- Menu items are flat/text-first when idle.

### 2.3 Content Browser composition

- Folder tree target width reduced to roughly 30%, clamped to a compact range.
- Asset table receives the majority of Content Browser horizontal space.
- Search row is now `Search | List | Grid | Settings`.
- List mode is active; Grid and Settings remain explicit Phase 13 tooling-shell stubs.
- Asset table is fully custom painted and includes category icons.
- Asset-row selection uses a restrained accent tint.
- No native ListView header, border or horizontal scrollbar is used.

### 2.4 Hierarchy and panel chrome

- Generic colored header markers are replaced in Pass 3 by semantic vector icons.
- Scene Hierarchy uses semantic icons for world, object, camera and environment/folder entries.
- Existing custom disclosure behavior is preserved.

### 2.5 Content-root robustness

**Design choice (not directly from the book):** a relative content root is resolved from the current process location first and, when needed, by walking parent directories from the built executable path. This prevents the Content Browser from silently depending on being launched from the repository root.

### 2.6 Startup paint fix

The Pass 3 shell forces one complete parent/child redraw after initial layout so uncovered panel gaps do not retain the previous Win32 background until the first resize.

## 3. Implementation structure

Pass 3 is implemented as a dedicated `EditorShellV3` while the previous Phase 13 shell remains in the branch for comparison and rollback during visual iteration.

Files added/changed:

- `Apps/NocturneEditor/EditorShellV3.h`
- `Apps/NocturneEditor/EditorShellV3.cpp`
- `Apps/NocturneEditor/main.cpp`
- `Ide/VS2026/NocturneEditor/NocturneEditor.vcxproj`

**Design choice (not directly from the book):** keeping the prior shell temporarily allows visual A/B iteration without discarding the already validated Pass 2 implementation. Once Pass 3 is build- and visually validated, the duplicate shell should be consolidated rather than retained indefinitely.

## 4. Visual metrics target

- Menu height: ~26 px
- Toolbar height: ~48 px
- Main toolbar buttons: ~34 px high
- Ordinary button gap: ~4 px
- Toolbar group gap: ~14 px effective spacing
- Content search/input height: ~30 px
- Content action buttons: ~30×30 px
- Content tree target width: ~30%, clamped around 110–145 px at typical editor widths
- Table header: ~26 px
- Table rows: ~25 px

## 5. Verification checklist

- [ ] Debug x64 build succeeds with zero errors.
- [ ] Button text remains visible immediately on startup.
- [ ] Initial frame has no white/unpainted panel gaps; resize is not required to clean the UI.
- [ ] Toolbar has semantic icons separated from labels.
- [ ] Toolbar groups read clearly without bright separator lines.
- [ ] Menu strip is flat when idle.
- [ ] Content Browser gives more width to the asset table than the folder tree.
- [ ] Search row includes compact list/grid/settings controls.
- [ ] Asset table remains fully dark/custom and has no native ListView chrome.
- [ ] Panel headers use semantic icons instead of generic square markers.
- [ ] Scene Hierarchy entries use semantic icons.
- [ ] Content Browser resolves `Data` when editor is launched from the built executable location.
- [ ] Resize behavior remains stable.
- [ ] No Phase 14 functionality is introduced.

## 6. Book grounding

The editor/world-tooling role remains grounded in Jason Gregory, *Game Engine Architecture (3rd Edition)*, Chapter 15.4 and its discussion of game-world editors and integrated asset/tool workflows.

All iconography, layout density, color treatment, custom GDI control rendering and Content Browser proportions in this document are **Design choice (not directly from the book)**.
