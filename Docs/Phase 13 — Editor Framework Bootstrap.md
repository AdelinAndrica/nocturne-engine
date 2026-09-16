# Phase 13 — Editor Framework Bootstrap

> **Status:** IMPLEMENTED ON `phase-13-editor-framework`; local Visual Studio build verification still required.
>
> **Reference UI:** Nocturne Editor wireframe generated for this phase.

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
- Menu bar and editor toolbar.
- Scene Hierarchy panel backed by current runtime `World` summary data.
- Central Viewport placeholder.
- Inspector / Properties placeholder.
- Content Browser reading the configured physical content root.
- Console / Output panel.
- Build / Play panel.
- Status bar.
- Build button connected to the existing Phase 11 `AssetImportPipeline::ImportAll()` mechanism.
- Runtime-owned `MainLoop` remains the only main loop.
- Minimal optional `IWindowMessageSink` extension point in `WinWindow` for tool UI messages.

### Explicit non-goals

- No viewport camera/navigation implementation.
- No transform gizmos.
- No viewport picking/selection.
- No ECS/component editing.
- No scene serialization.
- No Play-In-Editor runtime bridge.

Those remain assigned to later roadmap phases.

## 4. Implementation steps

1. Add `Apps/NocturneEditor` application entry point.
2. Initialize the existing `noc::Engine` and attach the editor main window through the existing engine API.
3. Add `EditorShell`, which creates and lays out the editor panels.
4. Add an optional native-window message sink to `WinWindow`; `WinWindow` continues to own `WndProc`.
5. Populate Content Browser from `EngineConfig::contentRoot`.
6. Populate Scene Hierarchy from the current runtime `World` summary.
7. Route toolbar/menu actions to Phase-appropriate behavior or clearly logged later-phase placeholders.
8. Run the editor through `noc::MainLoop::Run()`.
9. Add `NocturneEditor.vcxproj` and register it in `Nocturne.slnx`.

## 5. Verification checklist

- [ ] Build `NocturneEditor` in Visual Studio 2026, Debug x64.
- [ ] Start `NocturneEditor.exe` from the solution root working directory.
- [ ] Window title is `Nocturne Editor`.
- [ ] Menu and toolbar are visible.
- [ ] Scene Hierarchy is visible and shows Runtime World information.
- [ ] Viewport placeholder occupies the central editor area.
- [ ] Inspector is visible on the right.
- [ ] Content Browser lists entries from `Data/`.
- [ ] Console shows editor bootstrap messages.
- [ ] Build button invokes the asset import pipeline without crashing.
- [ ] F5 logs the Play request without starting a second runtime loop.
- [ ] Resizing the editor relayouts the panels.
- [ ] Closing the editor shuts down the engine cleanly.

## 6. Common pitfalls

- Do not implement a separate editor game loop; Runtime owns orchestration.
- Do not move viewport/gizmo implementation into this phase.
- Do not add a second serialization implementation for the editor.
- Do not let editor-specific code leak into Render, Physics, Audio or Gameplay modules.
- Keep the native Win32 shell replaceable; it is not an engine-level UI dependency.

## 7. Design choices (not directly from the book)

- **Native Win32 controls for the Phase 13 bootstrap.** This avoids introducing a third-party GUI dependency before the editor architecture is proven.
- **Fixed dock-like default layout.** User-driven docking/persistence is postponed until a dedicated editor UI layer is selected.
- **`IWindowMessageSink` extension point.** This allows tool UI messages without duplicating `WndProc` or the engine window abstraction.
- **Build button invokes `AssetImportPipeline::ImportAll()`.** Full Phase 12 cook/pack workflow remains a host/tooling concern until it is deliberately exposed to the editor.

## 8. Next chat handoff

Say:

> `Phase 13 este build-uit și verificat. Începem Phase 14 — Editor Rendering Viewport folosind Nocturne Editor existent.`

Bring the Visual Studio build output if any Phase 13 compile/runtime error remains.
