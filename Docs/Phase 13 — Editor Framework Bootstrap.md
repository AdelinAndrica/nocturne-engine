# Phase 13 — Editor Framework Bootstrap

> **Status:** ✅ COMPLETE + BUILD/RUNTIME/VISUAL VALIDATED on `phase-13-editor-framework`.
>
> **Final active shell:** `Apps/NocturneEditor/EditorShellV3.*`
>
> **Visual baseline:** the validated Nocturne Editor build after the Tabler SVG scaling/readback fixes.
>
> **Phase 14 readiness:** READY — the top-level editor window deliberately has no DX12 swap chain attached; Phase 14 can add a dedicated rendering surface inside the Viewport panel.

## 1. Phase name + objective

Phase 13 established the first standalone **Nocturne Editor** executable and the editor/runtime boundary needed by all later tooling phases.

The core architectural rule is now locked:

- the editor is a client of the existing engine/runtime;
- `noc::MainLoop` remains the single application loop;
- editor-specific UI code stays in `Apps/NocturneEditor`;
- the top-level editor HWND is a tooling shell, not the final DX12 render target;
- the Viewport panel is a Phase 13 placeholder and is the insertion point for the dedicated Phase 14 rendering surface.

## 2. Book grounding

Primary reference: Jason Gregory, *Game Engine Architecture (3rd Edition)*.

- §15.4 defines the **game world editor** as the gameplay-side tool used to define and populate game worlds.
- §15.4.1.2 discusses world visualization through perspective/orthographic views and notes that editors may use a rendering engine integrated into the tool, communicate with the game engine, or be integrated into the engine itself.
- §15.4.1.3 discusses 3D editor navigation and camera-control modes.
- §15.4.1.4 discusses object selection, including 3D picking and list/tree selection.
- §15.4.1.6 discusses property-grid presentation for the current selection.
- §15.4.1.7 discusses translation/rotation/scale handles and other alignment aids.
- §15.4.2 discusses integrated asset-management tools and the value of unified editor access to assets.

These sections ground the existence and broad responsibilities of the Nocturne Editor.

Everything about the exact Win32 implementation, theme, spacing, Tabler icon set, Direct2D SVG raster path and fixed panel composition is **Design choice (not directly from the book)**.

## 3. Final Phase 13 implementation

### 3.1 Standalone editor executable

Phase 13 added a separate `NocturneEditor` application/project. `main.cpp` initializes the same engine used by runtime applications, creates a resizable editor window, initializes `EditorShellV3`, then runs the existing `noc::MainLoop`.

`main.cpp` intentionally uses:

```cpp
window.Create(desc)
```

rather than attaching the runtime DX12 swap chain to the top-level editor HWND.

This was a critical Phase 13 correction: attaching DX12 to the top-level shell caused the renderer to paint across/over native editor child controls. The dedicated Viewport render target is therefore explicitly Phase 14 scope.

### 3.2 Runtime ownership preserved

No second editor loop was introduced. `noc::MainLoop::Run(engine, window)` remains the application loop.

`EditorShellV3` implements `noc::platform::IWindowMessageSink`, allowing editor-specific window/message handling while preserving `WinWindow` ownership of the Win32 window procedure.

### 3.3 Final editor layout

The active shell contains:

- custom client-area menu strip;
- compact grouped toolbar;
- Scene Hierarchy;
- central Viewport placeholder;
- Inspector / Properties;
- Content Browser;
- Console / Output;
- Build / Play panel;
- custom status bar.

The layout is fixed/dock-like for Phase 13. **Design choice (not directly from the book):** user-driven docking and layout persistence are not implemented yet. The Phase 13 goal was to prove the editor composition and subsystem boundaries before introducing a larger UI framework or docking system.

### 3.4 Scene Hierarchy

The Scene Hierarchy displays current runtime-world summary information and establishes the selection-oriented tree presentation required by later editor phases.

Phase 13 does not implement real viewport/world selection synchronization or scene mutation. Those remain later-phase responsibilities.

### 3.5 Inspector / Properties

The Inspector provides the final visual shell and empty-selection state, including guidance copy and the design language future property rows must use.

Actual component/property editing is not Phase 13 scope. Gregory's property-grid model is the architectural reference, but real editable properties depend on later entity/component and scene-editing phases.

### 3.6 Viewport placeholder

The central Viewport panel is intentionally a non-rendering placeholder containing:

- `Perspective / Lit / Show` controls;
- Nocturne watermark/branding;
- perspective-style grid;
- axis marker;
- grid-size badge;
- explicit Phase 14 message.

It exists to lock layout, input/UI ownership and visual composition before a dedicated DX12 child rendering surface is introduced.

### 3.7 Content Browser

The Content Browser was upgraded from stock/native controls into a fully Nocturne-styled editor surface:

- robust content-root resolution;
- folder tree;
- custom asset table;
- `Asset` / `Type` columns;
- list/grid/settings compact controls;
- semantic asset icons;
- dark custom selection/hover treatment;
- no exposed stock ListView header or native horizontal scrollbar.

Relative content roots are resolved from the process context and, when necessary, by walking parent directories from the executable so `Data/` resolves correctly when launching from build output.

### 3.8 Console / Output

The Console uses a dark borderless RichEdit-based output surface with smaller `Cascadia Mono` typography and themed log coloring. Timestamps are muted and `[Editor]` uses the Nocturne accent.

The console remains selectable/copyable while avoiding visible stock Win32 editor chrome.

### 3.9 Build / Play

The Build / Play panel establishes the future run/build workflow visually and provides Phase-appropriate behavior:

- Build is connected to the existing asset import/build-side tooling path available at this stage;
- Play/F5 remains a logged/editor-shell request and does not introduce a second runtime loop or PIE implementation.

Play-In-Editor remains Phase 27 in the roadmap.

### 3.10 Status bar

The status bar is custom painted and visually secondary. It exposes ready/issues/branch/object/version-style status information without stock Win32 status-bar bevels.

## 4. UI fidelity progression

Phase 13 was intentionally iterative. The final editor came from four visual/tooling passes.

### Pass 1 — modern skin foundation

The initial functional Win32 shell was restyled with:

- near-black/slate palette;
- electric-blue accent;
- `Segoe UI Variable Text` UI typography;
- `Cascadia Mono` console typography;
- owner-drawn toolbar/menu treatment;
- panel headers;
- modern viewport placeholder;
- custom status presentation.

This pass proved that the native shell could reach the intended visual direction without introducing a large GUI dependency.

### Pass 2 — remove stock Win32 chrome

Pass 2 introduced/reworked editor-only custom controls and removed the largest native-Windows artifacts:

- smaller typography and tighter metrics;
- custom buttons and focus states;
- flat idle menu items;
- narrow custom scrollbars;
- custom data table;
- custom tree presentation;
- custom dark inputs;
- RichEdit console integration;
- quieter Inspector / Build / status styling;
- corrected MSBuild repo-root handling.

A compile failure in this pass exposed `LONG`/`int` template-deduction mismatches in `std::min`, `std::max` and `std::clamp`; Win32 `RECT`/`POINT` values were explicitly converted at the boundary.

### Pass 3 — toolbar rhythm and Content Browser composition

Pass 3 moved the active implementation to `EditorShellV3` and refined:

- menu/toolbar height;
- toolbar grouping: File / History / Transform / Run-Build;
- button spacing;
- panel-header semantics;
- Scene Hierarchy icon slots;
- Content Browser proportions;
- list/grid/settings controls;
- custom table rows and asset categories;
- content-root resolution;
- startup redraw behavior.

The editor initially used hand-drawn GDI vector icons. This was a useful integration prototype but not the final icon system.

### Pass 4 — Tabler SVG icon system

The hand-drawn icon vocabulary was replaced by a commercially usable, coherent Tabler Icons subset vendored under:

`ThirdParty/TablerIcons/`

The upstream MIT license is included.

**Design choice (not directly from the book):** Tabler is the locked general-purpose editor icon vocabulary. Custom Nocturne art should be limited to Nocturne-specific concepts/branding.

The final icon path is:

`Tabler SVG (24x24 logical viewBox) -> Direct2D scale transform -> D2D target bitmap -> CPU-readable staging bitmap -> cached HBITMAP -> AlphaBlend`

Important fixes made while validating this path:

1. **Transparent icons after first SVG integration.** The initial implementation rendered into a D2D bitmap created from WIC, then read the original WIC backing store. Those pixels were not the rendered D2D result. The final implementation explicitly copies to a CPU-readable D2D staging bitmap and maps that bitmap.
2. **Icons looked like cropped corners.** Creating the SVG document directly at 11–12 px changed the SVG viewport rather than scaling the full 24x24 Tabler geometry. The final renderer keeps a 24x24 logical document and applies a Direct2D transform into the requested glyph size.
3. **Oversized optical weight.** Final glyph sizes are smaller than their layout slots so the icons have breathing room.
4. **C4244 overload warning.** Transparent clear color uses the explicit four-component `D2D1::ColorF(r,g,b,a)` overload.

Locked optical sizes:

- 16 px toolbar/action slot -> 12 px rendered glyph;
- panel header -> 11 px glyph;
- Scene Hierarchy -> 11 px glyph;
- Content Browser tree/table -> 11 px glyph;
- compact icon-only controls -> 11–12 px depending on slot.

## 5. Final icon mappings

The final Phase 13 semantic mapping is:

- New -> `file.svg`
- Open/folder -> `folder.svg`
- Save -> `device-floppy.svg`
- Undo / Redo -> `arrow-back-up.svg` / `arrow-forward-up.svg`
- Select -> `pointer.svg`
- Move -> `arrows-move.svg`
- Rotate -> `rotate.svg`
- Scale -> `maximize.svg`
- Play / Stop -> `player-play.svg` / `player-stop.svg`
- Build/general 3D asset -> `box.svg`
- List / Grid / Settings -> `list.svg` / `layout-grid.svg` / `settings.svg`
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

The older GDI drawings remain only as an emergency fallback and are not part of the approved visual language.

## 6. Build-system work completed during Phase 13

Phase 13 also hardened direct editor builds:

- `NocturneEditor.vcxproj` uses repository-relative paths rather than depending on an externally supplied `$(SolutionDir)`;
- editor sources compile as UTF-8;
- `Directory.Build.targets` supplies the engine include root for direct `NocturneEngine` project builds without late output-directory mutation;
- the previous MSB8012 target/output mismatch source was removed;
- the editor project includes the libraries required by its current Win32/DWM/RichEdit/Direct2D/D3D11/WIC/Shlwapi/AlphaBlend tooling path.

The D3D11 dependency here is editor-only support for Direct2D SVG rasterization; it is not the game renderer and does not replace the engine's DX12 renderer.

## 7. Bugs discovered and resolved

The following concrete issues were found during Phase 13 and are part of the phase history:

- DX12 attached to the top-level editor HWND painted over editor controls -> top-level attachment removed.
- Direct `.vcxproj` builds resolved engine paths under the IDE directory -> project paths normalized to the repository root.
- Late MSBuild output mutation produced MSB8012 -> late `OutDir` override removed.
- Custom-control compilation failed on mixed Win32 `LONG` and C++ `int` template arguments -> explicit conversions added at Win32 boundaries.
- Initial editor frame showed white/unpainted panel gaps until resize -> forced complete redraw after initial layout.
- Custom button labels disappeared -> labels stored in custom button state rather than relying on fragile window-text retrieval during owner drawing.
- Content Browser failed when launch working directory differed from repository root -> robust content-root discovery added.
- PowerShell patching temporarily corrupted UTF-8 punctuation -> migration script changed to explicit UTF-8 without BOM; final canonical sources are stored correctly.
- First Tabler SVG attempt produced invisible icons -> explicit D2D staging readback added.
- First small-glyph attempt cropped 24x24 SVG geometry -> fixed logical viewBox + Direct2D transform added.
- Initial Tabler optical scale was too heavy -> 12/11 px glyph baseline locked.

## 8. Files and ownership

### Active Phase 13 editor files

- `Apps/NocturneEditor/main.cpp`
- `Apps/NocturneEditor/EditorShellV3.h`
- `Apps/NocturneEditor/EditorShellV3.cpp`
- `Apps/NocturneEditor/EditorIconRenderer.h`
- `Apps/NocturneEditor/EditorIconRenderer.cpp`
- `Apps/NocturneEditor/EditorTheme.h`
- `Apps/NocturneEditor/EditorTheme.cpp`
- `Ide/VS2026/NocturneEditor/NocturneEditor.vcxproj`
- `Directory.Build.targets`
- `ThirdParty/TablerIcons/`

### Historical/fallback Phase 13 implementation

`EditorShell.*` / `EditorControls.*` represent earlier Phase 13 UI passes. `main.cpp` uses **EditorShellV3**. Phase 14 must not accidentally wire the rendering viewport into the older shell.

**Design choice (not directly from the book):** the older files are retained for now as Phase 13 history/fallback. They can be consolidated in a deliberate cleanup change, but that cleanup is not required to begin Phase 14.

## 9. Final verification

Phase 13 is considered complete because the following were exercised during the phase:

- [x] standalone Nocturne Editor builds and launches on Windows;
- [x] editor uses the runtime-owned main loop;
- [x] top-level shell does not attach the engine DX12 swap chain;
- [x] editor resizes and relayouts correctly;
- [x] initial paint no longer requires a manual resize to remove unpainted gaps;
- [x] Scene Hierarchy, Viewport, Inspector, Content Browser, Console and Build/Play panels are visible and styled consistently;
- [x] Content Browser resolves and lists `Data/` content;
- [x] Content table/tree no longer expose stock white/native chrome;
- [x] toolbar/menu no longer expose the original Vista-era white outline treatment;
- [x] Tabler SVG icons render visibly and completely;
- [x] final icon scaling was visually accepted as the Nocturne baseline;
- [x] Phase 13 stayed out of Phase 14 real viewport rendering;
- [x] no second main loop or PIE runtime was introduced.

The final visually approved icon/UI baseline was locked in commit `cfc06ed` (`chore(editor): lock validated Phase 13 icon baseline`).

## 10. Explicit Phase 13 non-goals / deferred work

The following are deliberately **not** part of Phase 13 completion:

- real DX12 rendering inside the editor viewport;
- editor camera/navigation;
- ray-pick selection;
- transform gizmos operating on scene objects;
- editor debug draw;
- ECS/component authoring;
- scene create/delete/edit workflows;
- scene serialization/save/load;
- true user-configurable docking and layout persistence;
- full project-file/project-manager abstraction;
- Play-In-Editor.

The roadmap assigns viewport camera/gizmos/selection/debug draw to Phase 14, entity/component infrastructure to Phase 15, scene editing to Phase 16, serialization to Phase 17 and PIE to Phase 27.

## 11. Common pitfalls carried into Phase 14

- Do **not** attach a renderer/swap chain to the top-level editor HWND.
- Do **not** create a second application main loop for the viewport.
- Do **not** wire Phase 14 into legacy `EditorShell`; use `EditorShellV3`.
- Do **not** let editor-only UI/Direct2D icon code leak into runtime rendering modules.
- Do **not** implement ECS, serialization or PIE while adding the viewport.
- Preserve the Phase 13 visual baseline unless a Phase 14 requirement directly needs a local viewport-control adjustment.

## 12. Next chat handoff

Use the dedicated document:

`Docs/Phase 14 — Editor Rendering Viewport Handoff.md`

Start the next chat with:

> `Phase 13 este COMPLETE. Începem Phase 14 — Editor Rendering Viewport. Studiază toate Phase Files și folosește Docs/Phase 14 — Editor Rendering Viewport Handoff.md ca punct de pornire. Păstrează baseline-ul UI din Phase 13 și implementează viewport camera, selection/picking, transform gizmos și debug draw fără să introduci Phase 15/16/17/27.`
