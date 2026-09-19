---
id: "noc.render"
doc_type: "system"
canonical: true
status: "implemented"
subsystem: "Rendering"
phase_introduced: 8
description: "Canonical renderer ownership, frame handoff, DirectX 12 backend and editor render-target integration contract."
source_files: ["Engine/Render/RenderSystem.h","Engine/Render/RenderSystem.cpp","Engine/Render/RenderQueue.h","Engine/Render/DX12/Dx12Renderer.h","Engine/Render/DX12/Dx12SwapChain.h"]
source_docs: ["Docs/nocturne_engine_architecture.md","Docs/Phase 8 — Rendering Bootstrap (DirectX 12).md","Docs/Phase 9 — Rendering Engine Foundation.md","Docs/Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md","Docs/Phase 14 — Completion Report.md"]
book_grounding: ["Jason Gregory — Game Engine Architecture (3rd ed.), §11.2 The Rendering Pipeline","Jason Gregory — Game Engine Architecture (3rd ed.), §11.2.1 Overview of the Rendering Pipeline"]
---

# Rendering

## Purpose

The Rendering layer owns GPU interaction and presentation. It consumes frame data produced by runtime/world systems and turns that data into rendered output.

Rendering does not own gameplay state.

## Runtime-to-render boundary

The current high-level seam is `RenderSystem`:

```text
Init(...)
AttachToWindow(...)
ResizeAttachedWindow(...)
SetResourceManager(...)
BeginFrame()
SetFrameRenderQueue(...)
EndFramePresent()
Shutdown()
```

`SetFrameRenderQueue()` explicitly takes a frame queue **without taking ownership**.

The authoritative world produces renderable frame data; the renderer consumes it.

```text
World / ECS
   |
   | BuildRenderQueue(frameArena, viewport)
   v
RenderQueue
   |
   v
RenderSystem
   |
   v
DX12 backend
   |
   v
swap chain / target
```

## Render extraction

`World::BuildRenderQueue()` is the runtime-side extraction boundary.

Phase 15 established that render extraction:

- reads ECS state;
- updates required derived transform/bounds state;
- writes renderer-facing POD frame data into the frame arena;
- does not expose mutable ECS component storage to the renderer;
- uses deterministic entity-index ordering.

This keeps simulation/authoring authority outside the renderer.

## DirectX 12 backend

Nocturne currently targets Direct3D 12 on Windows.

**Design choice (not directly from the book):** Direct3D 12 is the selected graphics API/backend for Nocturne. The books describe rendering architecture and graphics concepts but do not mandate this project-level API choice.

The DX12 layer owns concrete device, swap-chain, descriptor, pipeline-state and GPU-resource mechanics.

Higher layers should not need to contain gameplay decisions just because the backend is DirectX 12.

## Rendering pipeline grounding

Gregory describes the rendering pipeline as an ordered set of stages, from offline tools and asset conditioning through CPU application submission and GPU geometry/rasterization.

Nocturne's current boundary follows that separation:

- tools/import produce engine-ready content;
- Resources loads assets;
- World extracts potentially renderable instances;
- Render consumes the frame queue;
- DX12 performs GPU submission/presentation.

## Editor viewport

The editor attaches rendering to a dedicated child window through the same runtime/render attachment seam.

Phase 14 established resize-safe editor render-target handling, camera navigation, picking, selection, gizmos, depth-tested validation geometry, procedural grid/sky and debug selection.

The editor does not own a second rendering loop.

## Current debug channel

`Engine` exposes a deliberately small debug-selection channel that sends local bounds plus world transform to rendering without exposing editor/Win32 types inside the renderer.

**Design choice (not directly from the book):** this is development/editor scaffolding, not a general gameplay selection API.

## Invariants

- Renderer does not own authoritative world/gameplay state.
- Frame render data is extracted across an explicit boundary.
- `RenderSystem` does not take ownership of the submitted `RenderQueue`.
- GPU resource lifetime must respect in-flight usage.
- Resize operations update render targets through the renderer's explicit resize path.
- Editor rendering reuses runtime/render mechanisms.

## Non-responsibilities

Rendering does not own:

- entity/component authority;
- editor selection semantics;
- asset source authoring;
- gameplay logic;
- scene serialization.

## Book grounding

Primary grounding:

- Jason Gregory — *Game Engine Architecture, 3rd Edition*, §11.2, for rendering as a staged pipeline.
- Jason Gregory — *Game Engine Architecture, 3rd Edition*, §11.2.1, for the tools → asset-conditioning → application → GPU pipeline split.

Nocturne's DX12 backend, `RenderQueue` shape, frame-arena extraction and child-HWND editor integration are **Design choice (not directly from the book)**.

## Current implementation sources

- `Engine/Render/RenderSystem.h/.cpp`
- `Engine/Render/RenderQueue.h`
- `Engine/Render/DX12/`
- `Engine/Runtime/World.h/.cpp`
- `Apps/NocturneEditor/EditorViewportController.h/.cpp`

## Related documentation

- [Runtime](/docs/systems/runtime/)
- [Resources](/docs/systems/resources/)
- [World & ECS](/docs/systems/world-ecs/)
- [Editor](/docs/systems/editor/)
