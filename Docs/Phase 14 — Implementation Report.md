# Phase 14 — Editor Rendering Viewport — Implementation Report

> **Implementation status:** ACTIVE — CORE VIEWPORT / DEPTH / GRID / HIERARCHY / SELECTION FOUNDATION IMPLEMENTED
>
> **Validation status:** PICKING PASSED; CAMERA/NAVIGATION MOSTLY PASSED; MOUSE-WHEEL SPEED CONTROL FAILED; SEVERE FPS STUTTERING REMAINS AN OPEN BLOCKER
>
> **Code baseline covered by this report:** through `436bc5ec4517ff8724d74b73b6619f933437db0f`
>
> **Baseline branch:** `phase-13-editor-framework`
>
> **Active branch:** `phase-14-editor-rendering-viewport`

## 1. Phase name + objective

**Phase 14 — Editor Rendering Viewport**

Replace the Phase 13 central Viewport placeholder with a real DX12-backed editor viewport while preserving `EditorShellV3`, the Phase 13 visual baseline and the runtime-owned `noc::MainLoop`.

The Phase 14 target remains:

- dedicated rendered viewport inside Nocturne Editor;
- editor camera/navigation;
- resize-correct rendering;
- single-object viewport picking;
- selection visualization;
- Select / Move / Rotate / Scale gizmo foundation;
- editor debug rendering for grid, axes, selection and gizmo primitives;
- minimal hierarchy/viewport selection synchronization supported by the current pre-ECS scene representation.

Phase 14 is **not complete yet**. Core viewport functionality is implemented and picking has now passed explicit local validation, but camera speed-wheel behavior, performance stability, gizmo interaction and resize stress validation still remain open.

## 2. Book grounding

The implemented direction remains grounded in:

- Jason Gregory, *Game Engine Architecture (3rd Edition)*, §15.4.1.2 — game-world visualization in editor tooling;
- Gregory §15.4.1.3 — editor viewport navigation;
- Gregory §15.4.1.4 — object selection and synchronization;
- Gregory §15.4.1.7 — object placement and transform aids;
- Frank D. Luna, *Introduction to 3D Game Programming with DirectX 12*, Chapter 4 — swap chain, back buffer, depth buffer, viewport/scissor and resize lifecycle;
- Luna Chapter 17 — screen-space picking ray construction, local/object-space intersection testing and nearest-hit selection.

The exact child-HWND integration, Win32 input routing, procedural validation scene, procedural sky, GPU editor-grid pass, hierarchy bridge, selection debug pass and gizmo UX are **Design choice (not directly from the book)**.

## 3. Implemented Phase 14 functionality

### 3.1 Dedicated DX12 editor viewport

The editor renderer is attached to a dedicated child render host inside the active `EditorShellV3` Viewport body rather than the top-level editor HWND.

The shell exposes only the minimal Phase 14 integration seam required by the viewport controller:

- `ViewportBody()`;
- `SceneTree()`;
- `ActiveToolId()`.

The existing runtime `noc::MainLoop` remains authoritative; Phase 14 does not introduce a second application/render loop.

### 3.2 Resize-aware presentation target

The render-target resize path is routed through:

- `Engine`;
- `RenderSystem`;
- `Dx12Renderer`;
- `Dx12SwapChain`.

Viewport-dependent swap-chain resources are recreated through the DX12 resize path rather than rebuilding renderer/device infrastructure.

### 3.3 Depth buffer and depth-tested 3D rendering

The viewport swap chain owns a depth/stencil target using `DXGI_FORMAT_D24_UNORM_S8_UINT`.

The PSO cache key includes the DSV format, and the mesh pass:

- binds RTV + DSV;
- clears depth each frame;
- enables depth testing and depth writes for scene mesh rendering.

This replaced the earlier triangle-only smoke-test presentation with actual depth-tested 3D validation geometry.

### 3.4 Deterministic Phase 14 validation scene

The current editor validation scene contains:

- `Cube_A`;
- `Cube_B`;
- `Cube_C`;
- `Ground_Plane`;
- `Main Camera`;
- `Environment (Procedural Sky)`.

The three cubes intentionally use different translations, rotations and scales. `Ground_Plane` is represented by the same procedural cube geometry with a large X/Z scale and a very small Y scale.

This scene is **Design choice (not directly from the book)** and exists only to validate Phase 14 editor mechanics before later ECS/scene-authoring phases.

### 3.5 Procedural editor sky and corrected viewport projection

`Data/Shaders/EditorSky.hlsl` provides a simple procedural editor background.

The Phase 14 projection work also corrected the camera/view-matrix convention so the DX12 scene and editor-projected helper geometry use the same Nocturne matrix convention.

This is not the future production lighting/environment system.

### 3.6 GPU editor grid and world axes

The grid is no longer painted by a Win32/GDI overlay.

`Data/Shaders/EditorGrid.hlsl` and the editor LINE PSO render the grid and world axes directly through DX12 using the same camera `viewProj` and depth buffer as scene geometry.

The line pass uses:

- `D3D12_PRIMITIVE_TOPOLOGY_LINELIST`;
- depth test `LESS_EQUAL`;
- no depth writes.

The grid is positioned below the top surface of `Ground_Plane`, allowing the ground geometry to occlude it correctly.

### 3.7 Scene Hierarchy exposure

The Phase 14 hierarchy bridge exposes the validation objects individually:

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

The previous synthetic Win32 click bridge was removed. Selection synchronization updates hierarchy state directly, which eliminated the observed `Runtime Objects` selection flicker.

The hierarchy bridge is temporary editor plumbing for the current pre-ECS `World` representation and must not be mistaken for the later Phase 15/16 scene architecture.

### 3.8 Viewport input ownership

The dedicated render host is the authoritative mouse/focus/capture surface.

It is created with `SS_NOTIFY`, and its subclass returns `HTCLIENT` for `WM_NCHITTEST`. The layered overlay remains visual-only / hit-transparent.

This was added because the earlier `STATIC` child setup did not receive viewport mouse input reliably.

### 3.9 Single-object picking

The current picking path is:

1. viewport pixel -> NDC;
2. NDC -> camera/view ray;
3. view ray -> world ray;
4. world ray -> inverse translation / inverse rotation / inverse scale for each validation object;
5. local ray vs local AABB;
6. nearest positive hit wins.

Testing in local/object space allows the bounds test to follow rotated and non-uniformly scaled validation objects rather than relying on an expanded world-space AABB.

This follows the picking structure described by Luna Chapter 17. The concrete inverse-TRS implementation is **Design choice (not directly from the book)**.

### 3.10 Depth-aware oriented selection outline

Selection visualization is rendered through the GPU editor LINE pass rather than through the layered Win32 overlay.

The debug-selection bridge carries:

- local `boundsMin`;
- local `boundsMax`;
- the selected object's exact world matrix;
- an enabled flag.

`MeshPass` constructs the eight local bounds corners and transforms each corner by the selected object's exact world matrix. The resulting 12 edges therefore follow translation, rotation and non-uniform scale.

Because the selection box uses the depth-tested line pass, rear/occluded edges are not intentionally rendered through the object.

### 3.11 Constant selection-outline clearance

The previous oriented outline used percentage-based expansion:

```cpp
localHalf = localHalf * 1.015f + Vec3(0.008f, 0.008f, 0.008f);
```

That made a very large/thin object such as `Ground_Plane` look noticeably inflated.

Commit `436bc5ec4517ff8724d74b73b6619f933437db0f` replaced that policy with a constant world-space clearance:

```cpp
constexpr float kSelectionWorldOffset = 0.006f;
```

For each oriented local axis, the implementation extracts the axis scale from the selected object's world-matrix columns and converts the fixed world-space clearance back into local units before expanding the local half-extents.

The `0.006f` value and this editor-only clearance policy are **Design choice (not directly from the book)**.

### 3.12 Transform gizmo foundation

The Phase 14 controller contains the foundation for the editor toolbar modes:

- Select;
- Move;
- Rotate;
- Scale.

The current implementation includes gizmo drawing/hit-testing/drag plumbing and applies supported transform changes through the existing `World` TRS interface.

This is a Phase 14 interaction foundation only. It is not the later full scene-authoring, undo/redo or serialization system.

## 4. Local validation results

### 4.1 Picking — PASS

Picking has now been explicitly validated locally against the current validation scene.

Results:

```text
Cube_A: 10/10
Cube_B: 10/10
Cube_C: 10/10
Ground_Plane: 10/10
Empty click clears: YES
Rotated edge accuracy Cube_B: PASS
Rotated edge accuracy Cube_C: PASS
Viewport -> Hierarchy sync: PASS
```

Therefore the primary Phase 14 single-object picking gate is considered **PASS** for the current validation scene.

The implementation also contains nearest-positive-hit selection through `nearestT`. A dedicated overlapping-object stress case has not yet been separately exercised, but this is not currently treated as a blocker because ordinary object picking and post-camera-movement picking both pass.

### 4.2 Camera/navigation — PARTIAL PASS

The following camera/navigation checks have been explicitly validated locally:

```text
RMB mouse-look: PASS
WASD: PASS
Q/E: PASS
Shift speed: PASS
Mouse wheel speed: FAIL
RMB release stops capture: PASS
Focus isolation: PASS
Picking after camera movement: PASS
```

This confirms that editor-camera capture, look, six-axis movement, accelerated movement, focus isolation and selection after camera movement are functioning.

The mouse-wheel camera-speed adjustment remains a confirmed failure and must be investigated before the camera/navigation gate can be closed.

### 4.3 Severe FPS stuttering — OPEN BLOCKER

A **severe FPS stuttering problem** has been observed locally during Phase 14 runtime testing.

At this point no root cause has been established. The report intentionally does not attribute it to the renderer, editor timer, swap chain, camera update, synchronization, debug drawing or any other subsystem without evidence.

This is now an explicit Phase 14 blocker because viewport interaction cannot be considered production-stable while severe frame pacing/stuttering is present.

The next performance investigation must measure and isolate the issue rather than applying speculative fixes.

### 4.4 Other locally observed behavior

The following behavior has also been observed during local Windows testing:

- real DX12 content renders inside the central editor Viewport;
- procedural sky is visible;
- scene renders with depth-tested 3D cube instances;
- GPU grid is occluded by scene cubes rather than composited over them;
- Scene Hierarchy exposes the individual validation objects;
- previous hierarchy `Runtime Objects` flicker is removed;
- oriented selection bounds visually follow rotated cube objects correctly;
- rear selection-box edges are handled through depth-aware GPU rendering rather than always-visible Win32 painting.

## 5. Current known limitations / intentionally deferred work

Phase 14 currently does **not** provide:

- production lighting;
- shadow mapping;
- PBR/material authoring;
- a production sky/cubemap/environment system;
- ECS — Phase 15;
- full scene creation/deletion/component authoring — later phase;
- scene serialization/save/load — later phase;
- production undo/redo transform stack;
- PIE;
- a general final multi-mesh editor rendering architecture.

The procedural validation cubes, colors, ground and sky are therefore expected to look visually basic. Shadows/PBR must not be pulled into Phase 14 only to make the validation scene look more polished.

## 6. Verification checklist

### Verified from repository source/diff

- [x] Dedicated child HWND is used for the DX12 viewport target.
- [x] `EditorShellV3` remains the active shell.
- [x] Runtime `noc::MainLoop` remains authoritative.
- [x] Viewport resize is routed through renderer/swap-chain infrastructure.
- [x] Depth/stencil target is created and recreated with viewport size.
- [x] Scene mesh rendering is depth-tested.
- [x] Procedural sky pass exists.
- [x] GPU depth-tested grid/axes pass exists.
- [x] Individual validation objects are exposed in the hierarchy.
- [x] Synthetic hierarchy mouse-click synchronization was removed.
- [x] Render host uses `SS_NOTIFY`.
- [x] Render-host subclass returns `HTCLIENT` for `WM_NCHITTEST`.
- [x] Layered overlay remains hit-transparent.
- [x] Picking uses inverse-TRS local-space bounds tests.
- [x] Selection debug data carries local bounds plus exact world matrix.
- [x] Selection geometry follows translation/rotation/non-uniform scale.
- [x] Selection lines use depth testing and do not write depth.
- [x] Percentage-based outline inflation was removed.
- [x] Selection clearance is now a constant ~`0.006` world units converted per local axis.
- [x] No ECS/serialization/PIE/lighting/PBR/shadow scope was introduced.

### Locally confirmed — picking

- [x] Repeated direct clicks select `Cube_A` reliably — 10/10.
- [x] Repeated direct clicks select rotated `Cube_B` reliably — 10/10.
- [x] Repeated direct clicks select rotated `Cube_C` reliably — 10/10.
- [x] Repeated direct clicks select `Ground_Plane` reliably — 10/10.
- [x] Empty viewport/sky click clears selection.
- [x] Rotated-edge picking accuracy passes for `Cube_B`.
- [x] Rotated-edge picking accuracy passes for `Cube_C`.
- [x] Viewport selection consistently selects the matching hierarchy row.
- [x] Picking remains correct after camera movement.

### Locally confirmed — camera/navigation

- [x] RMB mouse-look works.
- [x] WASD movement works.
- [x] Q/E vertical movement works.
- [x] Shift accelerated movement works.
- [x] RMB release stops capture.
- [x] Camera input focus isolation works.
- [ ] Mouse-wheel camera-speed adjustment works. **FAIL — open defect.**

### Still requires explicit local validation or investigation before Phase 14 completion

- [ ] Diagnose and fix severe FPS/frame-pacing stuttering. **OPEN BLOCKER.**
- [ ] Re-test mouse-wheel camera-speed adjustment after fix.
- [ ] Latest constant-clearance outline is visually correct on `Ground_Plane`.
- [ ] Hierarchy selection consistently selects the matching viewport object after all remaining fixes.
- [ ] Projection/aspect remains correct through viewport resizing.
- [ ] Move gizmo interaction works reliably.
- [ ] Rotate gizmo interaction works reliably.
- [ ] Scale gizmo interaction works reliably.
- [ ] Resize/maximize/restore is stable across repeated operations.
- [ ] Zero/minimized viewport size paths remain safe.
- [ ] DX12 debug layer reports no relevant errors during the final validation pass.
- [ ] Phase 13 menu/toolbar/panel chrome/Content Browser/Console/status bar/Tabler icon baseline remains intact after final fixes.

The GitHub connector cannot execute the local Windows/MSVC/DX12 editor binary, so runtime checks remain dependent on explicit local testing.

## 7. Common pitfalls

- Do not use a transformed world AABB as the oriented selection visualization; it intentionally loses object orientation.
- Do not reintroduce percentage-based selection-box expansion for very large/scaled objects.
- Do not move viewport mouse ownership back to the layered color-key overlay.
- Do not move the GPU grid back into a Win32/GDI overlay; it must remain depth-aware.
- Keep gizmo always-visible policy separate from depth-aware object-selection visualization.
- Do not introduce a second engine/device/main loop for the editor viewport.
- Do not attach the swap chain to the top-level editor HWND.
- Do not pull shadows/PBR/material authoring into Phase 14 as visual polish.
- Do not guess at the cause of the FPS stuttering; measure and isolate it first.
- Do not mark Phase 14 complete while the stuttering blocker, mouse-wheel defect and remaining gizmo/resize checks are open.

## 8. Files substantially introduced/changed by Phase 14

The cumulative Phase 14 implementation touches the editor integration and rendering paths including:

- `Apps/NocturneEditor/EditorShellV3.h`;
- `Apps/NocturneEditor/EditorViewportController.h/.cpp`;
- `Apps/NocturneEditor/main.cpp`;
- `Apps/NocturneEditor/NocturneEditor.manifest`;
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
- `Ide/VS2026/NocturneEditor/NocturneEditor.vcxproj`;
- this implementation report.

## 9. Next validation focus

Before moving on to final gizmo/resize sign-off:

1. investigate and isolate the severe FPS/frame-pacing stuttering;
2. diagnose the failed mouse-wheel camera-speed behavior;
3. validate/fix both in code if necessary;
4. re-run camera/navigation validation;
5. then continue with Move / Rotate / Scale gizmo interaction and resize/maximize/restore stress checks.

Do **not** mark Phase 14 complete until the remaining defects and runtime checks pass.
