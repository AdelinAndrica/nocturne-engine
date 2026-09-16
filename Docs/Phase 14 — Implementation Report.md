# Phase 14 — Editor Rendering Viewport — Implementation Report

> **Implementation status:** ACTIVE — VIEWPORT INPUT + ORIENTED SELECTION FIX IMPLEMENTED ON `phase-14-editor-rendering-viewport`
>
> **Validation status:** SOURCE/DIFF VALIDATED; LOCAL WINDOWS BUILD + INTERACTIVE GPU/UI VALIDATION REQUIRED
>
> **Baseline:** `phase-13-editor-framework`

## 1. Phase name + objective

**Phase 14 — Editor Rendering Viewport**

Replace the Phase 13 central placeholder with a real DX12-backed editor viewport while preserving `EditorShellV3`, the approved Phase 13 chrome and the runtime-owned main loop.

This patch specifically addresses the two remaining interaction/visualization defects observed locally after the previous selection patch:

- direct viewport mouse selection did not reliably reach the DX12 render child;
- selection bounds for rotated objects were generated from a world-space AABB and therefore did not follow the object's actual rotation.

## 2. Key concepts from the books

The fix remains grounded in:

- Jason Gregory, *Game Engine Architecture (3rd Edition)*, §15.4.1.2 — game-world visualization in editor tooling;
- Gregory §15.4.1.3 — editor viewport navigation;
- Gregory §15.4.1.4 — object selection and synchronization;
- Gregory §15.4.1.7 — object placement and transform aids;
- Frank D. Luna, *Introduction to 3D Game Programming with DirectX 12*, Chapter 4 — viewport/depth-buffer rendering fundamentals;
- Luna Chapter 17 — screen-space picking, ray construction, bounding-volume intersection and nearest-hit selection.

The exact Win32 child-control input policy, `SS_NOTIFY`/`WM_NCHITTEST` handling, inverse-TRS picking implementation and debug-selection render bridge are **Design choice (not directly from the book)**.

## 3. What changed in this patch

### 3.1 Deterministic render-host mouse input

The dedicated DX12 viewport target is currently a Win32 `STATIC` child control. The previous patch subclassed that HWND for mouse handling but created the control without `SS_NOTIFY`. A STATIC control can otherwise behave as a passive/transparent UI element for mouse interaction, making direct viewport input unreliable.

The render host is now created with:

- `WS_CHILD`;
- `WS_VISIBLE`;
- `WS_CLIPSIBLINGS`;
- `WS_CLIPCHILDREN`;
- `WS_TABSTOP`;
- `SS_NOTIFY`.

Its subclass also handles `WM_NCHITTEST` explicitly and returns `HTCLIENT`. The layered visual overlay continues to return `HTTRANSPARENT`.

This establishes one explicit input owner:

- overlay = visual-only editor layer;
- render host = authoritative viewport mouse/focus/capture surface.

RMB camera capture, LMB selection, wheel speed and gizmo drag therefore all route through the same child HWND.

### 3.2 Picking remains local-space / inverse-TRS

The previous broad world-AABB test remains replaced by object-space testing:

1. viewport pixel -> NDC;
2. NDC -> camera/view ray;
3. ray rotated into world space;
4. world ray transformed by inverse translation / inverse rotation / inverse scale for each validation object;
5. local ray tested against the original local AABB;
6. nearest positive hit wins.

The direction is intentionally not re-normalized after inverse scale so the ray parameter remains comparable across objects with different scales.

This structure is aligned with Luna Chapter 17; the concrete inverse-TRS implementation is **Design choice (not directly from the book)**.

### 3.3 Selection visualization now uses oriented local bounds

The previous GPU selection pass was depth-aware, but its geometry still came from `TransformAabb(...)` / world-space AABB data. That is correct for broad-phase bounds, but it is not a correct visual representation of a rotated object's local box: the resulting box stays aligned to world axes and grows to contain the rotated object.

The debug-selection bridge now carries:

- selected object's local `boundsMin` / `boundsMax`;
- selected object's exact world matrix (`TRS`);
- enabled flag.

`MeshPass` constructs the eight local corners, applies a very small local-space expansion to avoid z-fighting, and transforms every corner through the selected object's world matrix with `TransformPoint`.

The 12 resulting world-space edges therefore follow:

- translation;
- rotation;
- non-uniform scale.

They are still drawn through the existing depth-tested editor LINE PSO (`LESS_EQUAL`, no depth writes), so hidden/rear edges remain occluded by scene depth.

This oriented debug-box implementation is **Design choice (not directly from the book)**.

### 3.4 Scope remains Phase 14

This patch does not introduce:

- ECS;
- serialization;
- scene-authoring persistence;
- PBR/material system;
- shadow mapping;
- production lighting.

Those remain later roadmap work. Phase 14 continues to validate editor viewport mechanics first.

## 4. Files changed by this patch

- `Apps/NocturneEditor/EditorViewportController.cpp`
- `Engine/Render/RenderQueue.h`
- `Engine/Runtime/Engine.h`
- `Engine/Runtime/Engine.cpp`
- `Engine/Render/DX12/MeshPass.cpp`
- `Docs/Phase 14 — Implementation Report.md`

`EditorShellV3` remains unchanged and continues to be the active shell.

## 5. Verification checklist

### Verified from source/diff

- [x] Render host uses `SS_NOTIFY`.
- [x] Render-host subclass returns `HTCLIENT` for `WM_NCHITTEST`.
- [x] Layered overlay remains hit-transparent.
- [x] Picking still uses inverse-TRS local-space bounds tests.
- [x] Debug selection carries local bounds plus the selected world matrix.
- [x] Selection geometry transforms local corners by the selected object's world matrix.
- [x] Selection lines remain depth-tested and do not write depth.
- [x] Grid remains below the Ground Plane.
- [x] No lighting/PBR/shadow/ECS scope was introduced.

### Requires local Windows validation

- [ ] `Debug x64` builds with zero errors.
- [ ] DX12 debug layer reports no errors.
- [ ] Repeated direct clicks select `Cube_A` reliably.
- [ ] Repeated direct clicks select rotated `Cube_B` reliably.
- [ ] Repeated direct clicks select rotated `Cube_C` reliably.
- [ ] Ground Plane can be selected directly from visible areas.
- [ ] Empty viewport/sky click clears selection.
- [ ] Viewport selection selects the matching hierarchy row.
- [ ] Hierarchy selection still selects the matching viewport object without flicker.
- [ ] Selection outline follows Cube_B/C rotation exactly.
- [ ] Selection outline follows scale changes exactly.
- [ ] Rear/occluded outline edges remain hidden by depth testing.
- [ ] RMB camera capture still works.
- [ ] Move/Rotate/Scale gizmo mouse capture still works.
- [ ] Resize/maximize/restore remains stable.

The GitHub connector cannot execute the Windows/MSVC/DX12 binary, so these runtime checks remain intentionally open.

## 6. Common pitfalls

- Do not use `TransformAabb()` output as the editor's oriented selection visualization. A world AABB is appropriate for broad-phase/culling but intentionally loses object orientation.
- Do not move viewport mouse ownership back to the color-keyed overlay.
- Keep the render child as the authoritative mouse/focus/capture HWND.
- Keep gizmo visibility policy separate from depth-aware selection-bound policy.
- Do not pull shadows/PBR into Phase 14 to compensate for the deliberately simple validation rendering.

## 7. Next chat handoff

Bring:

1. fresh `Debug x64` build output after pulling this commit;
2. screenshot with rotated `Cube_B` or `Cube_C` selected;
3. confirmation whether repeated direct viewport clicks select all three cubes reliably;
4. confirmation that RMB camera and gizmo dragging still work;
5. any DX12 debug-layer errors.

Do not mark Phase 14 complete until these runtime checks pass.
