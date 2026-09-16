# Phase 14 — Editor Rendering Viewport Handoff

> **Status:** READY TO START
>
> **Prerequisite:** Phase 13 is COMPLETE.
>
> **Starting branch:** create a Phase 14 branch from the completed `phase-13-editor-framework` baseline after pulling the latest documentation commit.
>
> **Roadmap scope:** viewport camera, gizmos, selection/picking and debug draw.

## 1. Phase name + objective

**Phase 14 — Editor Rendering Viewport**

Replace the Phase 13 central Viewport placeholder with a real DX12-backed editor viewport while preserving the editor/runtime architecture and the approved Phase 13 UI baseline.

The phase should finish with:

- a dedicated rendered viewport inside Nocturne Editor;
- an editor camera that can navigate the 3D scene;
- resize-correct viewport rendering;
- single-object viewport selection/picking;
- selection visualization;
- translate/rotate/scale gizmo foundation;
- editor debug draw for grid/axes/selection/gizmo primitives;
- hierarchy/viewport selection synchronization at the level supported by the current scene representation.

## 2. Read before implementation

At the start of the Phase 14 chat, study the full Phase 13 documentation and the current renderer/window/scene code before making architectural changes.

Required Phase 13 documents:

- `Docs/Phase 13 — Editor Framework Bootstrap.md`
- `Docs/Phase 13 — Completion Report.md`
- `Docs/Phase 13 — UI Fidelity Polish Pass.md`
- `Docs/Phase 13 — UI Fidelity Pass 2 Implementation.md`
- `Docs/Phase 13 — UI Fidelity Pass 3 Icons Toolbar Content Browser.md`
- `Docs/Phase 13 — UI Fidelity Pass 4 Tabler Icons.md`

Required code to inspect first:

- `Apps/NocturneEditor/main.cpp`
- `Apps/NocturneEditor/EditorShellV3.h/.cpp`
- `Engine/Platform/Win32/WinWindow.*`
- `Engine/Render/RenderSystem.*`
- `Engine/Render/DX12/Dx12Renderer.*`
- `Engine/Render/DX12/Dx12SwapChain.*`
- `Engine/Render/DX12/Dx12Device.*`
- `Engine/Render/DX12/Dx12FrameSync.*`
- current mesh/render-pass code;
- current `World` / scene representation from Phase 10;
- current input system from Phase 7.

**Important:** `main.cpp` uses `EditorShellV3`. Do not implement Phase 14 inside the historical `EditorShell` / `EditorControls` path.

## 3. Book grounding

### Jason Gregory — *Game Engine Architecture (3rd Edition)*

Phase 14 is directly aligned with the editor facilities discussed in Chapter 15.4:

- §15.4.1.2 — **Game World Visualization**: world editors provide 3D perspective and/or orthographic views and may integrate a rendering engine or communicate with the game engine for rendering.
- §15.4.1.3 — **Navigation**: 3D world editors require camera navigation/fly-through/orbit-style controls.
- §15.4.1.4 — **Selection**: editors require object selection, including ray-cast-style picking in a 3D view and synchronization with list/tree representations.
- §15.4.1.7 — **Object Placement and Alignment Aids**: position/orientation/scale are commonly manipulated through special handles in editor viewports.

### Frank D. Luna — *Introduction to 3D Game Programming with DirectX 12*

Relevant implementation grounding:

- Chapter 4 / Direct3D initialization — device, swap chain, back buffers, depth buffers, viewport/scissor and resize lifecycle.
- Chapter 17 — **Picking**: convert the clicked screen point to a view-space picking ray, transform the ray into object/local space, test bounding volumes first, then perform more precise intersections when required, choosing the nearest hit.

These references ground the rendering/picking mechanics.

The exact Nocturne editor-host integration, child HWND strategy, camera bindings, selection-state plumbing, debug-draw implementation and gizmo UX are **Design choice (not directly from the book)**.

## 4. Phase 13 constraints that are now hard requirements

### 4.1 Never attach DX12 to the top-level editor HWND

Phase 13 proved that attaching the swap chain to the top-level editor window paints over editor child controls.

Phase 14 must render to a **dedicated Viewport child surface** only.

### 4.2 Do not add a second application main loop

`noc::MainLoop` remains authoritative.

The editor viewport must participate in the existing engine/frame lifecycle rather than creating a separate `while` loop.

### 4.3 Preserve the Phase 13 visual baseline

Do not redesign:

- menu;
- toolbar;
- panel chrome;
- Content Browser;
- Console;
- status bar;
- Tabler icon system.

Only local Viewport-panel changes required to host rendering/input are Phase 14 scope.

### 4.4 Keep editor-only logic out of runtime UI

Nocturne Editor may call/runtime-drive renderer facilities, but editor panel code, Tabler icon code and gizmo interaction code must not become runtime-game UI dependencies.

## 5. Recommended Phase 14 architecture

### 5.1 Dedicated viewport child window

**Design choice (not directly from the book):** add a dedicated child HWND inside `EditorShellV3`'s Viewport body, for example:

```cpp
HWND viewportRenderHost_ = nullptr;
```

The child should occupy the renderable portion of the Viewport panel below/around the compact `Perspective / Lit / Show` controls.

The existing Phase 13 placeholder painting should be disabled/replaced only where the real render surface exists.

### 5.2 Renderer-side viewport target

Do not create a second engine.

Preferred direction to evaluate after inspecting the current DX12 renderer:

- keep one DX12 device/queue/fence infrastructure;
- add an editor viewport presentation/render-target concept that owns the resources associated with the child HWND;
- keep swap-chain/back-buffer/depth-buffer/RTV state localized to that presentation target;
- render the editor scene through existing render passes where possible.

If the current `Dx12SwapChain` is too tightly coupled to one top-level window, refactor that coupling rather than cloning the entire renderer.

**Design choice (not directly from the book):** the exact class name and ownership model must be decided after reading the current renderer. Do not pre-commit to a large multi-window rendering abstraction if a small clean viewport-target abstraction is sufficient.

## 6. Tight implementation scope

Phase 14 should implement only:

1. real rendering inside the central Viewport panel;
2. viewport resize lifecycle;
3. editor camera/navigation;
4. viewport input focus/capture rules;
5. single-selection picking;
6. selection visualization;
7. transform-gizmo foundation;
8. debug draw needed by the viewport/gizmos;
9. minimal synchronization between Viewport selection and Scene Hierarchy.

Do **not** implement:

- ECS architecture — Phase 15;
- full create/delete/component editing — Phase 16;
- scene serialization/save/load — Phase 17;
- physics/collision system — Phase 18;
- asset previewers — Phase 23;
- PIE — Phase 27.

## 7. Implementation steps

### Step 1 — establish the Phase 14 branch

After pulling the completed Phase 13 branch:

```powershell
git checkout phase-13-editor-framework
git pull --ff-only origin phase-13-editor-framework
git checkout -b phase-14-editor-rendering-viewport
```

**Design choice (not directly from the book):** this branch name matches the existing phase-based repository workflow.

### Step 2 — inspect renderer/window coupling before coding

Document:

- where the current swap chain receives its HWND;
- who owns resize;
- who owns RTV/DSV recreation;
- who owns viewport/scissor dimensions;
- whether the existing renderer assumes exactly one presentation target;
- where a second presentation target can integrate without duplicating device/queue/fence ownership.

Do not begin gizmos before this is clean.

### Step 3 — create the Viewport render host

Add the dedicated child HWND to `EditorShellV3`.

Requirements:

- child window is created/destroyed with the shell;
- layout resizes it with the Viewport body;
- zero/minimized dimensions are handled safely;
- Phase 13 toolbar/panel chrome remains above it;
- mouse focus/hover can be detected independently from other editor panels.

### Step 4 — render a first real frame

First rendering milestone:

- clear the Viewport child render target;
- present successfully;
- no rendering outside the Viewport panel;
- resizing recreates only the viewport-dependent resources required by the current renderer design;
- closing the editor releases viewport resources safely.

Do not add selection/gizmos until this milestone is stable.

### Step 5 — render the existing scene representation

Use the existing engine scene/world/render data path rather than building an editor-only duplicate scene.

The first goal is WYSIWYG use of runtime render data where the current architecture permits it.

### Step 6 — add an editor camera

Implement editor-only camera state with at minimum:

- perspective projection;
- correct aspect ratio from the render host client size;
- forward/back/strafe/up/down movement;
- mouse-look or equivalent fly navigation;
- predictable movement speed;
- focus rules so camera input is consumed only when the Viewport intends to own it.

Gregory §15.4.1.3 is the conceptual reference.

**Design choice (not directly from the book):** exact bindings, speeds and capture gestures are Nocturne Editor UX decisions.

### Step 7 — add picking ray generation

Follow Luna Chapter 17:

- convert mouse coordinates from viewport client space;
- account for the viewport width/height and projection matrix;
- create the ray in view space;
- transform the ray into the space used by intersection tests;
- normalize direction as required.

Keep this code separable from UI painting.

### Step 8 — implement single-object selection

Start with single selection only.

Recommended order:

1. broad/simple bounds test first;
2. nearest valid hit wins;
3. more precise mesh intersection only if supported cleanly by current CPU-side mesh data.

Luna explicitly motivates bounding-volume rejection before triangle-level work for picking.

Do not create an ECS solely to support selection.

### Step 9 — synchronize selection

Introduce a single editor selection state that can be driven by:

- Viewport click;
- Scene Hierarchy click.

Both views should reflect the same selected object where the current Phase 10 world representation permits stable object identity.

**Design choice (not directly from the book):** exact selection-service ownership will be decided from the current scene/object APIs. Avoid embedding separate independent selection variables in every panel.

### Step 10 — selection visualization

Render a clear but restrained selected-object visualization.

Examples that may be evaluated:

- bounds/wire box;
- tint/highlight pass;
- debug outline.

Do not turn this into the future full editor rendering framework unless needed.

### Step 11 — transform gizmo foundation

Gregory §15.4.1.7 grounds transform handles for position/orientation/scale.

Phase 14 should establish:

- Select / Move / Rotate / Scale toolbar mode connection;
- visible axis/handle primitives;
- hover/active axis state;
- mouse drag interaction foundation;
- transform updates only to the level safely supported by the existing world representation.

Because full editor scene authoring is Phase 16, Phase 14 must avoid inventing a second scene-editing/undo/serialization stack.

### Step 12 — debug draw

Add the minimum reusable editor/debug geometry needed for:

- grid;
- axes;
- bounds;
- picking visualization if useful;
- gizmos.

**Design choice (not directly from the book):** whether this becomes a renderer-level debug-draw service or a narrow editor render pass should be decided from the existing renderer architecture. Keep the first implementation small.

## 8. Suggested internal abstractions to evaluate

These names are suggestions, not mandates:

- `EditorViewport` — editor-side viewport state/input/camera/selection integration;
- `Dx12ViewportTarget` or `Dx12PresentationTarget` — child-HWND swap-chain/back-buffer ownership;
- `EditorCamera` — editor-only camera;
- `EditorSelection` — shared selected-object state;
- `DebugDraw` — line/primitive submission;
- `TransformGizmo` — editor-only gizmo interaction.

Every item above is **Design choice (not directly from the book)**. Only add an abstraction when current code proves it is needed.

## 9. Verification checklist

Phase 14 is not complete until:

- [ ] Debug x64 builds successfully.
- [ ] Editor starts with no top-level render overwrite.
- [ ] Real DX12 rendering is visible only inside the central Viewport panel.
- [ ] Viewport survives repeated resize/maximize/restore.
- [ ] Zero-size/minimized viewport paths do not crash or submit invalid resize work.
- [ ] Existing menu/toolbar/panels remain visually intact.
- [ ] Editor camera can navigate the scene.
- [ ] Camera input activates only under the intended Viewport focus/capture conditions.
- [ ] Projection/aspect ratio updates with viewport size.
- [ ] Clicking a visible selectable object produces the expected nearest selection.
- [ ] Empty-space click clears selection according to the chosen UX rule.
- [ ] Scene Hierarchy and Viewport reflect the same selected object.
- [ ] Selected object has visible viewport feedback.
- [ ] Select/Move/Rotate/Scale toolbar modes drive the gizmo mode.
- [ ] Gizmo axes/handles render correctly.
- [ ] Basic gizmo interaction works within the current scene representation.
- [ ] Debug grid/axes/bounds/gizmo rendering does not pollute runtime UI modules.
- [ ] Phase 13 Tabler/UI baseline remains intact.
- [ ] No Phase 15 ECS, Phase 16 full scene authoring, Phase 17 serialization or Phase 27 PIE implementation was pulled in.

## 10. Common pitfalls

- Rendering into the top-level editor HWND instead of a dedicated child target.
- Creating another DX12 device/engine/main loop when a presentation-target extension is sufficient.
- Rebuilding viewport resources every frame rather than only on size/target changes.
- Resizing swap-chain resources while GPU work still references them.
- Using full-window mouse coordinates instead of viewport-client coordinates for picking.
- Forgetting viewport aspect ratio when creating the picking ray.
- Doing triangle tests for every object before cheap bounds rejection.
- Letting selection state diverge between hierarchy and viewport.
- Implementing an ECS just to make gizmos possible.
- Adding serialization/undo/project persistence before their roadmap phases.
- Accidentally modifying the historical `EditorShell` instead of active `EditorShellV3`.
- Regressing Phase 13 visual spacing/icon sizes while integrating the child render host.

## 11. Phase 14 deliverables

Expected code/documentation by completion:

- dedicated editor Viewport child render host;
- renderer support for the editor presentation target;
- editor camera;
- picking/selection code;
- shared editor selection state;
- selected-object visualization;
- transform gizmo foundation;
- debug draw needed for editor viewport;
- Phase 14 documentation explaining ownership, frame lifecycle, resize, input, picking and gizmo decisions;
- updated architecture/roadmap status marking Phase 14 complete only after local Windows validation.

## 12. Next-chat handoff text

Start the Phase 14 chat with exactly this context:

> **Phase 13 is COMPLETE. Start Phase 14 — Editor Rendering Viewport. First study every existing Phase `.md` file, especially `Docs/Phase 13 — Completion Report.md` and `Docs/Phase 14 — Editor Rendering Viewport Handoff.md`. The active editor shell is `EditorShellV3`; do not use the historical shell. Preserve the Phase 13 visual baseline. First inspect the current DX12 renderer/swap-chain/window ownership, then design the smallest clean dedicated child-HWND viewport target. Phase 14 scope is real viewport rendering, editor camera/navigation, single-selection picking, transform gizmos and debug draw. Do not pull in Phase 15 ECS, Phase 16 full scene editing, Phase 17 serialization or Phase 27 PIE.**

Bring to the new chat:

- latest `phase-13-editor-framework` commit/branch state;
- the Phase 13 and Phase 14 docs above;
- any build output only if the new Phase 14 branch fails to build before changes.
