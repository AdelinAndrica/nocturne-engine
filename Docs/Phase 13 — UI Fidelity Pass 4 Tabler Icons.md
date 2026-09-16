# Phase 13 — UI Fidelity Pass 4: Tabler Icon System

> **Status:** IMPLEMENTED + VISUALLY VALIDATED on `phase-13-editor-framework`.
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

## Final implementation

`EditorIconRenderer.h/.cpp` provides an editor-only SVG asset layer. SVG remains the source of truth. On Windows, icons are rasterized through the Direct2D SVG API into transparent premultiplied BGRA bitmaps, cached by icon/size/color, and alpha-blended into the existing Phase 13 GDI controls.

The validated raster path is:

`Tabler SVG (24x24 logical coordinates) -> Direct2D scale transform -> D2D render target -> CPU-readable staging bitmap -> cached HBITMAP -> AlphaBlend`

The explicit 24x24 logical viewport is important: shrinking the SVG viewport itself clipped the geometry instead of scaling the full glyph. The final renderer preserves the full Tabler viewBox and applies a Direct2D transform to the requested glyph size.

The renderer searches for `ThirdParty/TablerIcons/icons/outline` from the current repository working directory and by walking parent directories from the built executable, so Debug/Development builds launched from `Build/bin` can still resolve editor icon assets.

## Locked icon sizing

**Design choice (not directly from the book):** the following values are now the Phase 13 visual baseline:

- toolbar / primary action slots: 16px slot, **12px rendered glyph**;
- panel headers: **11px rendered glyph**;
- Scene Hierarchy rows: **11px rendered glyph**;
- Content Browser tree/table rows: **11px rendered glyph**;
- compact icon-only buttons: 11-12px depending on slot size.

The slot remains larger than the glyph so text alignment and spacing stay stable while the icon receives optical breathing room.

## Locked semantic mappings

The final visual review explicitly re-checked the potentially ambiguous mappings and retained the following because they remain readable at 11-12px and fit the Nocturne tooling vocabulary:

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

No additional semantic substitutions are required for the Phase 13 baseline.

## Legacy fallback policy

The old hand-drawn GDI icon code is no longer part of the approved visual language. It remains only as an emergency fallback if SVG initialization or asset lookup fails. Normal editor rendering must use the Tabler SVG path.

The one-off PowerShell migration/repair scripts used while integrating the renderer have been removed after validation; the canonical implementation now lives directly in the editor source.

## Verification checklist

- [x] Tabler SVG subset and MIT license are vendored in the repository.
- [x] `EditorShellV3` calls the SVG renderer directly.
- [x] Full 24x24 Tabler geometry is scaled rather than clipped.
- [x] Toolbar glyphs render at the locked 12px optical size.
- [x] Header/tree/table glyphs render at the locked 11px optical size.
- [x] Toolbar icons use consistent Tabler geometry/stroke weight.
- [x] Panel-header icons use the same Tabler vocabulary.
- [x] Scene Hierarchy and Content Browser icons render cleanly at compact sizes.
- [x] Active/hover tinting follows the Nocturne theme.
- [x] Icons resolve when the editor is launched from the build output.
- [x] Windows runtime visual validation completed against the approved Phase 13 editor shell.
- [x] No Phase 14 functionality is introduced.

## Phase 13 visual baseline

The current editor screenshot after the Direct2D viewBox/scaling fix is the baseline for future editor phases. Phase 14+ work should preserve this icon sizing, spacing and visual hierarchy unless a later editor-specific design pass explicitly replaces it.

## Book grounding

The editor/tooling role remains grounded in Jason Gregory, *Game Engine Architecture (3rd Edition)*, Chapter 15.4 and its discussion of game-world editors and integrated asset/tool workflows.

Tabler selection, SVG rendering, Direct2D integration, icon caching, tinting, glyph sizing and exact UI styling are **Design choice (not directly from the book)**.
