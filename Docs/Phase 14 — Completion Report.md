# Phase 14 — Completion Report

> **Status:** ✅ COMPLETE
>
> **Branch:** `phase-14-editor-rendering-viewport`
>
> **Validated code head:** `6b8a3a45124e6c3a2b334952d7b3dd16e6a92813`
>
> **Phase 13 comparison baseline:** `fcec485e1345b878fd9cf69d9a705bfb9ce1e2ee`
>
> **Purpose:** canonical historical record of what Phase 14 implemented, what was fixed during runtime validation, what passed locally, and what remains deliberately deferred.

## 1. Executive summary

Phase 14 replaced the Phase 13 Viewport placeholder with a real DX12-backed editor viewport hosted by a dedicated child HWND inside `EditorShellV3`.

The finished phase includes:

- DX12 rendering isolated to the central Viewport;
- resize-safe back-buffer/depth-buffer handling;
- procedural 3D validation geometry;
- procedural sky;
- depth-tested grid/axes;
- editor fly camera;
- single-selection picking;
- hierarchy/viewport selection synchronization;
- depth-tested oriented selection visualization;
- working Move / Rotate / Scale gizmos;
- one-frame-cadence editor camera integration with the runtime `MainLoop`;
- resolution of the severe periodic stutter caused by repeated mesh-shader compilation;
- final Phase 13 UI regression/polish fixes;
- Nocturne Editor Windows application-icon integration.

The phase finished without pulling in Phase 15 ECS, later scene authoring/serialization, production lighting/PBR or PIE.

## 2. Architecture locked by Phase 14

The following rules are now proven and should be inherited by later editor phases:

- `EditorShellV3` is the active shell.
- The top-level editor HWND remains editor chrome only.
- DX12 presentation uses a dedicated Viewport child HWND.
- The editor does not create a second engine or main loop.
- `noc::MainLoop` remains authoritative.
- Editor-only camera/tool logic is driven through a narrow per-frame callback before `Engine::Tick()`.
- The runtime renderer does not depend on Win32 editor panel types.
- Selection debug rendering crosses the editor/runtime boundary through renderer-friendly bounds/world data, not Win32 UI objects.
- Grid and selection bounds that require scene occlusion are rendered through DX12, not painted as always-on-top GDI geometry.
- The Win32 layered overlay is restricted to transform gizmo handles.

The exact child-HWND and callback integration is **Design choice (not directly from the book)**.

## 3. Book grounding

Primary grounding:

- Jason Gregory, *Game Engine Architecture (3rd Edition)*, `15.4.1.2 — game-world visualization;
- Gregory `15.4.1.3 — navigation;
- Gregory `15.4.1.4 — selection;
- Gregory `15.4.1.7 — object placement/alignment aids and transform handles;
- Frank D. Luna, *Introduction to 3D Game Programming with DirectX 12*, Chapter 4 — swap-chain/depth/resize lifecycle;
- Luna Chapter 17 — picking ray, object/local-space tests and nearest hit.

Exact Nocturne editor composition, input bindings, procedural validation content, gizmo constants, menu proportions and icon resources are **Design choice (not directly from the book)**.

## 4. Runtime validation record

### Picking

- `Cube_A` — 10/10;
- `Cube_B` — 10/10;
- `Cube_C` — 10/10;
- `Ground_Plane` — 10/10;
- empty click clears — PASS;
- rotated-edge accuracy on `Cube_B` — PASS;
- rotated-edge accuracy on `Cube_C` — PASS;
- viewport → hierarchy synchronization — PASS;
- hierarchy → viewport synchronization — PASS;
- picking after camera movement — PASS;
- picking after transforms — PASS;
- nearest-hit final closure check — PASS.

### Camera/navigation

- RMB mouse-look — PASS;
- WASD — PASS;
- Q/E — PASS;
- Shift speed — PASS;
- mouse-wheel speed adjustment — PASS;
- RMB release stops capture — PASS;
- focus isolation — PASS;
- periodic stutter — RESOLVED.

### Gizmos

- Select hides gizmo — PASS;
- Move visible — PASS;
- Move X/Y/Z drag — PASS;
- Rotate rings visible — PASS;
- Rotate X/Y/Z drag — PASS;
- Scale visible — PASS;
- Scale X/Y/Z drag — PASS;
- hover-axis highlight — PASS;
- active-axis highlight — PASS;
- selection outline follows transform — PASS;
- gizmo remains usable after camera movement — PASS.

### Resize/render/UI closure

Reported PASS:

- repeated resize/maximize/restore;
- projection/aspect update;
- zero/minimized viewport safety;
- final `Ground_Plane` outline;
- DX12 debug-layer sanity check;
- Phase 13 UI regression check;
- final top-menu proportions after `8fdbee8`;
- application icon/resource integration.

## 5. Important defects found and resolved

### 5.1 Viewport mouse input reliability

The render host was made an explicit input surface using `SS_NOTIFY` and `HTCLIENT`.

### 5.2 Selection orientation

Selection visualization moved from world-AABB-style behavior to local bounds transformed by the exact selected-object world matrix.

### 5.3 Ground-plane outline inflation

Percentage-based expansion was replaced by a fixed world-space clearance of `0.006f` converted to local-axis units.

### 5.4 Camera update cadence

The editor timer-based camera update was removed. Camera input is now consumed once per engine frame through `MainLoop::FrameCallback`.

This architectural correction did **not** by itself resolve the periodic runtime hitch.

### 5.5 Severe periodic stutter

The mesh pass was recompiling `Basic.hlsl` every frame after PSO creation.

`cc6458f2ae216b065b8b1b14a11cdd8386ad4bc2` added the ready-state early return in `EnsureRootSigAndPso_()`.

After this commit the runtime stutter was reported resolved.

### 5.6 Transform gizmo completion

`e08dd05c594d34cc586453cfc1cf2c58ede8935c` finalized distinct Move/Rotate/Scale visuals, tool-specific hit tests, local-axis dragging and hover/active state.

### 5.7 Menu-bar proportions

`0651882f` initially enlarged both top menu and toolbar metrics.

`8fdbee86` corrected the final target by restoring toolbar metrics and enlarging only the menu row with a dedicated 14 px semibold font and larger item widths.

### 5.8 Nocturne application icon

`ad9e4038` added the resource compiler/Win32 icon path.

`6b8a3a45` finalized the icon assets and included the source `NocturneEngine-Logo.png`.

## 6. Branch audit — all commits after the Phase 13 comparison baseline

The GitHub comparison from `fcec485e1345b878fd9cf69d9a705bfb9ce1e2ee` to the validated Phase 14 head contains **41 commits, 0 behind**.

The first two commits are Phase 13 completion merges carried on the Phase 14 branch history:

1. `4139dce4` — `merge: complete Phase 13 editor framework`
2. `b294564a` — `merge: finalize Phase 13 documentation`

Phase 14 implementation/documentation commits:

3. `e7424532` — `phase14: add render-target resize entry point`
4. `4277137e` — `phase14: forward viewport resize through RenderSystem`
5. `1c70ca1b` — `phase14: expose DX12 viewport resize`
6. `e52da87d` — `phase14: add swap-chain resize support`
7. `b2abcd21` — `phase14: implement GPU-safe resizeable swap chain target`
8. `f3eeaf1e` — `phase14: make renderer resize-safe for editor child target`
9. `ce505944` — `phase14: track dedicated render target dimensions in Engine`
10. `fa0482f2` — `phase14: route rendering through dedicated target dimensions`
11. `b9e9ff6a` — `phase14: expose minimal EditorShellV3 viewport seam`
12. `b0a65707` — `phase14: add editor viewport controller interface`
13. `4be32b62` — `phase14: implement camera picking gizmos and debug overlay`
14. `ee1a5a21` — `phase14: wire dedicated viewport into editor startup`
15. `305f74d0` — `phase14: add viewport controller to editor project`
16. `ccafd615` — `docs: record Phase 14 viewport implementation and validation gates`
17. `c6b0ff81` — `Phase 14: declare Windows compatibility for layered child viewport overlay`
18. `ecc58601` — `Phase 14: embed editor compatibility manifest`
19. `ccf99e43` — `Phase 14: include depth format in PSO cache key`
20. `69b20a57` — `Phase 14: add viewport depth target to swap chain`
21. `433da73b` — `Phase 14: create and resize depth stencil target`
22. `c3b936ba` — `Phase 14: switch validation mesh path to procedural cube`
23. `f3d1029d` — `Phase 14: render depth-tested procedural cube instances`
24. `1284701a` — `Phase 14: track multi-object 3D validation scene selection`
25. `fda3d3c6` — `Phase 14: add real 3D validation scene and nearest-object picking`
26. `45e6063c` — `Phase 14: document depth-tested 3D viewport validation patch`
27. `efa98d9c` — `Phase 14: restore MeshPass PSO and root signature members`
28. `c4ae90ac` — `Phase 14: add procedural sky and lock viewport projection`
29. `fc182322` — `Phase 14: depth-test editor grid and expose scene hierarchy`
30. `832adce9` — `Phase 14: fix depth selection and viewport picking`
31. `0212f3a5` — `Phase 14: fix viewport hit-testing and oriented selection bounds`
32. `436bc5ec` — `Phase 14: use constant selection outline clearance`
33. `c1dcabc1` — `docs: update Phase 14 viewport validation status`
34. `b5f16917` — `docs: record Phase 14 picking and camera validation`
35. `cb4713ab` — `Phase 14: synchronize editor camera updates with frame loop`
36. `cc6458f2` — `Phase 14: avoid per-frame mesh shader compilation`
37. `e08dd05c` — `Phase 14: finalize transform gizmo interaction`
38. `0651882f` — `Phase 14: improve editor toolbar proportions`
39. `8fdbee86` — `Phase 14: enlarge editor menu bar`
40. `ad9e4038` — `Phase 14: add Nocturne Editor application icon`
41. `6b8a3a45` — `Phase 14: use full-quality Nocturne icon assets`

This audit is intentionally chronological so future work can distinguish intermediate fixes from the final accepted state.

## 7. Final implementation map

Primary Phase 14 files:

- `Apps/NocturneEditor/EditorViewportController.h/.cpp`
- `Apps/NocturneEditor/EditorShellV3.h/.cpp`
- `Apps/NocturneEditor/main.cpp`
- `Apps/NocturneEditor/NocturneEditor.manifest`
- `Apps/NocturneEditor/NocturneEditorResource.h`
- `Apps/NocturneEditor/Resources/NocturneEditor.ico`
- `Apps/NocturneEditor/Resources/NocturneEngine-Logo.png`
- `Data/Shaders/EditorSky.hlsl`
- `Data/Shaders/EditorGrid.hlsl`
- `Engine/Core/Math/MathTypes.h`
- `Engine/Render/RenderQueue.h`
- `Engine/Render/RenderSystem.h/.cpp`
- `Engine/Render/DX12/Dx12Renderer.h/.cpp`
- `Engine/Render/DX12/Dx12SwapChain.h/.cpp`
- `Engine/Render/DX12/Dx12PsoCache.h`
- `Engine/Render/DX12/MeshPass.h/.cpp`
- `Engine/Runtime/Engine.h/.cpp`
- `Engine/Runtime/MainLoop.h/.cpp`
- `Ide/VS2026/NocturneEditor/NocturneEditor.rc`
- `Ide/VS2026/NocturneEditor/NocturneEditor.vcxproj`

## 8. Deferred / next-phase boundary

Do not interpret Phase 14 validation scaffolding as authorization to pull later roadmap systems backward.

Still deferred:

- ECS — Phase 15;
- full scene authoring/create/delete/component editing — later phase;
- scene serialization — later phase;
- production undo/redo stack;
- physics — later phase;
- production lighting/shadows/PBR/materials;
- asset previewers;
- PIE — Phase 27.

The current validation cubes, ground and procedural sky may be replaced by later scene/rendering systems when their roadmap phases begin.

## 9. Known non-blocking renderer cleanup

`Dx12PsoKey` still uses shader-blob pointer values as part of the cache key. Phase 14 did not redesign that identity scheme.

The runtime blocker was resolved by preventing the already-ready mesh PSO path from recompiling the shaders each frame. A stable shader hash/ID cache design can be addressed later if the PSO cache is generalized.

This is **Design choice (not directly from the book)** and is not a Phase 14 completion blocker.

## 10. Next-chat handoff

For the next phase, bring:

- this completion report;
- `Docs/Phase 14 — Implementation Report.md`;
- the latest branch/merge state after Phase 14 is integrated according to the repository workflow.

The next phase must treat the dedicated child-HWND viewport, one-main-loop rule, validated camera/picking/selection/gizmo behavior, and Phase 13 editor shell as established baseline behavior.
