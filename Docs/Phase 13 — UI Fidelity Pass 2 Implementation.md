# Phase 13 — UI Fidelity Pass 2 Implementation

> **Status:** ✅ COMPLETE + VALIDATED on `phase-13-editor-framework`.
>
> **Source specification:** `Docs/Phase 13 — UI Fidelity Polish Pass.md`
>
> **Historical note:** Pass 2 established the custom Nocturne control language. Pass 3 later moved the active shell to `EditorShellV3`, but the visual/interaction decisions from this pass remain part of the Phase 13 baseline.

## 1. Objective

Remove the most visible stock-Windows artifacts from the Phase 13 editor shell while keeping the work strictly inside editor/tooling scope. No Phase 14 rendering, gizmos, viewport picking, ECS editing, scene editing, serialization or PIE behavior was introduced.

## 2. Implemented changes

- Reduced `Segoe UI Variable Text` sizes across menu, toolbar, panel titles, hierarchy, Inspector, Build / Play and status areas.
- Reduced `Cascadia Mono` console typography.
- Reduced panel/header/status metrics while preserving usable hit targets.
- Lower-contrast charcoal/navy palette and subtler borders.
- Added the editor-only custom-control layer in `EditorControls.h/.cpp`.
- Added fully custom editor buttons with idle/hover/pressed/active/focus states.
- Removed stock dotted/default Win32 button focus chrome.
- Made menu-strip buttons flat when idle.
- Added a narrow custom vertical `EditorScrollBar` with themed track/thumb and no stock arrow buttons.
- Replaced the stock Content Browser `ListView` presentation with a custom `EditorDataTable`.
- Replaced stock tree chrome with a custom editor tree presentation for Scene Hierarchy and Content Browser folders.
- Added custom dark input styling.
- Migrated Console / Output to borderless RichEdit so output remains selectable/copyable while fitting the Nocturne theme.
- Added themed console timestamp/tag/warning/error treatment.
- Tightened Build / Play spacing and field styling.
- Simplified Inspector empty-state styling.
- Reduced the status bar to a quieter custom presentation.
- Reduced Viewport placeholder contrast and density without adding real rendering.
- Removed the late MSBuild output-directory mutation that caused the earlier MSB8012 target-path mismatch warning.

## 3. Important implementation bug fixed

The first Pass 2 build exposed C++ template deduction failures where Win32 `RECT`/`POINT` members (`LONG`) were passed together with `int` values to `std::min`, `std::max` and `std::clamp`.

The fix was to make the Win32-to-editor coordinate boundary explicit with `static_cast<int>(...)` where required.

This is **Design choice (not directly from the book)** and is an implementation detail of the Win32 tooling layer.

## 4. New tooling primitives

**Design choice (not directly from the book):** these are editor/tooling primitives and must not become runtime-game UI dependencies.

- `EditorTheme`
- `EditorButton`
- `EditorScrollBar`
- `EditorDataTable`
- editor tree control/painter
- editor input styling
- custom panel/status/viewport painting

## 5. Files changed in this pass

- `Apps/NocturneEditor/EditorControls.h`
- `Apps/NocturneEditor/EditorControls.cpp`
- `Apps/NocturneEditor/EditorTheme.h`
- `Apps/NocturneEditor/EditorTheme.cpp`
- `Apps/NocturneEditor/EditorShell.h`
- `Apps/NocturneEditor/EditorShell.cpp`
- `Ide/VS2026/NocturneEditor/NocturneEditor.vcxproj`
- `Directory.Build.targets`

## 6. Validation

- [x] Debug x64 editor build completed successfully after the Win32 coordinate-type fix.
- [x] The previous MSB8012 source was removed.
- [x] Stock bright/native toolbar outlines were removed.
- [x] Custom focus/active states replaced the original default-button appearance.
- [x] Content Browser table became fully dark/custom.
- [x] Stock table header/horizontal scrollbar artifacts were removed.
- [x] Scene Hierarchy disclosure/selection treatment became custom.
- [x] Console remained selectable/copyable while adopting custom styling.
- [x] Search/input styling became flat/dark.
- [x] Typography became visibly smaller and less crowded.
- [x] Menu items no longer looked like boxed native buttons when idle.
- [x] Status presentation became visually secondary.
- [x] No Phase 14 functionality was introduced.

## 7. Relationship to later Phase 13 passes

Pass 3 created `EditorShellV3` and refined layout, toolbar grouping, Content Browser proportions and icon placement. Pass 4 replaced the temporary hand-drawn icon approach with the final Tabler SVG system.

Therefore this file documents the **custom-control / stock-chrome-removal milestone**, not the final active shell implementation.

For the final Phase 13 state see:

- `Docs/Phase 13 — Editor Framework Bootstrap.md`
- `Docs/Phase 13 — Completion Report.md`
- `Docs/Phase 13 — UI Fidelity Pass 4 Tabler Icons.md`

## 8. Book grounding

The editor facilities themselves remain grounded in Jason Gregory, *Game Engine Architecture (3rd Edition)*, Chapter 15.4 and its game-world-editor discussion.

Typography, color palette, custom buttons, custom scrollbars, custom table/tree presentation and exact UI fidelity are **Design choice (not directly from the book)**.
