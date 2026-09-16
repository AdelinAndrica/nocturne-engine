# Phase 14 — Editor Rendering Viewport — Implementation Report

> **Implementation status:** ACTIVE — DEPTH-TESTED EDITOR GRID + EXPLICIT HIERARCHY PATCH IMPLEMENTED ON `phase-14-editor-rendering-viewport`
>
> **Validation status:** SOURCE/DIFF VALIDATED; LOCAL WINDOWS BUILD + INTERACTIVE GPU/UI VALIDATION REQUIRED
>
> **Baseline:** `phase-13-editor-framework`

## 1. Phase name + objective

**Phase 14 — Editor Rendering Viewport**

Replace the Phase 13 central placeholder with a real DX12-backed editor viewport while preserving `EditorShellV3`, the approved Phase 13 chrome and the runtime-owned main loop.

The current implementation includes:

- a dedicated child HWND presentation target inside the Viewport panel;
- resize-aware swap-chain + depth-buffer lifecycle;
- depth-tested 3D validation geometry;
- a procedural editor sky;
- a depth-tested GPU editor grid and world axes;
- an editor fly camera;
- nearest-hit picking;
- explicit Phase 14 hierarchy rows for the actual validation objects;
- bidirectional viewport/hierarchy object selection;
- selection bounds and transform-gizmo foundation.

## 2. Key concepts from the books

The Phase 14 design remains grounded in:

- Jason Gregory, *Game Engine Architecture (3rd Edition)*, §15.4.1.2 — game-world visualization in editor tooling;
- Gregory §15.4.1.3 — editor viewport navigation;
- Gregory §15.4.1.4 — object selection and synchronization with list/tree representations;
- Gregory §15.4.1.7 — transform/placement handles;
- Frank D. Luna, *Introduction to 3D Game Programming with DirectX 12*, Chapter 4 — swap chain, back buffers, depth/stencil buffer, viewport/scissor and resize lifecycle;
- Luna Chapter 17 — screen-to-ray picking, bounds rejection and nearest-hit selection.

The exact child-HWND composition, procedural sky, GPU editor-grid pass, hierarchy bridge, validation-scene naming and Win32 selection integration are **Design choice (not directly from the book)**.

## 3. Current implementation

### 3.1 Dedicated viewport target and depth lifecycle

DX12 remains attached only to the dedicated child render host, never to the top-level editor HWND. `noc::MainLoop` remains authoritative; no second editor render loop exists.

The viewport resize path recreates swap-chain back buffers and the matching `D24_UNORM_S8_UINT` depth/stencil resource. Zero-sized viewport states suppress invalid resize/render work.

### 3.2 Procedural sky and camera projection

The viewport renders a fullscreen procedural sky before scene geometry. This supplies spatial context without introducing cubemaps, lighting or PBR before their roadmap phases.

The engine math path uses the Nocturne convention of column-major matrices with column vectors; the view basis is constructed consistently with that convention so the GPU scene and editor projection helpers use the same camera orientation.

### 3.3 Depth-tested validation scene

Phase 14 uses one procedural cube geometry through the existing instancing path. The deterministic validation scene contains:

- `Cube_A`;
- `Cube_B`;
- `Cube_C`;
- `Ground_Plane` (implemented at this stage as a flattened cube instance);
- the editor/main camera;
- `Environment (Procedural Sky)` as an editor hierarchy entry.

This scene is **Design choice (not directly from the book)** and exists to validate perspective, depth, picking and transforms. It does not replace Phase 16 scene authoring and does not claim that the current renderer is a finished multi-mesh/material renderer.

### 3.4 GPU editor grid and world axes

The previous implementation drew the grid and origin axes through the layered Win32 overlay. Because that overlay is composited after DX12, grid lines always appeared above the 3D geometry and could not participate in depth testing.

This patch removes grid/world-axis drawing from `EditorViewportController::PaintOverlay_()` and adds a narrow DX12 editor-grid path:

- static POSITION/COLOR line geometry;
- dedicated `EditorGrid.hlsl`;
- `D3D12_PRIMITIVE_TOPOLOGY_LINELIST`;
- depth testing enabled;
- `LESS_EQUAL` depth comparison;
- depth writes disabled;
- grid drawn after opaque validation objects;
- grid plane at `Y = -1.14`, 1 cm above the validation ground's top surface (`Y = -1.15`) to avoid direct coplanar z-fighting;
- world X/Y/Z axes submitted in the same depth-tested line pass.

**Design choice (not directly from the book):** the Phase 14 grid is a small GPU debug/editor pass rather than a generalized debug-draw service. The important architectural property is that world-space editor aids which must obey occlusion now share the viewport's depth buffer.

The layered Win32 overlay remains only for intentionally always-visible editor interaction feedback:

- selected-object bounds;
- transform-gizmo handles.

### 3.5 Explicit Scene Hierarchy bridge

The previous Phase 14 bridge displayed only an aggregate `Runtime Objects: N` row. Selecting a viewport object then synchronized the hierarchy by sending synthetic `WM_LBUTTONDOWN` / `WM_LBUTTONUP` messages to the custom tree. That coupling could cause visible selection flicker and could not preserve individual object identity in the hierarchy.

This patch replaces that behavior with a Phase 14-specific hierarchy bridge rendered through the existing `EditorShellV3` Scene Hierarchy HWND. The visible rows are:

- `Scene (Runtime World)`;
- `Runtime Objects (4)` — expandable;
  - `Cube_A`;
  - `Cube_B`;
  - `Cube_C`;
  - `Ground_Plane`;
- `Main Camera`;
- `Environment (Procedural Sky)`.

The four object rows correspond directly to the four `SceneObjectHandle`s created by the Phase 14 validation scene. This is still pre-ECS and does not introduce Phase 15/16 entity/component authoring.

**Design choice (not directly from the book):** until the later editor-scene architecture exists, `EditorViewportController` supplies the Phase 14 hierarchy presentation for these known runtime validation objects while `EditorShellV3` continues to own the panel/chrome HWND.

### 3.6 Flicker-free selection synchronization

Synthetic tree clicks were removed.

Viewport -> hierarchy:

- picking sets `selectedIndex_` directly;
- the matching hierarchy child row is selected directly;
- `Runtime Objects` is expanded automatically when necessary;
- only the hierarchy HWND is invalidated for repaint.

Hierarchy -> viewport:

- clicking `Cube_A/B/C` or `Ground_Plane` maps directly to the corresponding validation-object index;
- clicking the `Runtime Objects` group selects the group but clears object selection;
- clicking the group's arrow expands/collapses the group without synthetic mouse-message recursion;
- camera/environment/root rows clear viewport object selection.

This preserves one explicit Phase 14 selection identity and removes the old feedback loop that caused hierarchy flickering.

### 3.7 Picking, selection visualization and gizmos

Picking remains the Luna Chapter 17 broad-phase foundation:

1. viewport pixel -> NDC;
2. NDC -> view-space ray using FOV/aspect;
3. view ray -> world-space ray using editor-camera orientation;
4. ray vs transformed AABB for selectable validation objects;
5. nearest positive hit wins.

The Win32 overlay continues to draw selected-object AABB feedback and transform gizmo handles so editor controls remain clearly visible. Move/Rotate/Scale update the selected validation object's runtime `World` transform.

No ECS, serialization, undo system, material/PBR system, lighting pipeline or PIE implementation is introduced by this patch.

## 4. Files changed by this patch

- `Engine/Render/DX12/MeshPass.h`
- `Engine/Render/DX12/MeshPass.cpp`
- `Data/Shaders/EditorGrid.hlsl` — new
- `Apps/NocturneEditor/EditorViewportController.h`
- `Apps/NocturneEditor/EditorViewportController.cpp`
- `Docs/Phase 14 — Implementation Report.md`

`EditorShellV3` remains the active editor shell and its Phase 13 panel/layout/chrome implementation is not replaced.

## 5. Verification checklist

### Verified from repository/source diff

- [x] Grid/world axes are removed from the layered Win32 overlay.
- [x] Grid/world axes are submitted as DX12 line geometry.
- [x] Grid PSO uses viewport DSV, depth testing and `LESS_EQUAL`.
- [x] Grid PSO does not write depth.
- [x] Grid is positioned slightly above the current validation ground to reduce z-fighting.
- [x] Selection bounds/gizmo remain editor overlay feedback rather than runtime-game UI.
- [x] Synthetic Scene Hierarchy mouse clicks were removed from selection synchronization.
- [x] Four real Phase 14 validation object handles have explicit hierarchy rows.
- [x] Viewport selection maps directly to the matching hierarchy object row.
- [x] Hierarchy object selection maps directly to the matching viewport/runtime object.
- [x] Runtime Objects remains an expandable grouping row.
- [x] `EditorShellV3` remains the active shell.
- [x] No Phase 15 ECS, Phase 16 general scene-authoring or Phase 17 serialization work was introduced.

### Requires local Windows validation

- [ ] `Debug x64` builds successfully under VS2026/v145.
- [ ] DX12 debug layer reports no errors at launch.
- [ ] Procedural sky remains visible.
- [ ] Grid is visible on/over the ground where not occluded.
- [ ] Cubes correctly occlude grid lines passing behind them.
- [ ] Grid no longer appears composited over cube faces.
- [ ] No obvious z-fighting occurs between grid and ground.
- [ ] Scene Hierarchy displays `Cube_A`, `Cube_B`, `Cube_C`, `Ground_Plane`, `Main Camera` and procedural environment.
- [ ] Expanding/collapsing `Runtime Objects` is stable.
- [ ] Selecting `Runtime Objects` no longer flickers.
- [ ] Clicking each hierarchy object selects the corresponding viewport object.
- [ ] Clicking each visible viewport object selects the matching hierarchy row.
- [ ] Empty-space viewport click clears object selection.
- [ ] Selection bounds and gizmos remain stable after hierarchy/viewport selection changes.
- [ ] Repeated resize/maximize/restore remains DX12-debug-layer clean.

The GitHub connector cannot execute the Windows/MSVC/DX12 binary, so local build/runtime checks remain intentionally open.

## 6. Common pitfalls / follow-up risks

- Do not move the world grid back to a composited overlay if it is expected to obey occlusion.
- Keep gizmo/selection UI visibility policy separate from world-space debug geometry depth policy.
- The explicit hierarchy names are Phase 14 validation metadata, not a general scene naming/entity system.
- `MeshPass` still renders one procedural validation geometry instanced N times; this patch is not a complete multi-mesh renderer.
- Do not rebuild the hierarchy through synthetic mouse messages; selection state must be synchronized directly.
- Do not introduce ECS/serialization solely to improve the Phase 14 hierarchy.

## 7. Next chat handoff

Bring:

1. fresh `Debug x64` build output after pulling this commit;
2. one screenshot of the viewport showing grid occlusion around the cubes;
3. one screenshot with `Runtime Objects` expanded and the individual objects visible;
4. confirmation whether selecting/collapsing the hierarchy still flickers;
5. any DX12 debug-layer error or selection mismatch.

Do not mark Phase 14 complete until the local verification checklist is closed.
