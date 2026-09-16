# Phase 13 — UI Fidelity Pass 4: Tabler Icon System

> **Status:** RENDERER + ASSETS IMPLEMENTED on `phase-13-editor-framework`; one small `EditorShellV3.cpp` integration patch and local Windows build/visual verification remain.
>
> **Scope:** Phase 13 editor-shell polish only. No Phase 14 viewport rendering, picking or gizmo behavior is introduced.

## Objective

Replace the inconsistent hand-drawn GDI icon vocabulary from Pass 3 with a coherent commercially usable icon system matching the approved Nocturne Editor visual target.

## Source and license

**Design choice (not directly from the book):** Nocturne Editor standardizes on **Tabler Icons 3.46.0** as its primary general-purpose icon vocabulary.

Tabler Icons are MIT licensed. The selected SVG subset and the upstream MIT license are vendored under:

`ThirdParty/TablerIcons/`

The upstream visual contract is retained: 24x24 SVG viewBox, 2px outline stroke, round caps and round joins.

Custom Nocturne-specific art should be limited to the Nocturne mark and concepts for which the selected set has no suitable semantic icon.

## Implementation

`EditorIconRenderer.h/.cpp` adds an editor-only SVG asset layer. SVG remains the source of truth. On Windows, icons are rasterized through the Direct2D SVG API into transparent premultiplied BGRA bitmaps, cached by icon/size/color, and alpha-blended into the existing Phase 13 GDI controls.

This preserves the current editor shell while eliminating Unicode-glyph fallback and the improvised per-icon GDI geometry.

The renderer searches for `ThirdParty/TablerIcons/icons/outline` from the current repository working directory and by walking parent directories from the built executable, so Debug/Development builds launched from `Build/bin` can still resolve editor icon assets.

## Selected mappings

- New -> `file.svg`
- Open/folder -> `folder.svg`
- Save -> `device-floppy.svg`
- Undo/Redo -> `arrow-back-up.svg`, `arrow-forward-up.svg`
- Select -> `pointer.svg`
- Move -> `arrows-move.svg`
- Rotate -> `rotate.svg`
- Scale -> `maximize.svg`
- Play/Stop -> `player-play.svg`, `player-stop.svg`
- Build/general 3D asset -> `box.svg`
- List/Grid/Settings -> `list.svg`, `layout-grid.svg`, `settings.svg`
- Scene Hierarchy -> `hierarchy-2.svg`
- Viewport -> `device-desktop.svg`
- Inspector -> `adjustments-horizontal.svg`
- Console -> `terminal-2.svg`
- World -> `world.svg`
- Camera -> `camera.svg`
- Texture -> `photo.svg`
- Material -> `sphere.svg`
- Text -> `file-text.svg`
- Metadata -> `braces.svg`

## Final integration step

Because the GitHub connector only replaces complete files and `EditorShellV3.cpp` is intentionally kept intact for Phase 13 A/B iteration, the repository contains an idempotent integration script:

```powershell
.\Tools\Phase13\ApplyTablerIconRenderer.ps1
```

It performs only two changes to `EditorShellV3.cpp`:

1. adds `#include "EditorIconRenderer.h"`;
2. makes `DrawIcon()` try the Tabler SVG renderer first and fall back to the validated Pass 3 GDI icon only if SVG rendering is unavailable.

After local build and visual validation, commit that small generated diff to `phase-13-editor-framework`.

## Verification checklist

- [ ] Run `Tools/Phase13/ApplyTablerIconRenderer.ps1` once.
- [ ] Debug x64 builds with zero errors.
- [ ] Toolbar icons use consistent Tabler geometry/stroke weight.
- [ ] Panel-header icons use the same Tabler vocabulary.
- [ ] Scene Hierarchy and Content Browser icons render cleanly at 14-16px.
- [ ] Active/hover tinting still follows the Nocturne theme.
- [ ] Icons resolve when launched from `Build/bin`.
- [ ] If SVG initialization fails, the existing GDI icon fallback remains usable.
- [ ] No Phase 14 functionality is introduced.

## Book grounding

The editor/tooling role remains grounded in Jason Gregory, *Game Engine Architecture (3rd Edition)*, Chapter 15.4 and its discussion of game-world editors and integrated asset/tool workflows.

Tabler selection, SVG rendering, Direct2D integration, icon caching, tinting and exact UI styling are **Design choice (not directly from the book)**.
