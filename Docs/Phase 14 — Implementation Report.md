# Phase 14 — Editor Rendering Viewport — Implementation Report

> **Implementation status:** IMPLEMENTED ON `phase-14-editor-rendering-viewport`
>
> **Validation status:** SOURCE/DIFF VALIDATED; LOCAL WINDOWS BUILD + INTERACTIVE GPU VALIDATION REQUIRED
>
> **Baseline:** `phase-13-editor-framework`

## 1. Phase name + objective

**Phase 14 — Editor Rendering Viewport**

Replace only the central Phase 13 viewport placeholder with a DX12-backed child render surface while preserving `EditorShellV3`, the approved Phase 13 chrome, and the runtime-owned main loop.

The implementation adds:

- a dedicated child HWND presentation target inside the existing viewport body;
- resize-aware DXGI back-buffer recreation;
- real runtime scene rendering in the editor viewport;
- an editor fly camera;
- click picking against the current Phase 14 renderable;
- viewport/hierarchy selection synchronization at the level supported by the Phase 13 hierarchy;
- selection bounds visualization;
- translate / rotate / scale gizmo interaction foundation;
- editor-only grid, origin axes and gizmo debug drawing.

## 2. Key concepts from the books

The implementation follows the references already locked in `Docs/Phase 14 — Editor Rendering Viewport Handoff.md`:

- Jason Gregory, *Game Engine Architecture (3rd Edition)*, §15.4.1.2: game-world visualization inside editor tooling;
- §15.4.1.3: editor viewport navigation;
- §15.4.1.4: selection and synchronization with tree/list representations;
- §15.4.1.7: placement/alignment handles for transforms;
- Frank D. Luna, *Introduction to 3D Game Programming with DirectX 12*, Chapter 4: swap-chain/back-buffer/viewport lifecycle;
- Luna, Chapter 17: screen-to-ray picking and nearest-hit selection principles.

The exact Win32 child-window composition, editor overlay, control bindings, Phase 13 hierarchy bridge and gizmo interaction constants are **Design choice (not directly from the book)**.

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

The existing execution path remains:

`MainLoop -> Engine::BeginFrame() -> Engine::Tick() -> Engine::EndFrame()`

The viewport controller uses ordinary Win32 messages/timer events only to mutate editor camera/tool state. Rendering and presentation still occur exclusively through the engine frame lifecycle.

### 3.3 DX12 resize lifecycle

The rendering stack now exposes a narrow resize path:

- `Engine::ResizeRenderWindow()`
- `RenderSystem::ResizeAttachedWindow()`
- `Dx12Renderer::ResizeAttachedWindow()`
- `Dx12SwapChain::Resize()`

For a non-zero resize the renderer:

1. waits for GPU completion using the existing `Dx12FrameSync` / queue;
2. releases swap-chain back-buffer references;
3. calls `IDXGISwapChain::ResizeBuffers`;
4. refreshes the current back-buffer index;
5. recreates RTVs;
6. resets tracked back-buffer states to `PRESENT`.

A 0×0 editor viewport suspends frame submission without calling `ResizeBuffers(0, 0)`.

### 3.4 Real viewport dimensions used by the runtime camera

The old hard-coded `1280 × 720` values in `Engine::EndFrame()` were removed.

`Engine` now tracks the actual attached render-target extent and supplies it to `World::BuildRenderQueue()`. This keeps the runtime camera projection aspect synchronized with the child viewport dimensions.

### 3.5 Editor camera

`EditorViewportController` owns editor-only camera state and drives the existing runtime `World` camera object.

Controls:

- RMB capture + mouse move: yaw/pitch;
- `W/S`: forward/back;
- `A/D`: strafe;
- `Q/E`: down/up;
- `Shift`: faster movement;
- mouse wheel: camera speed adjustment.

Camera movement is active only while the viewport owns RMB capture.

### 3.6 Picking and selection

Phase 14 currently creates one deterministic visible triangle renderable plus an editor camera object before `EditorShellV3` populates its Phase 13 hierarchy.

Viewport picking:

1. converts the clicked pixel to normalized device coordinates;
2. creates a view-space ray using the current FOV/aspect;
3. rotates that ray into world space with the editor camera orientation;
4. intersects it with the renderable's transformed AABB;
5. selects the object on hit;
6. clears selection when clicking empty space.

This is the Phase 14 coarse-selection foundation. Per-triangle mesh picking remains outside this phase.

### 3.7 Hierarchy synchronization

The Phase 13 hierarchy does not yet expose authored per-entity rows; Phase 15/16 are intentionally not pulled forward.

Therefore Phase 14 maps the existing `Runtime Objects` hierarchy row to its single selectable renderable:

- viewport hit -> selects the `Runtime Objects` row;
- viewport empty click -> selects the root row / clears object selection;
- clicking `Runtime Objects` -> selects the viewport object.

This is intentionally limited to the level supported by the current Phase 13 hierarchy and current pre-ECS scene model.

### 3.8 Selection visualization and debug draw

An editor-only color-keyed Win32 overlay is layered above the DX12 child target. It renders:

- ground grid;
- world X/Y/Z origin axes;
- selected world-space AABB;
- transform gizmo axes and handles.

This avoids adding editor concerns to runtime mesh/material rendering.

**Design choice (not directly from the book):** the Phase 14 debug primitives are a Win32 editor overlay rather than a new runtime GPU debug-draw subsystem. A generalized GPU debug-draw facility can be introduced later without changing the Phase 14 viewport ownership model.

### 3.9 Gizmo foundation

The existing Phase 13 toolbar remains authoritative for tool mode:

- Select
- Move
- Rotate
- Scale

When an object is selected, axis handles are projected from the object's transform. Clicking and dragging a handle updates the same runtime `World` object transform used for rendering.

Implemented behavior:

- Move: drag along the selected local axis;
- Scale: change one selected local scale component with a positive floor;
- Rotate: apply an axis-angle delta around the selected local axis.

No ECS, serialization, undo stack, snapping system or prefab integration was introduced.

## 4. Files changed

### Editor

- `Apps/NocturneEditor/main.cpp`
- `Apps/NocturneEditor/EditorShellV3.h`
- `Apps/NocturneEditor/EditorViewportController.h` — new
- `Apps/NocturneEditor/EditorViewportController.cpp` — new
- `Ide/VS2026/NocturneEditor/NocturneEditor.vcxproj`

### Runtime/render integration

- `Engine/Runtime/Engine.h`
- `Engine/Runtime/Engine.cpp`
- `Engine/Render/RenderSystem.h`
- `Engine/Render/RenderSystem.cpp`
- `Engine/Render/DX12/Dx12Renderer.h`
- `Engine/Render/DX12/Dx12Renderer.cpp`
- `Engine/Render/DX12/Dx12SwapChain.h`
- `Engine/Render/DX12/Dx12SwapChain.cpp`

`EditorShellV3.cpp`, `EditorTheme`, Content Browser, Console, status bar, toolbar composition and Tabler icon rendering were not redesigned.

## 5. Verification checklist

### Verified from repository/source diff

- [x] Branch is based directly on `phase-13-editor-framework` and is ahead with no divergence at implementation time.
- [x] `EditorShellV3` remains the active shell.
- [x] Historical `EditorShell` / `EditorControls` were not used as the Phase 14 implementation path.
- [x] Top-level editor HWND remains unattached to DX12.
- [x] Dedicated child viewport HWND is the renderer target.
- [x] No second engine/editor main loop was introduced.
- [x] Actual viewport width/height replace the old hard-coded render extent.
- [x] Resize uses the existing device, queue and fence infrastructure.
- [x] 0×0 presentation is suppressed.
- [x] Camera/picking/gizmo/debug-draw code is editor-owned.
- [x] Phase 13 shell chrome/layout implementation was left intact.
- [x] No Phase 15+ ECS/serialization/PIE scope was introduced.

### Requires local Windows validation

- [ ] `Debug x64` build succeeds under the repository's VS2026/v145 setup.
- [ ] DX12 debug layer reports no errors during repeated resize/maximize/restore.
- [ ] Renderer output stays confined to the child viewport.
- [ ] Minimize/collapse/restore survives repeated 0×0 transitions.
- [ ] RMB fly camera controls feel correct and remain viewport-focused.
- [ ] Viewport aspect remains visually correct at arbitrary panel sizes.
- [ ] Clicking the triangle selects it; clicking empty space clears it.
- [ ] Hierarchy aggregate row and viewport selection remain synchronized.
- [ ] Selection bounds track the object after gizmo edits.
- [ ] Move/rotate/scale handles can be dragged repeatedly without instability.
- [ ] Grid/axes/selection/gizmo overlay remains correctly composited over DX12 on supported Windows versions.

The GitHub connector used for this implementation cannot execute the Windows/MSVC/DX12 binary, so these runtime items are deliberately not marked complete.

## 6. Common pitfalls / follow-up risks

- Do not attach the renderer to the editor top-level HWND; the child target is now an architectural boundary.
- Do not call swap-chain resize while an engine frame is open. The editor resize path is message-driven before frame submission and the renderer rejects an open-frame resize.
- Do not convert the editor timer into a second simulation/render loop.
- Do not move editor grid/gizmo behavior into gameplay/runtime object policy.
- The Phase 13 hierarchy is intentionally coarse. Do not expand Phase 14 into Phase 15 ECS or Phase 16 scene editing merely to obtain richer hierarchy rows.
- The Win32 overlay is a Phase 14 design choice; if it proves unsuitable under local GPU/compositor testing, replace only the debug-draw presentation mechanism, not the child-HWND/runtime-loop architecture.

## 7. Next chat handoff

Bring:

1. the `Debug x64` build output for `NocturneEditor`;
2. DX12 debug-layer output from launch + resize/maximize/restore;
3. one screenshot of the viewport after launch;
4. one screenshot with the triangle selected and a transform gizmo active;
5. any camera, picking, compositing or resize issue observed locally.

Do **not** start Phase 15 until the local verification checklist above is closed and this report can be promoted from implementation-complete to fully validated.
