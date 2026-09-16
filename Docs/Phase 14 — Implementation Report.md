# Phase 14 — Editor Rendering Viewport — Implementation Report

> **Implementation status:** ACTIVE — DEPTH-AWARE SELECTION + RELIABLE VIEWPORT PICKING PATCH PREPARED ON `phase-14-editor-rendering-viewport`
>
> **Validation status:** SOURCE/DIFF VALIDATION REQUIRED; LOCAL WINDOWS BUILD + INTERACTIVE GPU/UI VALIDATION REQUIRED AFTER PULL
>
> **Baseline:** `phase-13-editor-framework`

## 1. Phase name + objective

**Phase 14 — Editor Rendering Viewport**

Replace the Phase 13 central placeholder with a real DX12-backed editor viewport while preserving `EditorShellV3`, the approved Phase 13 chrome and the runtime-owned main loop.

The current implementation includes a dedicated child render HWND, resize-aware color/depth targets, procedural sky, a depth-tested validation scene, GPU grid/axes, editor camera, explicit hierarchy rows, nearest-hit picking, depth-aware selection visualization and transform-gizmo foundations.

## 2. Key concepts from the books

The Phase 14 design remains grounded in:

- Jason Gregory, *Game Engine Architecture (3rd Edition)*, §15.4.1.2 — game-world visualization in editor tooling;
- Gregory §15.4.1.3 — editor viewport navigation;
- Gregory §15.4.1.4 — object selection and synchronization with list/tree representations;
- Gregory §15.4.1.7 — transform/placement handles;
- Frank D. Luna, *Introduction to 3D Game Programming with DirectX 12*, Chapter 4 — swap chain, depth/stencil buffer, viewport/scissor and resize lifecycle;
- Luna Chapter 17 — screen-to-ray picking, bounding-volume intersection and nearest-hit selection.

The child-HWND composition, procedural sky, GPU editor-line pass, validation-scene layout, explicit Phase 14 hierarchy bridge, render-host input routing and debug-selection bridge are **Design choice (not directly from the book)**.

## 3. Current implementation

### 3.1 Dedicated viewport + depth lifecycle

DX12 is attached only to the dedicated child render host. `noc::MainLoop` remains authoritative. Viewport resize recreates the swap-chain buffers and matching `D24_UNORM_S8_UINT` depth target; zero-size states suspend rendering safely.

### 3.2 Validation scene and hierarchy

The deterministic Phase 14 scene currently exposes:

- `Cube_A`;
- `Cube_B`;
- `Cube_C`;
- `Ground_Plane`;
- `Main Camera`;
- `Environment (Procedural Sky)`.

The hierarchy shows those runtime validation objects explicitly beneath `Runtime Objects (4)`. Synthetic tree clicks were removed, so hierarchy selection no longer feeds back through fake Win32 mouse messages.

### 3.3 GPU grid below Ground Plane

The editor grid/world axes are DX12 line geometry using the viewport depth buffer. The line PSO uses `LESS_EQUAL` and does not write depth.

The previous grid height was `Y = -1.14`, while the flattened Ground Plane top is `Y = -1.15`. That intentionally placed the grid 1 cm *above* the ground and therefore made it visible across the platform.

This patch changes the policy:

- grid plane: `Y = -1.16`, 1 cm below the Ground Plane top;
- Ground Plane therefore occludes grid lines inside its footprint;
- world axes remain slightly above the surface as an editor orientation aid.

This exact offset is **Design choice (not directly from the book)**.

### 3.4 Depth-aware selection bounds

The previous selection box was drawn by the color-keyed Win32 overlay. Overlay pixels are composited after DX12 and cannot query the scene depth buffer, so all 12 AABB edges remained visible, including edges physically behind the selected cube.

The new path carries a small `RenderDebugSelection` record with the per-frame `RenderQueue`. `Engine` exposes a narrow generic debug-selection API and `MeshPass` draws the selected bounds through the existing editor LINE PSO:

- depth testing enabled;
- depth writes disabled;
- `LESS_EQUAL` comparison;
- one 24-vertex upload buffer per frame-in-flight;
- bounds are expanded slightly to keep front edges off the selected surface and avoid z-fighting;
- rear/occluded edges fail the scene depth test and are not visible through the object.

The Win32 overlay is now reserved for intentionally always-visible gizmo handles only.

The debug-selection handoff is **Design choice (not directly from the book)**.

### 3.5 Reliable viewport input routing

The color-keyed layered overlay previously also owned mouse handling. Transparent/color-keyed pixels can allow hit-testing to fall through to the DX12 child, while the DX12 child had no Phase 14 mouse handler. That made viewport selection intermittent.

The new ownership is explicit:

- overlay returns `HTTRANSPARENT` and is visual-only;
- `renderHost_` is subclassed as the single viewport input target;
- RMB camera capture, LMB picking, mouse movement, wheel speed and gizmo dragging are handled from the render host;
- mouse capture/focus is owned by `renderHost_` rather than the overlay.

This removes compositor transparency from the input path.

### 3.6 Oriented-object picking

Picking still follows the Luna Chapter 17 flow, but the object test is improved.

Old path:

1. construct a world-space ray;
2. transform each object's local bounds to a world-space AABB;
3. ray-test that enlarged world AABB.

New path:

1. viewport pixel -> NDC;
2. NDC -> view-space ray using viewport FOV/aspect;
3. rotate ray into world space with the editor camera;
4. for each candidate object, transform ray origin/direction through inverse translation, inverse rotation and inverse scale;
5. test the original local AABB in object space;
6. keep the smallest positive ray parameter.

Because the local ray direction is not renormalized after inverse scale, the returned slab parameter remains comparable between differently scaled objects. Rotated cubes therefore no longer depend on their enlarged world AABB for picking.

The exact inverse-TRS implementation is **Design choice (not directly from the book)**; the screen-ray / bounds / nearest-hit structure is grounded in Luna Chapter 17.

### 3.7 Visual-quality boundary

The procedural sky and per-face validation colors are intentionally not a finished lighting solution. Real-time shadows, production lighting, material/PBR response and related rendering polish remain later renderer phases in the Nocturne roadmap.

Phase 14 must validate editor viewport mechanics correctly before those systems are introduced. This patch therefore does **not** pretend to implement shadows as an editor-specific shortcut.

## 4. Files changed by this patch

- `Engine/Render/RenderQueue.h`
- `Engine/Runtime/Engine.h`
- `Engine/Runtime/Engine.cpp`
- `Engine/Render/DX12/MeshPass.h`
- `Engine/Render/DX12/MeshPass.cpp`
- `Apps/NocturneEditor/EditorViewportController.h`
- `Apps/NocturneEditor/EditorViewportController.cpp`
- `Docs/Phase 14 — Implementation Report.md`

`EditorShellV3` remains the active editor shell and its Phase 13 panel/layout/chrome is not redesigned.

## 5. Verification checklist

### Already observed locally before this patch

- [x] Procedural sky is visible.
- [x] Hierarchy displays `Cube_A`, `Cube_B`, `Cube_C`, `Ground_Plane`, camera and environment.
- [x] Previous `Runtime Objects` hierarchy flicker is no longer observed.
- [x] Cubes occlude the GPU grid correctly.
- [x] Grid was still visible over the Ground Plane, motivating this patch.
- [x] Win32 selection outline showed hidden/rear AABB edges, motivating this patch.
- [x] Direct viewport picking was intermittent, motivating this patch.

### Verified from source design for this patch

- [x] Overlay no longer owns viewport mouse input.
- [x] Render host has a dedicated input subclass.
- [x] Overlay hit-testing is transparent.
- [x] Picking tests local bounds through inverse TRS.
- [x] Selection bounds are submitted through a depth-tested GPU line pass.
- [x] Selection upload memory is duplicated per frame-in-flight.
- [x] Grid is below the Ground Plane top surface.
- [x] No ECS/serialization/PBR/shadow-system scope was introduced.

### Requires local Windows validation after pulling this commit

- [ ] `Debug x64` build succeeds with zero compile/link errors.
- [ ] DX12 debug layer reports no errors.
- [ ] Ground Plane hides grid lines inside its footprint.
- [ ] Grid remains visible outside the Ground Plane.
- [ ] Selected cube shows only depth-visible selection edges; rear edges do not show through the cube.
- [ ] Another cube can occlude the selected outline correctly.
- [ ] Clicking `Cube_A`, `Cube_B` and `Cube_C` directly in the viewport works consistently.
- [ ] Rotated `Cube_B` / `Cube_C` picking is reliable.
- [ ] Ground Plane can be selected from the viewport where it is visible.
- [ ] Empty sky/space click clears selection.
- [ ] Hierarchy -> viewport selection remains stable and flicker-free.
- [ ] RMB camera navigation still works through the render host.
- [ ] Move/Rotate/Scale gizmo drag still receives mouse capture correctly.
- [ ] Resize/maximize/restore remains stable.

The GitHub connector cannot execute the Windows/MSVC/DX12 binary, so those runtime checks remain intentionally open.

## 6. Common pitfalls / follow-up risks

- Do not move depth-dependent selection visualization back into the layered overlay.
- Keep transform gizmos intentionally visible even when selection bounds obey depth.
- Do not route viewport input through a color-keyed visual overlay again.
- The explicit hierarchy objects are Phase 14 validation metadata, not the final ECS/scene authoring model.
- `MeshPass` still renders one procedural validation geometry instanced N times.
- Real shadows/lighting/PBR must be added in their renderer phases rather than as Phase 14 editor-only special cases.

## 7. Next chat handoff

Bring:

1. fresh `Debug x64` build output after pulling the commit;
2. screenshot with a cube selected from the viewport;
3. screenshot showing Ground Plane hiding the grid beneath it;
4. confirmation that all three cubes can be selected repeatedly by direct viewport clicking;
5. any DX12 debug-layer or gizmo/camera regression.

Do not mark Phase 14 complete until the post-patch runtime checklist is closed.
