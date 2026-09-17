# Phase 14 — Editor Rendering Viewport — Implementation Report

> **Status:** ✅ COMPLETE + LOCAL WINDOWS RUNTIME/INTERACTION VALIDATED
>
> **Branch:** `phase-14-editor-rendering-viewport`
>
> **Final code baseline covered by this report:** `6b8a3a45124e6c3a2b334952d7b3dd16e6a92813`
>
> **Baseline branch:** `phase-13-editor-framework`
>
> **Baseline head used for audit:** `fcec485e1345b878fd9cf69d9a705bfb9ce1e2ee`
>
> **Final validation statement:** the remaining Phase 14 checks were reported PASS locally; the last visual issue was the undersized top menu bar, corrected in `8fdbee86682211f872679131e41096709d53cacf` and subsequently reported working.

## 1. Phase name + objective

**Phase 14 — Editor Rendering Viewport**

Replace the Phase 13 central Viewport placeholder with a real DX12-backed editor viewport while preserving the active `EditorShellV3` shell, the Phase 13 editor/runtime boundary, and the single runtime-owned `noc::MainLoop`.

The completed Phase 14 delivers:

- a dedicated child-HWND DX12 presentation target inside the editor Viewport;
- resize-safe color/depth presentation resources;
- a deterministic 3D validation scene;
- a procedural editor sky;
- a depth-tested GPU grid and axes;
- editor camera navigation;
- single-object nearest-hit picking;
- viewport ↔ hierarchy selection synchronization;
- depth-tested oriented selection bounds;
- Select / Move / Rotate / Scale gizmos with hover/active feedback;
- frame-synchronized editor camera input;
- removal of the per-frame mesh-shader recompilation hotspot that caused the observed periodic stutter;
- final menu-bar proportion polish;
- a Nocturne Editor application icon/resource based on the supplied Nocturne logo asset.

Phase 14 did **not** introduce ECS, full scene authoring, serialization, PBR/lighting/shadows, asset previewers or PIE.

## 2. Book grounding

The architectural direction is grounded in the provided books:

- Jason Gregory, *Game Engine Architecture (3rd Edition)*, `15.4.1.2 — game-world visualization in editor tooling;
- Gregory `15.4.1.3 — editor viewport navigation;
- Gregory `15.4.1.4 — object selection and synchronization;
- Gregory `15.4.1.7 — object placement and transform aids;
- Frank D. Luna, *Introduction to 3D Game Programming with DirectX 12*, Chapter 4 — device/swap-chain/back-buffer/depth-buffer/viewport/scissor/resize lifecycle;
- Luna Chapter 17 — picking-ray construction, local/object-space bounds tests and nearest-hit selection.

The exact child-HWND composition, Win32 message routing, editor frame hook, procedural validation scene, procedural sky, GPU editor-grid pass, hierarchy bridge, selection-debug bridge, gizmo UX/constants, menu metrics and Windows application-icon integration are **Design choice (not directly from the book)**.

## 3. Final implementation

### 3.1 Dedicated DX12 editor viewport

`EditorShellV3` exposes only the Phase 14 seam required by the viewport controller:

- `ViewportBody()`;
- `SceneTree()`;
- `ActiveToolId()`.

`EditorViewportController` creates a dedicated `STATIC` child render host with:

- `WS_CHILD`;
- `WS_VISIBLE`;
- `WS_CLIPSIBLINGS`;
- `WS_CLIPCHILDREN`;
- `WS_TABSTOP`;
- `SS_NOTIFY`.

The render-host subclass returns `HTCLIENT` for `WM_NCHITTEST`, making this child the authoritative viewport mouse/focus/capture surface.

The top-level editor HWND remains chrome-only. DX12 is attached to the dedicated render child, not the top-level editor window.

A separate layered child overlay remains hit-transparent and is limited to transform-gizmo drawing. The compatibility manifest added during Phase 14 declares the Windows compatibility needed by the layered-child overlay path.

### 3.2 Resize and zero-size handling

Resize is routed through:

`EditorViewportController → Engine → RenderSystem → Dx12Renderer → Dx12SwapChain`.

The final path:

- tracks the render target width/height in `Engine`;
- treats zero width/height as a suspended render extent rather than issuing a DXGI resize;
- skips BeginFrame/EndFrame rendering while the tracked extent is zero;
- waits for GPU completion before a non-zero swap-chain resize;
- releases/reacquires swap-chain back buffers;
- recreates the depth target at the new size;
- updates camera aspect from the child render-host client size.

`Dx12SwapChain` uses `DXGI_FORMAT_D24_UNORM_S8_UINT` for the depth/stencil target.

### 3.3 Depth-tested validation rendering

The original smoke-test path was replaced with procedural cube validation geometry.

The deterministic editor scene contains:

- `Cube_A`;
- `Cube_B`;
- `Cube_C`;
- `Ground_Plane`;
- `Main Camera`;
- `Environment (Procedural Sky)`.

The three cubes use different transforms. `Ground_Plane` is the same procedural cube geometry with a large X/Z scale and small Y scale.

This scene is **Design choice (not directly from the book)** and exists only to validate Phase 14 mechanics before later scene-authoring phases.

The mesh PSO is depth-tested/depth-writing, and the viewport owns a real DSV. The PSO cache key was extended to include the DSV format.

### 3.4 Procedural editor sky

`Data/Shaders/EditorSky.hlsl` adds a fullscreen procedural gradient sky.

This is deliberately not a production lighting/environment system.

During this work the camera/view-matrix convention in `MathTypes.h` was corrected so CPU projection helpers and GPU rendering use the same transform convention.

### 3.5 GPU grid and axes

`Data/Shaders/EditorGrid.hlsl` and the editor line pass render grid/axes directly through DX12.

The line pass uses:

- line-list topology;
- depth test `LESS_EQUAL`;
- no depth writes.

The grid is positioned just below the top of `Ground_Plane` so scene geometry can occlude it. World axes remain slightly above the ground surface as an editor aid.

### 3.6 Scene Hierarchy bridge

The Phase 14 hierarchy exposes the validation scene as:

```text
Scene (Runtime World)
  Runtime Objects (4)
    Cube_A
    Cube_B
    Cube_C
    Ground_Plane
  Main Camera
  Environment (Procedural Sky)
```

The hierarchy bridge is temporary editor plumbing over the current pre-ECS `World` representation. It is not the Phase 15/16 scene architecture.

Viewport selection updates the corresponding hierarchy row, and hierarchy selection updates viewport selection/debug feedback.

### 3.7 Picking

The final picking path is:

1. viewport pixel → NDC;
2. NDC → view-space ray;
3. camera rotation → world-space ray;
4. inverse translation/rotation/scale per validation object;
5. local ray vs local AABB slab test;
6. smallest non-negative `t` wins.

The direction is not re-normalized after inverse non-uniform scale, preserving comparable ray parameters across candidates.

This follows Luna Chapter 17 at the conceptual level. The exact inverse-TRS implementation is **Design choice (not directly from the book)**.

### 3.8 Selection visualization

Selection debug data carries:

- local bounds min/max;
- exact selected-object world matrix;
- enabled flag.

`MeshPass` reconstructs the eight local bounds corners, expands them by a fixed world-space clearance converted back to local-axis units, transforms all corners by the selected object's exact world matrix, and draws the 12 edges through the same depth-tested editor line pass.

The final clearance is:

```cpp
constexpr float kSelectionWorldOffset = 0.006f;
```

This replaced percentage-based expansion, which visually over-inflated large/thin objects such as `Ground_Plane`.

The `0.006f` policy is **Design choice (not directly from the book)**.

### 3.9 Editor camera/navigation

Final camera behavior:

- RMB captures camera navigation;
- mouse movement controls yaw/pitch;
- W/S move forward/back;
- A/D strafe;
- Q/E move vertically;
- Shift applies the faster movement multiplier;
- mouse wheel adjusts `cameraSpeed_` in the range `1.0f..30.0f`;
- RMB release/focus loss ends capture.

An important architectural correction was made in `cb4713ab2230b3a279ace97a35f0055112dbe86c`:

- the Phase 14 `SetTimer(..., 16, ...)` camera update was removed;
- the top-level timer subclass was removed;
- mouse deltas are accumulated by Win32 input;
- `EditorViewportController::TickFrame()` consumes navigation once per engine frame;
- `noc::MainLoop` gained an optional pre-`Engine::Tick()` frame callback.

This preserves one runtime main loop and synchronizes editor-camera state with frame execution.

**Important historical note:** this commit improved the input/update architecture but did **not** eliminate the reported periodic stutter by itself.

### 3.10 Periodic stutter diagnosis and fix

The observed blocker was a periodic severe hitch while RMB capture was held.

Source inspection found that `MeshPass::Record()` reached `EnsureRootSigAndPso_()` every frame while validation instances were present, and that function recompiled `Basic.hlsl` VS/PS unless it returned early.

Commit `cc6458f2ae216b065b8b1b14a11cdd8386ad4bc2` added:

```cpp
if (psoReady_ && pso_ && rootReady_ && rootSig_)
    return true;
```

This prevents the mesh shader pair from being synchronously recompiled every frame once the graphics state is ready.

After this commit, local runtime testing reported the periodic stutter resolved.

The earlier `cb4713a` camera-frame synchronization remains part of the final architecture, but the runtime-confirmed stutter resolution occurred after `cc6458f`.

### 3.11 Transform gizmos

Commit `e08dd05c594d34cc586453cfc1cf2c58ede8935c` finalized transform-gizmo interaction.

The final implementation provides:

- Select mode with no transform gizmo;
- Move X/Y/Z handles with arrow heads;
- Scale X/Y/Z handles with square endpoints;
- Rotate X/Y/Z rings;
- tool-specific hit testing;
- hover highlighting;
- active-axis highlighting during drag;
- mouse capture for drag;
- local-axis transform behavior;
- world/TRS updates through the current `World::SetLocalTRS()` path;
- immediate selection-outline refresh after transform;
- gizmo world size derived from camera depth so the screen-space size remains usable while moving the camera.

Current design constants include:

- 64 segments for rotation rings;
- target gizmo size of approximately 88 pixels;
- hit radius of approximately 9 pixels.

These values and the screen-space interaction model are **Design choice (not directly from the book)**.

### 3.12 Phase 13 UI preservation and final menu-bar correction

The active shell remains `EditorShellV3`. Content Browser, Console, status bar, panel chrome and the Tabler icon system remain the Phase 13 foundation.

Commit `0651882fbeb2bacbc8806b23e14c833b02af5a76` initially increased both menu and toolbar vertical metrics.

That was not the requested final target.

Commit `8fdbee86682211f872679131e41096709d53cacf` corrected the result:

- toolbar height returned to 48;
- toolbar button height returned to 34;
- top menu height became 36;
- a dedicated 14 px semibold `Segoe UI Variable Text` menu font was added;
- menu-item widths/padding were increased.

The final visual validation was reported PASS after this correction.

Exact menu/font metrics are **Design choice (not directly from the book)**.

### 3.13 Nocturne Editor application icon

Commit `ad9e4038c97ff8a648dc39e9d0767b0a7971d652` added Windows application-icon integration:

- `Apps/NocturneEditor/NocturneEditorResource.h`;
- `Ide/VS2026/NocturneEditor/NocturneEditor.rc`;
- `Apps/NocturneEditor/Resources/NocturneEditor.ico`;
- `ResourceCompile` integration in the Visual Studio project;
- runtime loading of large/small icon resources;
- `WM_SETICON` for the editor HWND.

Commit `6b8a3a45124e6c3a2b334952d7b3dd16e6a92813` finalized the icon assets and added:

- `Apps/NocturneEditor/Resources/NocturneEngine-Logo.png`;
- the full-quality `NocturneEditor.ico` asset;
- project tracking for the source PNG.

This branding/resource integration is **Design choice (not directly from the book)**.

## 4. Final local validation record

### 4.1 Picking — PASS

Explicitly reported:

```text
Cube_A: 10/10
Cube_B: 10/10
Cube_C: 10/10
Ground_Plane: 10/10
Empty click clears: PASS
Rotated edge accuracy Cube_B: PASS
Rotated edge accuracy Cube_C: PASS
Viewport -> Hierarchy sync: PASS
Picking after camera movement: PASS
```

The final closure pass was also reported successful for nearest-hit behavior.

### 4.2 Camera/navigation — PASS

```text
RMB mouse-look: PASS
WASD: PASS
Q/E: PASS
Shift speed: PASS
Mouse wheel speed control: PASS
RMB release stops capture: PASS
Focus isolation: PASS
Picking after camera movement: PASS
Periodic FPS stuttering: RESOLVED
```

Mouse wheel changes navigation speed; it is not a zoom/dolly command.

### 4.3 Transform gizmos — PASS

Explicitly reported:

```text
Select tool hides gizmo: PASS

Move gizmo visible: PASS
Move X drag: PASS
Move Y drag: PASS
Move Z drag: PASS

Rotate rings visible: PASS
Rotate X drag: PASS
Rotate Y drag: PASS
Rotate Z drag: PASS

Scale gizmo visible: PASS
Scale X drag: PASS
Scale Y drag: PASS
Scale Z drag: PASS

Hovered axis highlights: PASS
Active axis highlights during drag: PASS
Selection outline follows transform: PASS
Picking still works after transforms: PASS
Gizmo remains usable after camera movement: PASS
```

### 4.4 Final closure checks — PASS

The final closure checklist was reported working locally, including:

- repeated resize/maximize/restore;
- projection/aspect behavior through resize;
- zero/minimized viewport handling;
- hierarchy → viewport selection synchronization;
- final `Ground_Plane` selection-outline appearance;
- nearest-hit selection behavior;
- DX12 debug-layer sanity check;
- Phase 13 editor UI regression check.

The only visual issue reported at that stage was the undersized top menu bar. After `8fdbee8` corrected that specific menu row, the final report was: **all working**.

### 4.5 Application icon — PASS

After the resource/icon commits, the editor/application icon integration was reported working.

## 5. Verification checklist

- [x] Dedicated child HWND is the DX12 presentation target.
- [x] Top-level editor HWND remains editor chrome, not the renderer target.
- [x] `EditorShellV3` remains active.
- [x] One runtime `noc::MainLoop` remains authoritative.
- [x] Resize is routed through Engine → RenderSystem → Dx12Renderer → Dx12SwapChain.
- [x] Zero-size viewport is handled without invalid DXGI resize work.
- [x] Back buffers and DSV are recreated on valid resize.
- [x] Scene mesh rendering is depth-tested.
- [x] Procedural sky renders.
- [x] GPU grid/axes render with depth testing and no depth writes.
- [x] Validation hierarchy exposes Cube_A/B/C and Ground_Plane.
- [x] Render-host mouse input is reliable through `SS_NOTIFY` + `HTCLIENT`.
- [x] Picking uses inverse-TRS local-space AABB tests.
- [x] Nearest positive hit wins.
- [x] Empty click clears selection.
- [x] Viewport ↔ hierarchy synchronization passes.
- [x] Selection outline follows translation/rotation/non-uniform scale.
- [x] Selection outline uses constant world-space clearance.
- [x] Camera RMB/WASD/QE/Shift/wheel/focus behavior passes.
- [x] Camera update is synchronized to engine frame cadence.
- [x] Per-frame mesh shader recompilation is eliminated after PSO readiness.
- [x] Observed periodic stuttering is resolved.
- [x] Move/Rotate/Scale gizmos render and interact correctly on X/Y/Z.
- [x] Gizmo hover/active feedback works.
- [x] Picking remains correct after transforms.
- [x] Resize/maximize/restore and minimize/restore checks pass.
- [x] DX12 debug-layer sanity check passes.
- [x] Phase 13 UI baseline remains intact after final menu correction.
- [x] No ECS/serialization/full scene-authoring/PIE/PBR/shadow scope was pulled into Phase 14.
- [x] Nocturne Editor application icon/resource integration works.

## 6. Deliberately deferred work

Phase 14 does **not** implement:

- ECS — Phase 15;
- full create/delete/component authoring — later phase;
- scene serialization/save/load — later phase;
- production undo/redo transform history;
- physics/collision — later phase;
- production lighting/shadows/PBR/material authoring;
- production environment/sky system;
- asset previewers;
- PIE — Phase 27;
- a generalized final multi-mesh editor-rendering architecture.

The procedural cube scene, gradient sky and editor-line rendering are validation scaffolding, not final production rendering content.

## 7. Known renderer cleanup not required for Phase 14 sign-off

During the stutter investigation, the PSO cache was observed to use shader-blob pointer values as part of `Dx12PsoKey` identity. Phase 14 did not redesign that cache. The validated stutter fix was instead the early return that prevents the already-ready mesh PSO path from recompiling shaders every frame.

Treat stable shader identity/hash design as future renderer cleanup if/when the cache is generalized. It is not recorded as a Phase 14 runtime blocker because the final Phase 14 validation passed.

This cleanup direction is **Design choice (not directly from the book)**.

## 8. Files substantially introduced/changed by Phase 14

Phase 14 substantially changed or introduced:

- `Apps/NocturneEditor/EditorShellV3.h/.cpp`;
- `Apps/NocturneEditor/EditorViewportController.h/.cpp`;
- `Apps/NocturneEditor/main.cpp`;
- `Apps/NocturneEditor/NocturneEditor.manifest`;
- `Apps/NocturneEditor/NocturneEditorResource.h`;
- `Apps/NocturneEditor/Resources/NocturneEditor.ico`;
- `Apps/NocturneEditor/Resources/NocturneEngine-Logo.png`;
- `Data/Shaders/EditorSky.hlsl`;
- `Data/Shaders/EditorGrid.hlsl`;
- `Engine/Core/Math/MathTypes.h`;
- `Engine/Render/RenderQueue.h`;
- `Engine/Render/RenderSystem.h/.cpp`;
- `Engine/Render/DX12/Dx12Renderer.h/.cpp`;
- `Engine/Render/DX12/Dx12SwapChain.h/.cpp`;
- `Engine/Render/DX12/Dx12PsoCache.h`;
- `Engine/Render/DX12/MeshPass.h/.cpp`;
- `Engine/Runtime/Engine.h/.cpp`;
- `Engine/Runtime/MainLoop.h/.cpp`;
- `Ide/VS2026/NocturneEditor/NocturneEditor.rc`;
- `Ide/VS2026/NocturneEditor/NocturneEditor.vcxproj`;
- Phase 14 documentation.

## 9. Completion state

Phase 14 is **COMPLETE** on `phase-14-editor-rendering-viewport` at the code baseline listed at the top of this report.

The canonical roll-up for future chats is:

`Docs/Phase 14 — Completion Report.md`
