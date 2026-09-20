---
id: "noc.runtime"
doc_type: "system"
canonical: true
status: "implemented"
subsystem: "Runtime"
phase_introduced: 1
description: "Canonical runtime ownership, lifecycle, frame execution, configuration and subsystem orchestration contract."
source_files: ["Engine/Runtime/Engine.h","Engine/Runtime/Engine.cpp","Engine/Runtime/MainLoop.h","Engine/Runtime/MainLoop.cpp","Engine/Runtime/EngineConfig.h"]
source_docs: ["Docs/nocturne_engine_architecture.md","Docs/Phase 6 — Job System & Async Infrastructure.md","Docs/Production Engineering Standard.md"]
book_grounding: ["Jason Gregory — Game Engine Architecture (3rd ed.), §8.2 The Game Loop","Jason Gregory — Game Engine Architecture (3rd ed.), §8.6 Multiprocessor Game Loops"]
aliases: ["Runtime","Engine Runtime","Main Loop"]
deprecated_aliases: []
api_symbols: ["noc::Engine","noc::MainLoop","noc::World"]
---

# Runtime

## Purpose

The Runtime layer coordinates Nocturne Engine execution. It owns the engine-level lifecycle, the master frame loop, boot-time configuration consumption, subsystem ordering and the frame boundaries through which the other engine systems are serviced.

The runtime is an **orchestrator**, not a container for gameplay policy.

## Authoritative owner

`noc::Engine` owns the current engine services that participate in the runtime lifecycle:

- persistent allocator and frame `LinearArena`;
- `VirtualFileSystem`;
- `JobSystem`;
- `ResourceManager`;
- `AssetImportPipeline`;
- `InputSystem`;
- `RenderSystem`;
- `World`;
- engine configuration and render-target attachment state.

The application may configure the engine before initialization, but it does not replace the runtime's lifecycle ownership.

## Lifecycle

The public runtime seam includes:

```text
Engine::ConfigMutable()
Engine::Init()
Engine::Run()
Engine::TickOnce()
Engine::BeginFrame()
Engine::Tick()
Engine::EndFrame()
Engine::Shutdown()
```

The architectural contract is deterministic initialization and shutdown ordering.

Boot configuration is mutable before `Engine::Init()`. Once initialization begins, configuration is frozen.

**Design choice (not directly from the book):** Nocturne exposes an engine-owned `EngineConfig` mechanism while the application supplies project policy such as content roots and enabled behavior.

## Frame execution

Gregory describes the game loop as the master loop that periodically services engine subsystems, noting that different systems can require different update rates.

Nocturne follows that model:

```text
OS / window events
      |
      v
input snapshot
      |
      v
runtime/world servicing
      |
      v
render extraction
      |
      v
render stage / present
```

Rendering is a stage inside the runtime frame. It does not own a second master loop.

The runtime architecture allows systems to use different cadences where required. Nocturne does not introduce a separate monolithic "scheduler subsystem" merely to express multi-rate updates.

**Design choice (not directly from the book):** explicit runtime ordering remains preferred over adding generic scheduling complexity without a concrete requirement.

## Job system boundary

The `JobSystem` is owned by `Engine` and is available to systems that benefit from asynchronous or parallel work.

It does not own overall engine execution.

Gregory's multiprocessor game-loop discussion motivates decomposing frame work into task- or data-parallel jobs, but the runtime remains responsible for when those jobs are created, synchronized and consumed.

## Window and render attachment

The runtime owns the seam used to attach rendering to a native window:

```text
AttachWindow(...)
AttachRenderWindow(...)
ResizeRenderWindow(...)
CreateAndAttachMainWindow(...)
```

The editor reuses this seam for its dedicated child viewport. The runtime still owns frame execution.

## Invariants

- There is one authoritative engine lifecycle.
- The editor does not fork or replace the runtime loop.
- Configuration is not mutated after initialization begins.
- Subsystem lifetime follows explicit initialization/shutdown order.
- Frame-arena data does not outlive its frame ownership.
- Higher-level application policy is not embedded into lower-level engine mechanisms.

## Non-responsibilities

Runtime does not own:

- game-specific rules;
- editor presentation state;
- asset authoring UX;
- rendering implementation details;
- persistent scene serialization;
- physics, animation or audio policies that belong to their respective systems.

## Book grounding

Primary grounding:

- Jason Gregory — *Game Engine Architecture, 3rd Edition*, §8.2, for the game loop as the master subsystem-service loop.
- Jason Gregory — *Game Engine Architecture, 3rd Edition*, §8.6, for task/data decomposition and job-system use in multiprocessor frame execution.

Nocturne-specific ownership, configuration and API shapes are **Design choice (not directly from the book)**.

## Current implementation sources

- `Engine/Runtime/Engine.h/.cpp`
- `Engine/Runtime/MainLoop.h/.cpp`
- `Engine/Runtime/EngineConfig.h`
- `Engine/Core/Subsystems/SubsystemRegistry.*`
- `Engine/Core/Jobs/JobSystem.*`

## Related documentation

- [Architecture Overview](/docs/architecture/overview/)
- [Resources](/docs/systems/resources/)
- [Rendering](/docs/systems/rendering/)
- [World & ECS](/docs/systems/world-ecs/)
- [Editor](/docs/systems/editor/)
