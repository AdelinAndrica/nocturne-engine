# Phase 13 — Editor Framework Bootstrap

> **Status:** IMPLEMENTED ON `phase-13-editor-framework`; the base shell was built successfully in Debug x64 before the modernization pass. Rebuild is required after the modern skin changes.
>
> **Reference UI:** Nocturne Editor wireframe + ultra-modern dark-theme concept generated for this phase.

## 1. Phase name + objective

Create the first standalone **Nocturne Editor** application shell while preserving the locked Editor ↔ Runtime boundary: the editor is a client of the runtime engine and does not own or fork the engine main loop.

## 2. Key concepts from the books

Primary reference: Jason Gregory, *Game Engine Architecture (3rd Edition)*.

- Chapter 15.4 describes the **game world editor** as the gameplay-side authoring tool used to define and populate game worlds.
- Section 15.4.1 describes common editor facilities including world visualization, selection/tree views, property grids, placement tools and saving/loading.
- Section 15.4.2 discusses integrated asset management in world editors.
- Gregory stresses rapid iteration and warns against overcomplicated data-driven tooling; Phase 13 therefore implements only the shell needed by later phases.

The project architecture additionally locks the rule that the editor shares runtime engine modules and that Runtime retains ownership of the main loop.

## 3. What we implement now

- `NocturneEditor` executable project.
- Native Windows desktop shell titled **Nocturne Editor**.
- Custom dark client-area menu bar and editor toolbar.
- Scene Hierarchy panel backed by current runtime `World` summary data.
- Central Viewport placeholder with grid/axis/branding treatment.
- Inspector / Properties placeholder with guidance content.
- Content Browser reading the configured physical content root.
- Console / Output panel with timestamped editor messages.
- Build / Play panel.
- Custom status bar.
- Build button connected to the existing Phase 11 `AssetImportPipeline::ImportAll()` mechanism.
- Runtime-owned `MainLoop` remains the only main loop.
- Minimal optional `IWindowMessageSink` extension point in `WinWindow` for tool UI messages.
- Centralized `EditorTheme` visual tokens for colors and metrics.
- Direct `.vcxproj` builds no longer depend on a correct externally supplied `$(SolutionDir)`.

### Explicit non-goals

- No viewport camera/navigation implementation.
- No transform gizmos.
- No viewport picking/selection.
- No ECS/component editing.
- No scene serialization.
- No Play-In-Editor runtime bridge.
- No user-driven docking/persistence yet.

Those remain assigned to later roadmap phases.

## 4. Implementation steps

1. Add `Apps/NocturneEditor` application entry point.
2. Initialize the existing `noc::Engine` and create the editor shell window without attaching the DX12 swap chain to the top-level HWND.
3. Add `EditorShell`, which creates and lays out the editor panels.
4. Add an optional native-window message sink to `WinWindow`; `WinWindow` continues to own `WndProc`.
5. Populate Content Browser from `EngineConfig::contentRoot`.
6. Populate Scene Hierarchy from the current runtime `World` summary.
7. Route toolbar/menu actions to Phase-appropriate behavior or clearly logged later-phase placeholders.
8. Run the editor through `noc::MainLoop::Run()`.
9. Add `NocturneEditor.vcxproj` and register it in `Nocturne.slnx`.
10. Add `EditorTheme` and owner-drawn controls for the Phase 13 visual modernization pass.
11. Keep Phase 13 rendering detached from the top-level editor window; Phase 14 will provide a dedicated viewport render target.

## 5. Modernization pass

The modern UI concept is implemented as a native Win32 skin rather than a new editor framework dependency. The visual system now uses a near-black/slate base, restrained electric-blue accent, high-contrast typography, increased spacing, owner-drawn toolbar/menu buttons, custom panel headers, a dark viewport placeholder, dark tree/list surfaces, a monospace console, modern Build/Play presentation and a custom segmented status bar.

The goal is to make the Phase 13 shell visually credible while keeping its architecture replaceable. The implementation deliberately does not turn the editor shell into an engine-level UI dependency.

## 6. Verification checklist

- [ ] Pull latest `phase-13-editor-framework`.
- [ ] Build `NocturneEditor` Debug x64 from the terminal or Visual Studio 2026.
- [ ] Build completes with 0 errors.
- [ ] Start `Build/bin/Debug/NocturneEditor.exe` from the repository root.
- [ ] Window title is `Nocturne Editor` and receives the dark native title-bar treatment where supported by Windows.
- [ ] Client-area menu and toolbar use the dark theme.
- [ ] Scene Hierarchy is visible with a blue selection state.
- [ ] Viewport placeholder shows the Nocturne grid, branding, axis and Phase 14 message.
- [ ] Inspector is visible on the right with the empty-selection guidance state.
- [ ] Content Browser lists entries from `Data/` and uses dark tree/list surfaces.
- [ ] Console shows timestamped editor bootstrap messages.
- [ ] Build button invokes the asset import pipeline without crashing.
- [ ] F5 logs the Play request without starting a second runtime loop.
- [ ] Resizing the editor relayouts the panels.
- [ ] Closing the editor shuts down the engine cleanly.

## 7. Common pitfalls

- Do not attach the DX12 swap chain to the top-level Phase 13 editor HWND; it paints over the native editor controls. Phase 14 needs a dedicated viewport render target.
- Do not implement a separate editor game loop; Runtime owns orchestration.
- Do not move viewport/gizmo implementation into this phase.
- Do not add a second serialization implementation for the editor.
- Do not let editor-specific code leak into Render, Physics, Audio or Gameplay modules.
- Keep the native Win32 shell replaceable; it is not an engine-level UI dependency.

## 8. Design choices (not directly from the book)

- **Native Win32 controls for the Phase 13 bootstrap.** This avoids introducing a third-party GUI dependency before the editor architecture is proven.
- **Custom owner-drawn dark skin.** Colors, font choices, spacing, rounded button treatment, grid placeholder and icon-like glyphs are product/UI decisions derived from the Nocturne Editor concept image, not game-engine architecture requirements from the books.
- **Centralized `EditorTheme`.** Visual tokens are kept out of engine modules so the editor skin remains replaceable.
- **Fixed dock-like default layout.** User-driven docking/persistence is postponed until a dedicated editor UI layer is selected.
- **Native dark title bar through DWM where supported.** This is Windows tooling polish, not a book-level architecture requirement.
- **Per-monitor DPI awareness for Nocturne Editor.** This is a Windows desktop tooling choice.
- **`IWindowMessageSink` extension point.** This allows tool UI messages without duplicating `WndProc` or the engine window abstraction.
- **Top-level editor window is not the Phase 13 DX12 render target.** Phase 14 will own a dedicated viewport rendering surface.
- **Build button invokes `AssetImportPipeline::ImportAll()`.** Full Phase 12 cook/pack workflow remains a host/tooling concern until it is deliberately exposed to the editor.
- **MSBuild repo-root normalization.** Direct `.vcxproj` builds use repo-relative paths rather than relying on `$(SolutionDir)` being supplied by a solution build.

## 9. Next chat handoff

After the modernization build and runtime checks pass, say:

> `Phase 13 este build-uit și verificat cu UI-ul modern. Începem Phase 14 — Editor Rendering Viewport folosind Nocturne Editor existent.`

Bring the terminal build output and a screenshot if any Phase 13 compile/runtime or visual issue remains.
