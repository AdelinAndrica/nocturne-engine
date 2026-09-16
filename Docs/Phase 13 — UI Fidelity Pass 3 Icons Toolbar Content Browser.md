# Phase 13 — UI Fidelity Pass 3: Icons, Toolbar Rhythm & Content Browser

> **Status:** ✅ COMPLETE + VALIDATED on `phase-13-editor-framework`.
>
> **Scope:** continuation of Phase 13 editor-shell polish only. No Phase 14 viewport rendering, gizmos, picking, scene editing, ECS authoring, serialization or PIE behavior was introduced.
>
> **Final active shell introduced here:** `Apps/NocturneEditor/EditorShellV3.*`

## 1. Objective

Bring the Nocturne Editor shell closer to the approved visual target by improving visual hierarchy, toolbar grouping, panel chrome and Content Browser composition, while preserving the editor/runtime boundary established earlier in Phase 13.

## 2. Implemented changes

### 2.1 `EditorShellV3`

Pass 3 introduced a dedicated `EditorShellV3` implementation and switched `main.cpp` to use it.

The previous `EditorShell` / `EditorControls` implementation remains in the branch as historical/fallback code, but it is not the active shell used by the editor executable.

**Important for future phases:** modify `EditorShellV3`, not the older shell, unless a deliberate consolidation refactor is being performed.

### 2.2 Toolbar rhythm

- Menu height reduced to roughly 26 px.
- Toolbar reduced to roughly 48 px.
- Main toolbar hit targets remain roughly 34 px high.
- Ordinary button gaps reduced.
- Larger optical spacing separates File, History, Transform and Run/Build groups.
- Idle borders are low contrast.
- Hover, active and keyboard-focus states remain custom.
- Menu items are flat/text-first when idle.

The final conceptual grouping is:

`New Open Save | Undo Redo | Select Move Rotate Scale | Play Stop Build`

### 2.3 Content Browser composition

- Folder tree reduced to roughly 30% of Content Browser width with compact clamping.
- Asset table receives the majority of horizontal space.
- Compact action row established for list/grid/settings modes.
- List mode is the Phase 13 active mode.
- Grid and Settings remain tooling-shell stubs.
- Asset table remains fully custom painted.
- Selection/hover treatment uses restrained theme colors.
- Stock ListView header/chrome/horizontal scrollbar are not used.

### 2.4 Hierarchy and panel chrome

- Generic panel markers were replaced with semantic icon slots.
- Scene Hierarchy rows gained semantic world/object/camera/folder roles.
- Existing custom disclosure behavior was preserved.
- Panel headers were made quieter and more consistent with the target mockup.

### 2.5 Content-root robustness

A relative content root is now resolved robustly from the process context and, when required, by walking parent directories from the built executable.

This prevents the editor from depending on the repository root being the current working directory.

**Design choice (not directly from the book):** this is editor/tooling path-resolution behavior.

### 2.6 Startup paint fix

The shell performs a complete parent/child redraw after initial layout.

This fixed the visual defect where white/unpainted gaps could remain between panels until the first manual window resize.

### 2.7 Button-label reliability

Custom button labels are stored in the button state and painted from that stored state.

This fixed the regression where the compact custom buttons could render with no visible text after owner-drawing changes.

## 3. Icon history in Pass 3

Pass 3 initially used lightweight hand-drawn GDI vector icons as a fast way to validate:

- icon placement;
- text/icon spacing;
- toolbar grouping;
- panel-header icon slots;
- Scene Hierarchy icon slots;
- Content Browser asset icon slots.

That icon implementation was intentionally superseded in Pass 4.

The final Phase 13 icon system is **Tabler SVG**, documented in:

`Docs/Phase 13 — UI Fidelity Pass 4 Tabler Icons.md`

The Pass 3 GDI drawings remain only as emergency fallback code and are not the approved visual language.

## 4. Files added/changed

- `Apps/NocturneEditor/EditorShellV3.h`
- `Apps/NocturneEditor/EditorShellV3.cpp`
- `Apps/NocturneEditor/main.cpp`
- `Ide/VS2026/NocturneEditor/NocturneEditor.vcxproj`

Later Pass 4 integration added the SVG renderer and Tabler assets without changing the role of `EditorShellV3` as the active shell.

## 5. Locked layout metrics from this pass

Approximate baseline established here:

- Menu height: ~26 px
- Toolbar height: ~48 px
- Main toolbar buttons: ~34 px high
- Ordinary button gap: ~4 px
- Toolbar group gap: ~14 px effective spacing
- Content search/input height: ~30 px
- Content action buttons: ~30×30 px
- Content tree target width: ~30%, compactly clamped
- Table header: ~26 px
- Table rows: ~25 px

Later icon optical sizes are documented separately in Pass 4.

## 6. Validation

- [x] Debug x64 editor executable built and launched.
- [x] Button text remains visible immediately on startup.
- [x] Initial frame no longer requires resize to remove white/unpainted gaps.
- [x] Toolbar groups read clearly without bright separators.
- [x] Menu strip is flat when idle.
- [x] Content Browser gives most horizontal space to the asset table.
- [x] Compact list/grid/settings controls are present.
- [x] Asset table remains fully dark/custom.
- [x] Panel headers use semantic icon slots.
- [x] Scene Hierarchy entries use semantic icon slots.
- [x] Content Browser resolves `Data` when launched from build output.
- [x] Resize behavior is stable.
- [x] No Phase 14 functionality was introduced.
- [x] Temporary GDI icon system was successfully replaced by the validated Pass 4 Tabler system.

## 7. Book grounding

The editor/world-tooling role remains grounded in Jason Gregory, *Game Engine Architecture (3rd Edition)*, Chapter 15.4 and its discussion of game-world visualization, selection/tree views and integrated asset workflows.

Toolbar density, panel proportions, Content Browser composition, custom Win32 drawing and temporary/final icon choices are **Design choice (not directly from the book)**.
