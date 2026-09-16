# Phase 14 — Editor Rendering Viewport — Implementation Report

> **Implementation status:** IMPLEMENTED + 3D VALIDATION PATCH ON `phase-14-editor-rendering-viewport`
>
> **Validation status:** SOURCE/DIFF VALIDATED; LOCAL WINDOWS BUILD + INTERACTIVE GPU VALIDATION REQUIRED
>
> **Baseline:** `phase-13-editor-framework`

## 1. Phase name + objective

**Phase 14 — Editor Rendering Viewport**

Replace only the central Phase 13 viewport placeholder with a DX12-backed child render surface while preserving `EditorShellV3`, the approved Phase 13 chrome, and the runtime-owned main loop.

The current implementation now includes:

- a dedicated child HWND presentation target inside the existing viewport body;
- resize-aware DXGI back-buffer **and depth-buffer** recreation;
- depth-tested 3D rendering in the editor viewport;
- a deterministic validation scene with three selectable cubes plus a ground/platform cube;
- an editor fly camera;
- nearest-hit single-object picking across the selectable validation cubes;
- viewport/hierarchy selection synchronization at the level supported by the Phase 13 hierarchy;
- selection bounds visualization;
- translate / rotate / scale gizmo interaction foundation;
- editor-only grid, origin axes and gizmo debug drawing.

## 2. Key concepts from the books

The implementation follows the references locked in `Docs/Phase 14 — Editor Rendering Viewport Handoff.md`:

- Jason Gregory, *Game Engine Architecture (3rd Edition)*, §15.4.1.2: game-world visualization inside editor tooling;
- Gregory §15.4.1.3: editor viewport navigation;
- Gregory §15.4.1.4: selection and synchronization with tree/list representations;
- Gregory §15.4.1.7: placement/alignment handles for transforms;
- Frank D. Luna, *Introduction to 3D Game Programming with DirectX 12*, Chapter 4: swap chain, back buffers, depth buffering, viewport/scissor and resize lifecycle;
- Luna Chapter 17: screen-to-ray picking, bounding-volume tests and nearest-hit selection.

Luna Chapter 4 explicitly places creation of the depth/stencil buffer and its DSV in the Direct3D initialization/resize lifecycle. Phase 14 now follows that requirement for the editor viewport target.

The exact Win32 child-window composition, editor overlay, camera bindings, validation-scene layout, procedural cube geometry, hierarchy bridge and gizmo interaction constants are **Design choice (not directly from the book)**.

## 3. What was implemented

### 3.1 Dedicated child presentation target

`EditorShellV3` remains the active and visually authoritative shell. It exposes only a minimal Phase 14 integration seam:

- `ViewportBody()`
- `SceneTree()`
- `ActiveToolId()`

`EditorViewportController` creates a child `renderHost_` inside the existing viewport body. The top-level editor HWND remains chrome-only and never receives the DX12 swap chain.

The existing Perspective / Lit / Show strip remains owned by Phase 13. The render child begins below that strip so the approved shell layout is preserved.

### 3.2 Runtime main-loop ownership preserved

No editor main loop was added.

The execution path remains:

`MainLoop -> Engine::BeginFrame() -> Engine::Tick() -> Engine::EndFrame()`

The viewport controller uses Win32 messages/timer events only to mutate editor camera/tool state. Rendering and presentation still occur through the engine frame lifecycle.

### 3.3 DX12 resize + depth lifecycle

The rendering stack exposes the narrow resize path:

- `Engine::ResizeRenderWindow()`
- `RenderSystem::ResizeAttachedWindow()`
- `Dx12Renderer::ResizeAttachedWindow()`
- `Dx12SwapChain::Resize()`

For a non-zero resize the renderer waits for GPU completion, releases viewport-sized resources, resizes the swap-chain buffers, recreates RTVs, and recreates a matching `D24_UNORM_S8_UINT` depth/stencil resource + DSV.

The depth resource remains in `D3D12_RESOURCE_STATE_DEPTH_WRITE` for this simple Phase 14 forward pass. A 0×0 viewport suspends frame submission without invalid DXGI resize work.

**Design choice (not directly from the book):** the depth target currently lives with `Dx12SwapChain` because that object already owns the viewport-dependent dimensions/back-buffer lifecycle. A larger presentation-target abstraction is intentionally deferred until the codebase proves it necessary.

### 3.4 Depth-tested MeshPass

`MeshPass` now:

- includes the DSV format in the PSO cache key;
- creates a PSO with depth enabled, writes enabled and `LESS` comparison;
- binds RTV + DSV together;
- clears both color and depth every frame;
- renders the Phase 14 validation instances with real depth occlusion.

This is the minimum required to judge a viewport as real spatial 3D rather than a flat triangle smoke test.

### 3.5 Procedural validation cube

The old renderer path hardcoded `Meshes/triangle.nmsh` as its one uploaded geometry. Phase 14 now uploads a colored unit cube procedurally and draws it through the existing instancing path.

**Design choice (not directly from the book):** this does **not** claim that `MeshPass` is now a general multi-mesh renderer. `RenderQueue::mesh` is still not used for per-instance mesh routing by this pass. Refactoring the renderer into a complete multi-mesh/material system is outside this patch and must not be hidden behind the Phase 14 viewport task.

The procedural cube exists specifically so depth, perspective, transform, selection and gizmo behavior can be validated honestly.

### 3.6 Real 3D validation scene

Before the Phase 13 hierarchy is populated, `EditorViewportController::PrepareScene()` creates:

- three spatially separated/selectable cube instances at different positions/depths/scales/rotations;
- one large flattened cube used as a ground/platform reference;
- one editor camera object.

The ground object is deliberately not selectable. The three visible cube objects are selectable.

This scene is **Design choice (not directly from the book)** and exists only as a deterministic Phase 14 validation environment; it is not a replacement for Phase 16 scene authoring.

### 3.7 Editor camera

Controls remain:

- RMB capture + mouse move: yaw/pitch;
- `W/S`: forward/back;
- `A/D`: strafe;
- `Q/E`: down/up;
- `Shift`: faster movement;
- mouse wheel: camera speed adjustment.

Projection aspect follows the actual viewport child dimensions.

### 3.8 Picking and nearest selection

Picking follows the Luna Chapter 17 flow at the current broad-phase precision level:

1. convert viewport-client pixel coordinates to NDC;
2. construct a view-space ray from FOV/aspect;
3. rotate the ray into world space with the editor camera;
4. test transformed world AABBs for every selectable validation cube;
5. choose the valid hit with the smallest positive `t`;
6. clear selection on empty-space click.

Per-triangle mesh intersection remains outside the necessary Phase 14 scope because transformed bounds are sufficient for the current primitive validation scene.

### 3.9 Hierarchy synchronization

Phase 13 exposes only an aggregate `Runtime Objects` row rather than stable authored object rows.

Therefore:

- any viewport cube selection highlights `Runtime Objects`;
- empty-space selection maps back to the root row;
- clicking `Runtime Objects` selects the primary validation cube.

A synchronization guard prevents the synthetic tree update from overwriting which of the three cubes was selected in the viewport.

This is deliberately limited until Phase 15/16 introduce richer object/editor representation.

### 3.10 Selection visualization, gizmos and debug draw

The editor-only color-keyed overlay renders:

- projected ground grid;
- world X/Y/Z origin axes;
- selected cube world AABB;
- transform gizmo axes/handles.

Move/Rotate/Scale now operate on whichever validation cube is selected rather than a single hardcoded object.

No ECS, serialization, undo stack, snapping system, prefab system, lighting/PBR or PIE was introduced.

## 4. Files changed by the 3D validation patch

- `Engine/Render/DX12/Dx12PsoCache.h`
- `Engine/Render/DX12/Dx12SwapChain.h`
- `Engine/Render/DX12/Dx12SwapChain.cpp`
- `Engine/Render/DX12/MeshPass.h`
- `Engine/Render/DX12/MeshPass.cpp`
- `Apps/NocturneEditor/EditorViewportController.h`
- `Apps/NocturneEditor/EditorViewportController.cpp`
- `Docs/Phase 14 — Implementation Report.md`

Earlier Phase 14 work also changed the runtime render-window integration, `main.cpp`, the minimal `EditorShellV3.h` seam and editor project/manifest integration. `EditorShellV3.cpp` remains visually unchanged.

## 5. Verification checklist

### Verified from repository/source diff

- [x] `EditorShellV3` remains the active shell.
- [x] Top-level editor HWND remains unattached to DX12.
- [x] Dedicated child viewport HWND remains the renderer target.
- [x] No second engine/editor main loop exists.
- [x] Viewport-sized depth resource is created and recreated with the viewport target.
- [x] PSO depth testing is enabled and DSV format participates in the cache key.
- [x] Color + depth are both cleared before the validation draw.
- [x] The triangle-only validation geometry has been replaced by a real cube primitive.
- [x] Multiple cube instances at different depths are submitted through the existing runtime `World -> RenderQueue` path.
- [x] Picking chooses the nearest selectable AABB hit.
- [x] Gizmo edits target the currently selected validation cube.
- [x] Phase 13 shell layout/chrome implementation was not redesigned.
- [x] No Phase 15+ ECS/serialization/PIE scope was introduced.

### Requires local Windows validation

- [ ] `Debug x64` build succeeds under the repository's VS2026/v145 setup.
- [ ] DX12 debug layer reports zero errors at launch and during resize.
- [ ] Three colored 3D cubes + ground/platform are visible with correct perspective.
- [ ] Near geometry correctly occludes farther geometry.
- [ ] Renderer output stays confined to the child viewport.
- [ ] Repeated resize/maximize/restore recreates color/depth targets cleanly.
- [ ] Minimize/collapse/restore survives zero-size transitions.
- [ ] RMB fly camera works and remains viewport-focused.
- [ ] Viewport aspect stays correct at arbitrary panel sizes.
- [ ] Clicking each visible cube selects the nearest expected cube.
- [ ] Empty-space click clears selection.
- [ ] Hierarchy aggregate row and viewport selection remain synchronized.
- [ ] Selection bounds track the selected cube after gizmo edits.
- [ ] Move/rotate/scale handles can be dragged repeatedly without instability.
- [ ] Grid/axes/selection/gizmo overlay composites correctly over DX12.

The GitHub connector cannot execute the Windows/MSVC/DX12 binary, so runtime items are intentionally not marked complete.

## 6. Common pitfalls / follow-up risks

- `MeshPass` still renders one geometry instanced N times; do not mistake the Phase 14 procedural cube for finished multi-mesh routing.
- Do not attach the renderer to the editor top-level HWND.
- Do not resize viewport resources while GPU work still references them.
- The DSV must always match the current child viewport extent.
- Do not remove depth testing just to hide geometry/winding problems.
- The hierarchy remains intentionally coarse until later editor/entity phases.
- The Win32 overlay remains a Phase 14 design choice; if compositor testing exposes issues, replace the overlay presentation mechanism without undoing the viewport ownership architecture.

## 7. Next chat handoff

Bring:

1. fresh `Debug x64` build output after pulling this patch;
2. full launch log;
3. one screenshot showing the 3D cubes + ground;
4. one screenshot with a non-primary cube selected and a gizmo active;
5. any DX12 debug-layer, picking, camera, depth, compositing or resize issue observed locally.

Do **not** start Phase 15 until this local verification checklist is closed.
