# Nocturne Engine — Unified Spec + Implementation Appendix

_Last generated: 2026-02-15_

This document consolidates the entire Markdown pack into a single coherent spec (**phases 1–10**), with **all implementation code moved into one up-to-date appendix**.

> **Historical/generated snapshot notice:** this file remains useful for the earlier consolidated material, but it is **not the authoritative roadmap or acceptance standard for Phase 15+**.
>
> For all future phases use:
>
> - `Docs/nocturne_engine_architecture.md` — canonical roadmap and phase boundaries;
> - `Docs/Production Engineering Standard.md` — mandatory production-grade completion standard from Phase 15 onward;
> - the current phase handoff / implementation / completion reports.
>
> Phase 15 is COMPLETE. Its final documentation pack is:
>
> - `Docs/Phase 15 — Entity Component System Architecture.md`
> - `Docs/Phase 15 — Implementation Report.md`
> - `Docs/Phase 15 — Test and CI Validation Report.md`
> - `Docs/Phase 15 — Completion Report.md`
> - `Docs/Phase 16 — Editor Scene Editing Handoff.md`
>
> Phase 16 planning / production-grade contract:
>
> - `Docs/Phase 16 — Runtime Reflection Architecture Contract.md`
> - `Docs/Phase 16 — Professional Grade Implementation Contract.md`
> - `Docs/Phase 16 — Editor Scene Editing Implementation Checklist.md`
>
> Any shorter future-phase descriptions embedded later in this generated snapshot are superseded by those documents.

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Engine Build Roadmap](#engine-build-roadmap)
- [Phase Specs](#phase-specs)
  - [Phase 1 — Core Systems (Final, Updated)](#phase-1-core-systems-final-updated)
  - [Phase 2 - Window & Main Loop](#phase-2---window-main-loop)
  - [Phase 3 — Resources & Virtual File System (Checklist)](#phase-3-resources-virtual-file-system-checklist)
  - [Phase 4 — Resource Manager](#phase-4-resource-manager)
  - [Phase 5 — Typed Resources & Loader Registry](#phase-5-typed-resources-loader-registry)
  - [Phase 6 — Job System & Async Infrastructure](#phase-6-job-system-async-infrastructure)
  - [Phase 7 — Input System (Raw Input + Snapshots + Action Mapping)](#phase-7-input-system-raw-input-snapshots-action-mapping)
  - [Phase 8 — Rendering Bootstrap (DirectX 12)](#phase-8-rendering-bootstrap-directx-12)
  - [Phase 9 — Rendering Engine Foundation](#phase-9-rendering-engine-foundation)
  - [Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources)](#phase-95-gpu-resource-foundation-dx12-memory-descriptors-asset-backed-gpu-resources)
  - [Phase 10 — Scene Representation](#phase-10-scene-representation)
- [Appendices](#appendices)
  - [Appendix A — Implementation Listings](#appendix-a--implementation-listings)

## Architecture Overview

> Target: Windows | Language: Modern C++ (C++20+)
>
> Genre Target: First-Person Survival Horror
>
> Primary Source of Truth: *Game Engine Architecture (3rd Edition)* — Jason Gregory

---

### 1. Architectural Philosophy
Nocturne Engine is a **layered, subsystem-oriented game engine**. Each subsystem has a clearly defined responsibility, explicit ownership rules, and a deterministic initialization and shutdown order.

Key principles:

- Clear separation between **engine code** and **game code**
- Explicit **platform abstraction**
- Engine **owns the main loop**
- Subsystems communicate through **well-defined interfaces**, not global state
- Gameplay is **data-driven where possible**, code-driven where necessary
- **Asynchronous, non-blocking systems by default** (I/O, resource loading)

---

### 2. High-Level Layering

> **Code moved to Appendix A:** see Listing 1 (from `nocturne_engine_architecture.md` — Nocturne Engine — Architecture Overview > 2. High-Level Layering).


---

### 3. Engine Layers
#### 3.1 Engine/Core
**Purpose:** Provide low-level foundational systems used by all other engine modules.

Responsibilities:

- Memory allocation (global allocator, arenas, pools)
- Math library (vectors, matrices, quaternions)
- Time system (high-resolution timing, frame delta)
- Logging and assertions
- Engine-wide type definitions and utilities

Rules:

- No dependency on platform-specific APIs
- No dependency on rendering, physics, resources, or gameplay

---

#### 3.2 Engine/Platform
**Purpose:** Isolate operating-system-specific functionality.

Responsibilities:

- Window creation and management (Win32)
- OS message pump
- High-resolution timers (QueryPerformanceCounter)
- File system primitives
- Raw input acquisition

Rules:

- Platform code is never included directly by gameplay
- Exposes abstract interfaces to higher layers

---

#### 3.3 Engine/Runtime
**Purpose:** Coordinate engine execution and subsystem lifetimes.

Responsibilities:

- Engine startup and shutdown sequencing
- Subsystem registration and dependency ordering
- Main game loop ownership
- Frame lifecycle management
- Multi-rate subsystem scheduling

Canonical loop structure (conceptual):


> **Code moved to Appendix A:** see Listing 2 (from `nocturne_engine_architecture.md` — Nocturne Engine — Architecture Overview > 3. Engine Layers > 3.3 Engine/Runtime).


##### Multi-Rate Update Model (Locked)
Nocturne does not introduce a single monolithic “scheduler subsystem.” Instead, once the Job System exists, individual engine systems declare their update cadence and are serviced accordingly within the engine frame lifecycle.


Examples:

- Input is sampled once per frame.
- Physics may run at a fixed timestep (potentially multiple steps per frame) with interpolation.
- Animation, AI, audio, and gameplay may run at their own cadences if needed.

Multi-rate simulation is an emergent property of system design coordinated by Engine/Runtime, not a standalone phase.

Gameplay and physics simulation use fixed timesteps where determinism is required; rendering interpolates between simulation states.

---

Notes:

- The **game loop is the master loop**.
- Subsystems may be serviced at different frequencies (e.g., physics > animation > AI).
- Rendering is a **stage within** the master loop, not a separate owner of execution.


---

#### 3.3.1 Engine Configuration Model (Boot-Time)
The engine exposes an **engine-owned configuration model** used to control
boot-time behavior.

Key rules:

- The engine defines its own configuration structures (e.g. `EngineConfig`)
- Configuration provides **mechanisms**, not policies
- The application (game/editor/host) may override configuration values
- Configuration is **consumed during engine initialization**

##### Configuration Lifecycle
- Engine configuration is **mutable only before `Engine::Init()`**
- Once initialization begins, configuration is considered **frozen**
- Modifying configuration after initialization is explicitly disallowed

Rationale:

Many engine subsystems (Virtual File System, memory, job system, rendering)
depend on configuration values during startup. Supporting mutation after
initialization would require teardown and reinitialization paths and is
out of scope for the core engine architecture.

This model ensures deterministic startup, clear ownership, and predictable
runtime behavior.


---

#### 3.4 Engine/Resources
**Purpose:** Unified resource access, loading, and lifetime management.

Responsibilities:

- Virtual File System (VFS)
  - Mount loose directories (development)
  - Mount packed archives (ZIP/composite files)
- Virtual path namespace shared across all resource types
- Resource identification via virtual paths (string + hash)
- Asynchronous resource loading from day one
- Resource lifetime tracking and single-instance guarantees
- Composite resource handling (e.g., models referencing meshes, textures, animations)
- Post-load initialization and tear-down hooks per resource type

Rules:

- Runtime supports **read-only** access to packed archives
- Archive creation and asset packaging are handled by **Tools**
- No direct dependency on rendering or gameplay logic

##### Content Root & File System Policy
The engine does **not** infer or discover where content lives on disk.

Rules:

- The application/build environment defines **physical content layout**
- Physical content roots are supplied to the engine via configuration
- The engine mounts exactly the paths it is given
- Paths may be absolute or relative to the process working directory
- Relative paths are resolved deterministically; no guessing is performed

The engine owns the **Virtual File System mechanism**, while the application
owns the **content layout policy**.

The runtime engine never:
- queries executable paths
- assumes content is located next to the executable
- embeds platform-specific directory rules

This separation ensures portability, testability, and predictable behavior
across development, tools, and shipping builds.

---

#### 3.5 Engine/Render
**Purpose:** All rendering and GPU interaction.

Responsibilities:

- Rendering API abstraction (Direct3D 12)
- GPU resource management
- Pipeline state management
- Frame graph / submission
- Consumption of immutable render data produced by gameplay

Rules:

- No gameplay logic
- Rendering is driven by data produced by gameplay systems

---

#### 3.6 Engine/Audio
**Purpose:** Audio playback and spatial sound simulation.

Responsibilities:

- Audio device management
- Sound effect playback
- Music playback and transitions
- 3D spatialization

---

#### 3.7 Engine/Physics
**Purpose:** Physical simulation and collision queries.

Responsibilities:

- Collision detection
- Rigid body simulation
- Character controller support
- Spatial queries (raycasts, sweeps)

Notes:

- May integrate third-party middleware
- Engine maintains abstraction boundary regardless of implementation

---

#### 3.8 Engine/Input
**Purpose:** Unified human interface device handling.

Responsibilities:

- Keyboard, mouse, controller input
- Per-frame input state buffering
- Action mapping (logical actions decoupled from physical devices)

Rules:

- Input is polled and buffered once per frame
- Gameplay consumes input data produced by this system

---

#### 3.8.5 Engine/Camera
**Purpose:** View representation and control.

Responsibilities:

- Camera transforms and projection parameters
- First-person, third-person, and cinematic camera models
- Camera controllers driven by gameplay or editor
- View data production for rendering

Rules:

- Cameras are data producers only
- Rendering consumes camera view data
- Gameplay may control cameras but does not render directly

---

#### 3.9 Engine/Gameplay
**Purpose:** Gameplay foundation layer shared by all games built on the engine.

**Responsibilities**:

- Game object model
- Component system
- World representation
- Messaging and events
- Gameplay-level systems
- Scripting integration (future)

**Notes**:

- Serialization is shared by both runtime and tools/editor workflows:
  - Runtime: save/load game state.
  - Tools/Editor: scenes, prefabs, component data, and authored configurations.
- The engine must not maintain separate “editor serializer” and “runtime serializer.”

**Rules**:

- Depends on engine systems
- Never depends on Game/* code

---

### 4. Game Layer
#### Game/HorrorGame
**Purpose:** Game-specific code and content.

Responsibilities:

- Player mechanics
- Enemy logic
- Horror-specific systems
- Game rules and progression

Rules:

- Can depend on Engine modules
- Must not modify Engine internals

---

### 5. Tools Layer
**Purpose:** Offline and development tools.

**Editor philosophy**:

- The editor shares the same engine modules as the game runtime (resources, rendering, physics, audio, gameplay foundation).
- Editor-only features are layered on top (panels, inspectors, gizmos, asset preview, PIE bridges).
- The editor does not become a second engine; it is a client of the engine plus authoring UI.

**Responsibilities**:

- Asset importers
- Resource linker / packager (build ZIP/composite archives)
- Build and cook pipeline
- Debug and profiling tools
- Editor (authoring tool built on the runtime engine)

Notes:

- Tools are responsible for **writing** packed archives
- Runtime engine is responsible only for **reading** them

#### Editor ↔ Runtime Boundary (Locked)
The editor is a tool built on top of the runtime engine. The runtime retains ownership of the main loop and system orchestration.

Rules:

- The editor may embed and drive an instance of the runtime engine.
- The editor must not replace or fork the engine loop; it can only provide hooks (e.g., pause/step, inspection, gizmos).
- Play-In-Editor runs the same runtime loop with editor-provided bridges, not a separate “editor loop.”

---

### 6. Dependency Rules (Non-Negotiable)
- Core → used by everything
- Platform → used by Runtime, Input, File I/O
- Runtime → orchestrates all systems
- Resources → used by Render, Audio, Physics, Gameplay
- Render / Physics / Audio → independent of gameplay
- Gameplay → depends on engine systems
- Game → depends on Gameplay + Engine

No upward dependencies are allowed.

---

#### 6.1 Policy vs Mechanism Ownership (Locked)
Architectural rule:

- The engine provides **mechanisms**
- The application provides **policies**

Examples:

- Engine provides: Virtual File System, resource loading, rendering APIs
- Application decides: where content lives, which archives are mounted,
  which systems are enabled

The engine must never embed application- or project-specific policy decisions.
All such decisions are expressed via configuration or higher-level systems.

This rule preserves correct dependency direction and prevents platform or
project assumptions from leaking into engine code.

---

### 7. Naming and Code Conventions
- Engine namespace: `noc::`
- Game namespace: `game::`
- No STL types in public engine headers
- Explicit ownership (no hidden globals)

---

### 8. What This Document Is
- A **living architectural contract**
- The reference used before adding any new system
- The baseline against which refactors are judged

---

### 9. Roadmap (Phase-Based)
#### Completed
- [x] **Phase 1 — Core Systems**
  (logging, asserts, memory, time, basic utilities)

- [x] **Phase 2 — Window & Main Loop Skeleton**
  (Win32 window, message pump, frame lifecycle)

- [x] **Phase 3 — Resources & Virtual File System**
  (virtual paths, mounts, loose + archive read)

- [x] **Phase 4 — Resource Manager Core**
  (resource identity, cache, async load thread, states)

- [x] **Phase 5 — Typed Resources & Loader Registry**
  (text/json/binary resources, decode/parse stage)

- [x] **Phase 6 — Job System & Async Infrastructure**
  (thread pool, work stealing/queues, futures)

- [x] **Phase 7 — Input System**
  (raw devices, action mapping, rebinding)

- [x] **Phase 8 — Rendering Bootstrap**
  (graphics API setup, device, swapchain, command submission, clear/present)

- [x] **Phase 9 — Rendering Engine Foundation**
  (SRP-based renderer, frame lifecycle,
  GPU resource foundation: default/upload heaps, descriptor heaps, root signature,
  PSO cache, asset-backed mesh upload, deferred GPU-safe destruction)

---

#### Full Roadmap
1. **Phase 1 — Core Systems**
   (logging, asserts, memory, time, basic utilities)

2. **Phase 2 — Window & Main Loop Skeleton**
   (Win32 window, message pump, frame lifecycle)

3. **Phase 3 — Resources & Virtual File System**
   (virtual paths, mounts, loose + archive read)

4. **Phase 4 — Resource Manager Core**
   (resource identity, cache, async load thread, states)

5. **Phase 5 — Typed Resources & Loader Registry**
   (text/json/binary resources, decode/parse stage)

6. **Phase 6 — Job System & Async Infrastructure**
   (thread pool, work stealing/queues, futures)

7. **Phase 7 — Input System**
   (raw devices, action mapping, rebinding)

8. **Phase 8 — Rendering Bootstrap**
   (graphics API setup, device, swapchain, command submission, clear/present)

9. **Phase 9 — Rendering Engine Foundation**
   (SRP-based renderer, frame lifecycle,
   GPU resource foundation: default/upload heaps, descriptor heaps, root signature,
   PSO cache, asset-backed mesh upload, deferred GPU-safe destruction)

10. **Phase 10 — Scene Representation**
    (world, transforms, spatial hierarchy, visibility basics)

11. **Phase 11 — Asset Import Pipeline**
    (importers, intermediate formats, metadata, dependency graph)

12. **Phase 12 — Cooker & Packager Tools**
    (cook step, deterministic outputs, archive build, versioning)

13. **Phase 13 — Editor Framework Bootstrap**
    (desktop app shell, docking UI, project system, content browser)

14. **Phase 14 — Editor Rendering Viewport**
    (viewport camera, gizmos, selection, debug draw)

15. **Phase 15 — Entity / Component System**
    (ECS or component model, serialization-ready data layout)

16. **Phase 16 — Editor Scene Editing**
    (create/delete entities, component inspectors, prefab prototype)

17. **Phase 17 — Serialization & Save / Load**
    (scene files, prefabs, savegame, version tolerance)

18. **Phase 18 — Physics & Collision**
    (broadphase, narrowphase, rigid bodies, queries, character controller)

19. **Phase 19 — Animation System**
    (skeletons, clips, blend trees / state machines, retargeting baseline)

20. **Phase 20 — Audio System**
    (device, voices, mixing, 3D spatialization, streaming audio)

21. **Phase 21 — Lighting & Post-Processing**
    (deferred/forward+ choice, shadows, tone mapping, fog, bloom)

22. **Phase 22 — Materials & PBR Workflow**
    (material parameter system, texture sets, instancing)

23. **Phase 23 — Editor Asset Previewers**
    (mesh/animation/texture/audio preview panes, reimport hooks)

24. **Phase 24 — Scripting & Gameplay Runtime Layer**
    (bindings, events, triggers, gameplay framework)

25. **Phase 25 — AI & Navigation**
    (navmesh build, pathfinding, perception, BT/FSM framework)

26. **Phase 26 — Gameplay Systems for Horror**
    (interaction, inventory, doors/locks, stamina, sanity/fear hooks)

27. **Phase 27 — Editor Play-In-Editor (PIE)**
    (PIE launch, hot-reload of scripts/data, runtime ↔ editor bridge)

28. **Phase 28 — Debug & Profiling Tooling**
    (in-engine profiler, GPU timings, capture tools, debug overlays)

29. **Phase 29 — Build & Deployment Pipeline**
    (configurations, packaging, crash reporting hooks, installer)

30. **Phase 30 — Optimization & Content Validation**
    (asset validation rules, LODs, streaming budgets, performance gates)

31. **Phase 31 — Shipping Polish**
    (QA tools, regression tests, deterministic cooks, final editor UX passes)


---

This document remains the top-level reference for all future work.



## Engine Build Roadmap

| Phase | Title | Source file |
|---:|---|---|
| 1 | Phase 1 — Core Systems (Final, Updated) | `phase_1_core_systems.md` |
| 2 | Phase 2 - Window & Main Loop | `Phase 2 - Window & Main Loop.md` |
| 3 | Phase 3 — Resources & Virtual File System (Checklist) | `Phase 3 — Resources & Virtual File System.md` |
| 4 | Phase 4 — Resource Manager | `Phase 4 — Resource Manager.md` |
| 5 | Phase 5 — Typed Resources & Loader Registry | `Phase 5 — Typed Resources & Loader Registry.md` |
| 6 | Phase 6 — Job System & Async Infrastructure | `Phase 6 — Job System & Async Infrastructure.md` |
| 7 | Phase 7 — Input System (Raw Input + Snapshots + Action Mapping) | `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` |
| 8 | Phase 8 — Rendering Bootstrap (DirectX 12) | `Phase 8 — Rendering Bootstrap (DirectX 12).md` |
| 9 | Phase 9 — Rendering Engine Foundation | `Phase 9 — Rendering Engine Foundation.md` |
| 9.5 | Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources) | `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` |
| 10 | Phase 10 — Scene Representation | `Phase 10 — Scene Representation.md` |

## Phase Specs

## Phase 1 — Core Systems (Final, Updated)

> **Status:** COMPLETE ✅\
> **Verified:** Logging, Assert, Clock/Time, Memory, Subsystem lifecycle with dependency ordering

This document is the **authoritative implementation reference** for Phase 1 of Nocturne Engine. It reflects the **actual final code**, including fixes and adjustments discovered during implementation.

---

### 1. Phase Objective
Establish a **rock-solid engine core** that provides:

- Deterministic startup and shutdown
- Visibility (logging + asserts)
- Stable time measurement
- Controlled memory allocation
- Explicit subsystem dependency ordering

No rendering, windows, or gameplay systems exist yet.

---

### 2. Final Folder Structure (After Phase 1)

> **Code moved to Appendix A:** see Listing 3 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 2. Final Folder Structure (After Phase 1)).


---

### 3. Subsystem Model (Final)
#### 3.1 SubsystemDesc

> **Code moved to Appendix A:** see Listing 4 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 3. Subsystem Model (Final) > 3.1 SubsystemDesc).


**Why:**

- No virtual dispatch
- No global initialization order reliance
- Context pointer allows safe access to Engine internals without globals

---

### 4. SubsystemRegistry (Final Behavior)
#### Responsibilities
- Detect duplicate subsystem names
- Resolve dependencies via **topological sort**
- Detect:
  - missing dependencies
  - dependency cycles
- Enforce deterministic startup/shutdown order

#### Startup Rules
- All dependencies start **before** dependents
- On startup failure, already-started systems are shut down in reverse order

#### Shutdown Rules
- Reverse of startup order

#### Algorithm
- DFS-based topological sort
- States: `Unvisited`, `Visiting`, `Visited`
- Cycles detected when visiting a `Visiting` node

> **Design choice (not directly from the book):** DFS topo-sort implementation. Requirement for ordered startup/shutdown is book-grounded.

---

### 5. Logging System
#### Files
- `Core/Log.h`
- `Core/Log.cpp`

#### Key Types

> **Code moved to Appendix A:** see Listing 5 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 5. Logging System > Key Types).


#### Usage

> **Code moved to Appendix A:** see Listing 6 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 5. Logging System > Usage).


#### Behavior
- Enabled in Debug / Dev
- Compiled out in Ship
- Output to:
  - Visual Studio Output window (`OutputDebugStringA`)
  - stdout

---

### 6. Assert System
#### Files
- `Core/Assert.h`
- `Core/Assert.cpp`

#### Macros
- `NOC_ASSERT(expr)`
- `NOC_ASSERT_MSG(expr, msg)`
- `NOC_VERIFY(expr)` (evaluates in all builds)

#### Behavior
- Debug/Dev: logs + breaks into debugger
- Ship: compiled out (except VERIFY evaluation)

---

### 7. Clock / Time System
#### Files
- `Core/Clock.h`
- `Core/Clock.cpp`
- `Platform/Win32/WinPlatform.h / .cpp`

#### Why renamed
Originally named `Time.h`, but this **collided with CRT \*\*\*\*****\<time.h>**, breaking `<ctime>`. Renamed to `Clock` to permanently avoid standard header collisions.

#### Key Classes

> **Code moved to Appendix A:** see Listing 7 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 7. Clock / Time System > Key Classes).


#### Features
- Uses `QueryPerformanceCounter`
- dt clamping to suppress breakpoint spikes
- `SecondsSinceStart()` for profiling/debug

---

### 8. Memory System
#### Components
##### IAllocator
Abstract allocation interface.

##### MallocAllocator
- Wraps `_aligned_malloc` / `_aligned_free`
- Baseline allocator

##### DebugAlloc
- Wraps another allocator
- Tracks:
  - total allocated bytes
  - outstanding bytes
  - allocation count
- Debug/Dev only

##### LinearArena
- Bump allocator
- Used for **per-frame transient allocations**
- Reset once per frame

#### Engine Integration
- Engine owns allocator instances
- Frame arena allocated once at startup
- Freed at shutdown
- Outstanding memory verified to be 0

---

### 9. Engine Class (Runtime)
#### Responsibilities
- Register subsystems
- Own allocator + frame arena
- Drive startup / tick / shutdown

#### Public Accessors

> **Code moved to Appendix A:** see Listing 8 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 9. Engine Class (Runtime) > Public Accessors).


#### Internal Lifecycle

> **Code moved to Appendix A:** see Listing 9 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 9. Engine Class (Runtime) > Internal Lifecycle).


Subsystems call these via context pointer.

---

### 10. Verified Startup Order (Final)
#### Startup
1. Log
2. Time
3. Memory
4. Assert

#### Shutdown
1. Assert
2. Memory
3. Time
4. Log

---

### 11. Negative Test Results (Verified)
#### Missing Dependency

> **Code moved to Appendix A:** see Listing 10 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 11. Negative Test Results (Verified) > Missing Dependency).


#### Cycle Detection

> **Code moved to Appendix A:** see Listing 11 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 11. Negative Test Results (Verified) > Cycle Detection).


---

### 12. Phase 1 Completion Criteria
All criteria met:

- Deterministic lifecycle
- Memory correctness
- Time stability
- Explicit dependency management

Phase 1 is **DONE**.

---

### 13. Next Phase Handoff
**Phase 2 Objective:**

> Create a real OS window, message pump, and continuous game loop.

Phase 2 will implement:

- Win32 window creation
- Message processing
- Real-time main loop stages
- Frame pacing (sleep vs spin)

---

### 14. Implementations
> This section contains the **full, final implementations** of every file produced in Phase 1.\
> Paths are shown exactly as they exist in the repository.\
> These implementations are **canonical** and must be kept in sync with engine code.

---

#### `Engine/Core/BuildConfig.h`

> **Code moved to Appendix A:** see Listing 12 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/BuildConfig.h`).


---

#### `Engine/Core/Assert.h`

> **Code moved to Appendix A:** see Listing 13 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Assert.h`).


---

#### `Engine/Core/Assert.cpp`

> **Code moved to Appendix A:** see Listing 14 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Assert.cpp`).


---

#### `Engine/Core/Log.h`

> **Code moved to Appendix A:** see Listing 15 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Log.h`).


---

#### `Engine/Core/Log.cpp`

> **Code moved to Appendix A:** see Listing 16 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Log.cpp`).


---

#### `Engine/Core/Clock.h`

> **Code moved to Appendix A:** see Listing 17 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Clock.h`).


---

#### `Engine/Core/Clock.cpp`

> **Code moved to Appendix A:** see Listing 18 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Clock.cpp`).


---

#### `Apps/NocturneHost/main.cpp`

> **Code moved to Appendix A:** see Listing 19 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Apps/NocturneHost/main.cpp`).


---

#### `Engine/Platform/Win32/WinPlatform.h`

> **Code moved to Appendix A:** see Listing 20 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Platform/Win32/WinPlatform.h`).


---

#### `Engine/Platform/Win32/WinPlatform.cpp`

> **Code moved to Appendix A:** see Listing 21 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Platform/Win32/WinPlatform.cpp`).


---

#### `Engine/Core/Memory/Allocator.h`

> **Code moved to Appendix A:** see Listing 22 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Memory/Allocator.h`).


---

#### `Engine/Core/Memory/Allocator.cpp`

> **Code moved to Appendix A:** see Listing 23 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Memory/Allocator.cpp`).


---

#### `Engine/Core/Memory/LinearArena.h`

> **Code moved to Appendix A:** see Listing 24 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Memory/LinearArena.h`).


---

#### `Engine/Core/Memory/LinearArena.cpp`

> **Code moved to Appendix A:** see Listing 25 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Memory/LinearArena.cpp`).


---

#### `Engine/Core/Memory/DebugAlloc.h`

> **Code moved to Appendix A:** see Listing 26 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Memory/DebugAlloc.h`).


---

#### `Engine/Core/Memory/DebugAlloc.cpp`

> **Code moved to Appendix A:** see Listing 27 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Memory/DebugAlloc.cpp`).


---

#### `Engine/Core/Subsystems/Subsystem.h`

> **Code moved to Appendix A:** see Listing 28 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Subsystems/Subsystem.h`).


---

#### `Engine/Core/Subsystems/SubsystemRegistry.h`

> **Code moved to Appendix A:** see Listing 29 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Subsystems/SubsystemRegistry.h`).


---

#### `Engine/Core/Subsystems/SubsystemRegistry.cpp`

> **Code moved to Appendix A:** see Listing 30 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Subsystems/SubsystemRegistry.cpp`).


---

#### `Engine/Runtime/Engine.h`

> **Code moved to Appendix A:** see Listing 31 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Runtime/Engine.h`).


---

#### `Engine/Runtime/Engine.cpp`

> **Code moved to Appendix A:** see Listing 32 (from `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Runtime/Engine.cpp`).


---

**End of Phase 1**


## Phase 2 - Window & Main Loop


Transform the engine from a **single-tick test harness** into a **real-time application** by adding:

* A native **Win32 window**
* A non-blocking **OS message pump**
* A continuous **engine-owned main loop**
* Deterministic **frame lifecycle stages**

At the end of Phase 2, Nocturne Engine runs continuously until the user closes the window.

No rendering, input mapping, or gameplay logic exists yet.

---

### 2. Book-Grounded Scope (Why This Phase Exists)

Phase 2 is derived primarily from:

* **Chapter 6 — Engine Support Systems**
  Subsystem ownership, startup/shutdown ordering, and engine-controlled execution 
* **Chapter 8.5 — Measuring and Dealing with Time**
  Frame-to-frame timing, real-time loop considerations, and dt stability 
* **Chapter 3 — The Game Loop**
  Why the engine—not the OS—must own the loop structure and frame boundaries 

Everything else is explicitly labeled **Design choice (not directly from the book)**.

---

### 3. Phase 2 Responsibilities (Hard Rules)

Phase 2 introduces **exactly three new responsibilities**:

1. **Window ownership** (platform-specific)
2. **Message pumping** (non-blocking)
3. **Continuous main loop** (engine-owned)

#### Explicit Non-Goals

* ❌ No rendering API
* ❌ No input abstraction layer
* ❌ No ECS, scene, or gameplay code
* ❌ No editor or tools

---

### 4. Updated Folder Structure (After Phase 2)


> **Code moved to Appendix A:** see Listing 33 (from `Phase 2 - Window & Main Loop.md` — 4. Updated Folder Structure (After Phase 2)).


---

### 5. New Subsystems Introduced

#### 5.1 Window Subsystem

**Location:** `Engine/Platform/Win32/WinWindow.*`

**Responsibility:**

* Register Win32 window class
* Create and destroy a native window
* Own the `HWND`
* Forward OS close events to the engine

**Rules:**

* No rendering knowledge
* No message pumping logic
* No global HWND access

> **Book grounding:** Platform abstraction and OS isolation are mandatory engine support systems. 

---

#### 5.2 Main Loop (Runtime-Owned)

**Location:** `Engine/Runtime/MainLoop.*`

**Responsibility:**

* Own the real-time loop
* Define per-frame execution stages
* Integrate Phase 1 TimeSystem

Canonical structure:


> **Code moved to Appendix A:** see Listing 34 (from `Phase 2 - Window & Main Loop.md` — 5. New Subsystems Introduced > 5.2 Main Loop (Runtime-Owned)).


> **Book grounding:** The engine must own the loop to guarantee determinism and subsystem ordering. 

---

### 6. Engine Lifecycle (Updated)

#### Startup Order (Phase 2)

1. Core subsystems (Phase 1)
2. Window subsystem
3. Main loop initialization

#### Shutdown Order

1. Main loop stops
2. Window destroyed
3. Core subsystems shut down (reverse order)

This preserves **Phase 1 guarantees**: no global static teardown hazards.

---

### 7. Message Pump Design

#### Non-Blocking Rule (Mandatory)

* Use `PeekMessage`, **never** `GetMessage`
* Engine loop must continue even with no OS messages


> **Code moved to Appendix A:** see Listing 35 (from `Phase 2 - Window & Main Loop.md` — 7. Message Pump Design > Non-Blocking Rule (Mandatory)).


> **Book grounding:** Blocking the engine loop breaks real-time simulation assumptions. 

---

### 8. Frame Timing Rules

* `TimeSystem::BeginFrame()` called **once per loop**
* `TimeSystem::EndFrame()` called **once per loop**
* dt clamping behavior from Phase 1 remains unchanged
* No fixed timestep yet (added later)

This ensures **stable dt** before rendering or simulation exist.

---

### 9. Relationships & Dependency Graph (Phase 2)


> **Code moved to Appendix A:** see Listing 36 (from `Phase 2 - Window & Main Loop.md` — 9. Relationships & Dependency Graph (Phase 2)).


Rules:

* Platform code never depends on Runtime
* Runtime owns execution
* Core remains dependency-free

---

### 10. Verification Checklist (Phase 2 Is Done When…)

* [ ] Window appears and remains responsive
* [ ] Engine runs continuously until window close
* [ ] Closing the window exits cleanly
* [ ] dt is stable and logged every frame
* [ ] No blocking calls stall the loop
* [ ] All subsystems shut down deterministically

---

### 11. Common Pitfalls (Phase 2)

* Using `GetMessage` instead of `PeekMessage` (hard stall) 
* Letting Win32 own the main loop (inverted control)
* Tying dt to OS message frequency
* Performing rendering or input mapping too early

---

### 12. Phase 2 Completion Criteria

Phase 2 is complete when:

* The engine owns a real-time loop
* The OS window lifecycle is fully controlled
* Phase 1 systems remain unchanged and stable

No graphics. No gameplay. No shortcuts.

---

### 13. Next Phase Handoff

When Phase 2 is complete, say:

> “Phase 2 is complete. The window runs and the loop is stable. Start Phase 3: Rendering bootstrap.”

Phase 3 will introduce:

* Graphics API selection (DX12)
* Swap chain
* First triangle

---

### 14. Implementations

Below are **all files implemented/modified in Phase 2**, each with:

* **`path/to/file`**
* **full implementation**

---

#### `Apps/NocturneHost/AppConfig.h`


> **Code moved to Appendix A:** see Listing 37 (from `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Apps/NocturneHost/AppConfig.h`).


---

#### `Apps/NocturneHost/main.cpp`


> **Code moved to Appendix A:** see Listing 38 (from `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Apps/NocturneHost/main.cpp`).


---

#### `Engine/Platform/Win32/WinWindow.h`


> **Code moved to Appendix A:** see Listing 39 (from `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Engine/Platform/Win32/WinWindow.h`).


---

#### `Engine/Platform/Win32/WinWindow.cpp`


> **Code moved to Appendix A:** see Listing 40 (from `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Engine/Platform/Win32/WinWindow.cpp`).


---

#### `Engine/Runtime/MainLoop.h`


> **Code moved to Appendix A:** see Listing 41 (from `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Engine/Runtime/MainLoop.h`).


---

#### `Engine/Runtime/MainLoop.cpp`


> **Code moved to Appendix A:** see Listing 42 (from `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Engine/Runtime/MainLoop.cpp`).


---

#### `Engine/Runtime/Engine.h`
> **Code moved to Appendix A:** see Listing 43 (from `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Engine/Runtime/Engine.h`).


---

#### `Engine/Runtime/Engine.cpp`

> This file **includes** the Phase 1 subsystem registrations unchanged, then adds the Phase 2 window subsystem and `Run()`.


> **Code moved to Appendix A:** see Listing 44 (from `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Engine/Runtime/Engine.cpp`).


---

**End of Phase 2**

---

### What's Next?

Phase 3 will implement:

- Virtual File System
- Loose directory mounts
- ZIP archive mounts (read-only)
- Virtual path namespace

## Phase 3 — Resources & Virtual File System (Checklist)


#### ✅ Completed

- [x] **VPath normalization core is working**
  - Virtual paths like `hello.txt` resolve consistently through VFS.

- [x] **VirtualFileSystem mounts loose directories**
  - `MountLooseDirectory(...)` succeeds and logs mount info.

- [x] **Deterministic content root via engine-owned config**
  - Engine has default `EngineConfig` with `contentRoot = "Data"`.
  - Application can override via **pre-init setters** (e.g. `engine.SetContentRoot(...)`).

- [x] **Config lifecycle rule enforced**
  - Config is **mutable only before** `Engine::Init()`.
  - Attempts to modify after init **fail and log**.

- [x] **No platform path discovery in Runtime**
  - Content root selection is solved via config/policy, not Win32 queries.

- [x] **Loose file read end-to-end validated**
  - Host-side smoke test confirmed: mount → open → read → close works.

- [x] **Archive mount indexing validated**
  - ZIP central directory parsed.
  - Entry count logged during mount.

- [x] **Stored-entry archive reads verified**
  - ZIP method **0 (stored)** entries can be opened and read successfully.

- [x] **Unsupported archive compression handled cleanly**
  - ZIP entries using unsupported compression methods fail open with a clear error.

- [x] **VFS convenience APIs implemented**
  - `VirtualFileSystem::ReadAllBytes(...)`
  - `VirtualFileSystem::ReadAllText(...)` (debug-only helper)

- [x] **Smoke tests moved out of `Engine::Init()`**
  - All file I/O validation now lives in host-side test code.

- [x] **Mount priority behavior implemented, tested, and locked**
  - Policy: **later mounts override earlier mounts**
  - Verified with override test (`DataOverrides` beats `Data`).


---

### 1. Phase Objective

Transform Nocturne Engine from a code-only runtime into a **data-driven engine** by adding:

- A **Virtual File System (VFS)** owned by the engine
- A unified **virtual path namespace**
- Support for **multiple mount points**
- Read-only access to:
  - Loose directories (development)
  - Archive files (packed data)

At the end of Phase 3:

- All file access goes through the VFS
- No engine system calls OS file APIs directly
- Resource systems can be built without knowing *where data lives*

---

### 2. Book-Grounded Scope (Why This Phase Exists)

Phase 3 is grounded primarily in:

- **Chapter 7 — Engine Support Systems**  
  File systems as foundational, platform-isolated engine services
- **Chapter 8 — Resources and the File System**  
  Virtual paths, mount-based resolution, and resource indirection

Jason Gregory explicitly stresses that **resource identity must be decoupled from physical storage**, enabling packaging, streaming, and platform portability.

Everything not explicitly mandated is marked **Design choice (not directly from the book)**.

---

### 3. Phase 3 Responsibilities (Hard Rules)

Phase 3 introduces **exactly four responsibilities**:

1. **Virtual path abstraction**
2. **Mount-based resolution**
3. **Platform-isolated file I/O**
4. **Unified read-only file access API**

#### Explicit Non-Goals

- ❌ No resource manager
- ❌ No async I/O
- ❌ No asset parsing (textures, meshes, audio)
- ❌ No hot reload or file watching
- ❌ No compression strategy tuning
- ❌ No editor or tooling

Those come later.

---

### 4. Updated Folder Structure (After Phase 3)


> **Code moved to Appendix A:** see Listing 45 (from `Phase 3 — Resources & Virtual File System.md` — 4. Updated Folder Structure (After Phase 3)).


---

### 5. Virtual Paths

All file access uses **virtual paths**, never OS paths.

Examples:


> **Code moved to Appendix A:** see Listing 46 (from `Phase 3 — Resources & Virtual File System.md` — 5. Virtual Paths).


Rules:

- Forward slashes `/` only
- Case-sensitive (engine rule)
- No drive letters
- No absolute paths
- No `..` traversal

> **Book grounding:** Virtual paths enable resource packaging, redirection, and portability.

---

### 6. Mount System Design

#### Mount Concept

A **mount** maps a virtual root (`/`) to a physical data source.

Example mounts (actual Phase 3 behavior):

| Mount Order | Virtual Root | Physical Source |
|------------|--------------|-----------------|
| 0 | `/` | Packed archive (`.zip`) |
| 1 | `/` | Loose content directory |
| 2 | `/` | Loose override directory |

#### Resolution Rules (LOCKED)

1. All mounts share the same virtual root namespace.
2. Mounts are searched **in reverse order of insertion**.
3. **Later mounts override earlier mounts**.
4. The first successful open wins.

This enables:
- Packed data as a baseline
- Loose content during development
- Explicit override directories for rapid iteration

Mount order enforced by the engine:


> **Code moved to Appendix A:** see Listing 47 (from `Phase 3 — Resources & Virtual File System.md` — 6. Mount System Design > Resolution Rules (LOCKED)).


---

### 7. File System Backends

Phase 3 introduces **file system backends**, not resources.

#### 7.1 Loose Directory Mount

- Reads directly from the OS filesystem
- Development-friendly
- Read-only from engine perspective

#### 7.2 Archive Mount

- Read-only
- Central directory lookup
- No runtime modification

> **Design choice:** ZIP-like archive format for simplicity and tooling support.

---

### 8. Platform Isolation (Mandatory)

All OS file access is isolated to:


> **Code moved to Appendix A:** see Listing 48 (from `Phase 3 — Resources & Virtual File System.md` — 8. Platform Isolation (Mandatory)).


Responsibilities:

- Open file
- Read bytes
- Seek
- Query file size
- Close file

No other engine layer may include `<Windows.h>` for file I/O.

---

### 9. VFS Ownership & Lifetime

Ownership hierarchy:


> **Code moved to Appendix A:** see Listing 49 (from `Phase 3 — Resources & Virtual File System.md` — 9. VFS Ownership & Lifetime).


## Phase 4 — Resource Manager


* **Resource identity is decoupled from physical storage** (virtual paths, mounts, packaging/portability). 
* A **Resource Manager** provides a central place to **request**, **cache**, and **share** resources so multiple systems don’t reload the same thing independently. 
* A practical resource system needs a notion of **handles/IDs** and a **lifecycle** (requested → loading → ready/failed), enabling streaming/async later. 
* The VFS exists so higher-level systems never need to know whether data comes from **loose files vs archives**. 

---

### What we implement now (tight scope)

We implement **one resource type**: a raw **binary blob** loaded from a VFS virtual path.

Capabilities:

* `RequestBinary(vpath)` returns a **ResourceHandle**
* background thread loads bytes via `VirtualFileSystem`
* main thread calls `Resources().Update()` to finalize completed loads
* you can check `IsReady(handle)` and access `GetBytes(handle)`

**Design choice (not directly from the book):**

* Resource IDs are **64-bit FNV-1a hashes** of normalized vpaths.
* Handles are **(index + generation)** to detect stale use.
* One dedicated loader thread (job system comes later).

---

### Implementation steps

1. Create the new `Engine/Resources/` files for IDs, handles, and the manager.
2. Update `Engine` to own a `ResourceManager`:

   * Init it **after VFS mounts**
   * Shutdown it **before** shutting down core subsystems (so memory/logging are still valid)
   * Call `resources_.Update()` during `Engine::Tick()`
3. Update `NocturneHost` to request `hello.txt` via the Resource Manager and log the results (host-side test, not in engine init).

---

### Verification checklist (Phase 4 done when…)

* [x] Running `NocturneHost` logs that the Resource Manager started.
* [x] `hello.txt` is requested via `engine.Resources().RequestBinary("hello.txt")`.
* [x] It becomes **Ready** and logs file size and a short preview.
* [x] Closing the window still shuts down cleanly with no leaks/crashes.
* [x] Requesting the same vpath twice returns the **same cached resource** (no duplicate load).

### Verification (Phase 4)

#### Test Setup

- `NocturneHost` sets the content root explicitly via a build macro:
  - `engine.SetContentRoot(NOC_CONTENT_ROOT)`
- A test file exists at:
  - `${NOC_CONTENT_ROOT}/hello.txt`

#### Expected Behavior

- The engine mounts the loose directory content root.
- The Resource Manager starts a loader thread.
- `hello.txt` is requested via `RequestBinary("hello.txt")`.
- The resource transitions to `READY`.
- Requesting the same virtual path twice returns the same cached handle.
- Shutdown is clean with no outstanding allocations.

#### Verified Log Output


> **Code moved to Appendix A:** see Listing 50 (from `Phase 4 — Resource Manager.md` — Verification (Phase 4) > Verified Log Output).


---

### Common pitfalls

* Calling `ResourceManager::Shutdown()` **after** Memory shutdown (you’ll crash/free invalid allocator).
* Accessing resource bytes without checking `IsReady()`.
* Forgetting to pump `Resources().Update()` each frame (loads will complete in the worker, but never get finalized).
* Treating OS paths as identities instead of virtual paths (breaks mounting/archives later).

---

### Next chat handoff (exactly what to say/bring)

> “Phase 4 is complete. I can request `hello.txt` through the Resource Manager and it loads asynchronously via the VFS. Start Phase 5: typed resources + loaders (text, JSON, image/texture stub, mesh stub).”

---

## Implementations

Below are **every file implemented/updated in Phase 4**, each with its **path** and **full implementation**.

---

### `Engine/Resources/ResourceID.h`


> **Code moved to Appendix A:** see Listing 51 (from `Phase 4 — Resource Manager.md` — Implementations > `Engine/Resources/ResourceID.h`).


---

### `Engine/Resources/ResourceHandle.h`


> **Code moved to Appendix A:** see Listing 52 (from `Phase 4 — Resource Manager.md` — Implementations > `Engine/Resources/ResourceHandle.h`).


---

### `Engine/Resources/ResourceManager.h`


> **Code moved to Appendix A:** see Listing 53 (from `Phase 4 — Resource Manager.md` — Implementations > `Engine/Resources/ResourceManager.h`).


---

### `Engine/Resources/ResourceManager.cpp`


> **Code moved to Appendix A:** see Listing 54 (from `Phase 4 — Resource Manager.md` — Implementations > `Engine/Resources/ResourceManager.cpp`).


---

### `Engine/Runtime/Engine.h` (updated)


> **Code moved to Appendix A:** see Listing 55 (from `Phase 4 — Resource Manager.md` — Implementations > `Engine/Runtime/Engine.h` (updated)).


---

### `Engine/Runtime/Engine.cpp` (updated)


> **Code moved to Appendix A:** see Listing 56 (from `Phase 4 — Resource Manager.md` — Implementations > `Engine/Runtime/Engine.cpp` (updated)).


---

### `Apps/NocturneHost/main.cpp` (updated)


> **Code moved to Appendix A:** see Listing 57 (from `Phase 4 — Resource Manager.md` — Implementations > `Apps/NocturneHost/main.cpp` (updated)).


---

## Phase 5 — Typed Resources & Loader Registry


**Objective:** Extend the Phase 4 Resource Manager to support **typed resource requests** (e.g., Text, JSON later, Image later) using:

- a **ResourceType** tag on each record,
- a **ResourceLoaderRegistry** that selects the correct loader,
- an **IResourceLoader** contract that defines threading + ownership rules,
- one real example: **TextResource** (UTF-8 text file) end-to-end.

This keeps “files are not resources” intact: vpaths identify *what you want*, loaders define *how bytes become a runtime object*. 

---

### Key concepts from the books

- **Resource identity and caching**: Resources need stable IDs and a manager to avoid duplicate loads and centralize lifecycle. (Gregory; Phase 4 implements the “foundation” of this.) 
- **Asynchronous producer/consumer handoff**: When work spans threads, you need a queue/hand-off point and a clear “who owns processing” rule. This is the same architectural shape as an event-queue spanning threads. 
- **Narrow interfaces**: Keep typed resource complexity behind the resource system; don’t leak loader details into gameplay code. (Pattern-aligned guidance on minimizing exposed surface area.) 

---

### What we implement now

Tight scope (Phase 5 is *not* a job system, *not* rendering, *not* GPU uploads):

1. **ResourceType enum** (starts small).
2. **Typed handle** `ResourceHandleT<T>` (wraps existing `ResourceHandle`).
3. **IResourceLoader** interface:
   - advertises `ResourceType`
   - validates/decodes bytes into a runtime object
4. **ResourceLoaderRegistry**:
   - `RegisterLoader(type, loader*)`
   - `FindLoader(type) -> loader*`
5. **ResourceManager typed API**:
   - `RequestText(vpath)` returns `ResourceHandleT<TextResource>`
   - `GetText(handle)` returns pointer/ref when ready
6. **Concrete example**: **TextResource** + **TextResourceLoader**
   - loader reads bytes (VFS) via existing Phase 4 path
   - decodes UTF-8 into `std::string`
   - main thread finalizes state to READY/FAILED
7. **NocturneHost test**:
   - request `"hello.txt"` as Text
   - log its size + preview

**Design choice (not directly from the book):**
- We keep the Phase 4 dedicated loader thread and add typed decoding there (safe for CPU-only types). A job system replaces this later. 

---

### Typed Resource Lifecycle (authoritative)

This is the lifecycle that every typed resource must follow.

#### 1) Request
- Call: `RequestText("path.txt")`
- ResourceManager:
  - normalizes vpath → ResourceID (Phase 4)
  - looks up cache:
    - if present, returns existing handle
    - else creates a new record:
      - `state = REQUESTED`
      - `type = ResourceType::Text`
      - `loader = registry.FindLoader(Text)`
- Enqueues request for loader thread.

#### 2) Load (loader thread)
- ResourceManager performs **I/O** via VFS (Phase 4 style).
- On success: passes raw bytes to loader for **Decode**:
  - `Decode(bytes) -> object or error`
- Stores decoded object into a “completed” payload and pushes it to the completion queue.

#### 3) Finalize (main thread)
- `ResourceManager::Update()` drains completion queue.
- For each completed item:
  - installs the decoded object into the record
  - transitions:
    - READY if decode succeeded
    - FAILED if I/O or decode failed
- From here on:
  - `GetText(handle)` is valid iff READY
  - cached object is shared by all requesters

#### 4) Use
- Gameplay/systems only see:
  - `ResourceHandleT<TextResource>`
  - `const TextResource*` (or `std::string_view`)
- No file paths, no loaders, no VFS knowledge outside resources.

#### 5) Shutdown
- ResourceManager shuts down loader thread
- Clears records after thread joins (no concurrent access).

This matches the diagram’s intent: async worker produces results, main thread finalizes ownership/state. 

---

### Loader contracts (non-negotiable)

#### A) Registration & identity
- Each loader handles exactly one `ResourceType` (initially).
- Registry registration must happen during engine init (or subsystem startup), before gameplay requests.

#### B) Threading rules
- `Decode()` **runs on the loader thread** in Phase 5.
  - It must be **CPU-only** and must not touch OS windowing, input state, GPU objects, or global singletons that are not thread-safe.
- `Finalize()` is performed by the ResourceManager on the **main thread** (inside `Resources().Update()`).

Why: we want the same shape as a producer/consumer queue spanning threads. 

#### C) Ownership rules
- A loader returns a result whose memory is **owned by ResourceManager** after completion.
- A typed resource object becomes immutable once READY.
- Consumers must treat returned pointers/refs as read-only.

#### D) Error handling
- Loader must produce a stable error string on failure.
- ResourceManager stores the error on the record for debugging:
  - `HasFailed(handle)`
  - `GetError(handle)`

#### E) Determinism
- Given identical bytes and loader version, `Decode()` must be deterministic.

---

### Implementation steps

1. Add **typed resource core types**:
   - `ResourceType`
   - `ResourceHandleT<T>`
   - `ResourceLoadContext`, `ResourceLoadResult`
2. Add loader interfaces:
   - `IResourceLoader`
   - `ResourceLoaderRegistry`
3. Extend `ResourceManager`:
   - add typed request path
   - store per-record: `ResourceType`, decoded object pointer (type-erased)
4. Implement **TextResource + TextResourceLoader**
5. Integrate into engine startup:
   - create registry
   - register `TextResourceLoader`
6. Host test:
   - request text resource
   - log success + preview

---

### Verification checklist (Phase 5 done when…)

- [ ] Engine boots and initializes ResourceLoaderRegistry.
- [ ] `TextResourceLoader` is registered.
- [ ] `RequestText("hello.txt")` returns a handle immediately (non-blocking).
- [ ] Within a few frames, resource transitions to READY.
- [ ] You can call `GetText(handle)` and log:
  - full size
  - first N characters preview
- [ ] Duplicate requests for the same vpath return the same cached resource (no double-load).
- [ ] Bad path transitions to FAILED and provides error string.
- [ ] Shutdown joins loader thread and exits cleanly (no leaks, no deadlocks).

---

### Common pitfalls

- **Letting loaders touch GPU or renderer state**: that will deadlock later when a render thread exists.
- **Returning views into temporary memory**: decoded objects must outlive the completion handoff.
- **Skipping main-thread finalize**: READY/FAILED transitions must be single-threaded to keep record state simple.
- **Leaking loader details into gameplay**: gameplay should not know “which loader” or “where bytes came from”.

---

### Implementations

> Notes:
> - Paths assume your existing layout (`Engine/Resources/...`) from Phase 4. 
> - Names follow the architecture diagram vocabulary. 

#### Engine/Resources/Typed/ResourceType.h
> **Code moved to Appendix A:** see Listing 58 (from `Phase 5 — Typed Resources & Loader Registry.md` — Implementations > Engine/Resources/Typed/ResourceType.h).


#### Engine/Resources/Typed/IResourceLoader.h


> **Code moved to Appendix A:** see Listing 59 (from `Phase 5 — Typed Resources & Loader Registry.md` — Implementations > Engine/Resources/Typed/IResourceLoader.h).


#### Engine/Resources/Typed/ResourceLoaderRegistry.h


> **Code moved to Appendix A:** see Listing 60 (from `Phase 5 — Typed Resources & Loader Registry.md` — Implementations > Engine/Resources/Typed/ResourceLoaderRegistry.h).


#### Engine/Resources/Typed/TextResource.h


> **Code moved to Appendix A:** see Listing 61 (from `Phase 5 — Typed Resources & Loader Registry.md` — Implementations > Engine/Resources/Typed/TextResource.h).


#### Engine/Resources/Typed/TextResourceLoader.h


> **Code moved to Appendix A:** see Listing 62 (from `Phase 5 — Typed Resources & Loader Registry.md` — Implementations > Engine/Resources/Typed/TextResourceLoader.h).


#### Engine/Resources/ResourceManager.h


> **Code moved to Appendix A:** see Listing 63 (from `Phase 5 — Typed Resources & Loader Registry.md` — Implementations > Engine/Resources/ResourceManager.h).


#### Engine/Resources/ResourceManager.cpp (Phase 5 additions/updates)


> **Code moved to Appendix A:** see Listing 64 (from `Phase 5 — Typed Resources & Loader Registry.md` — Implementations > Engine/Resources/ResourceManager.cpp (Phase 5 additions/updates)).


---

### Next chat handoff (ONLY what to say/bring next)

“Phase 5 is implemented. Here are the new files and the ResourceManager diffs. I can request TextResource and it becomes READY. Now start Phase 6 — Job System & Async Infrastructure.”
## Phase 6 — Job System & Async Infrastructure


**Objective:** Implement a minimal but production-leaning **Job System** and refactor ResourceManager to schedule work on it, replacing the dedicated loader thread while preserving:

- stable resource identity + caching
- explicit states (Requested → Loading → Ready/Failed)
- **main-thread finalization** and predictable frame boundaries
- clean shutdown and leak-free behavior

---

### Key concepts from the books (book-grounded)

- **Engine subsystems own execution**: the engine controls when and how work happens (main loop is the orchestrator).   
- **Asynchronous work needs explicit handoff**: background production + deterministic consumption/finalization is a common engine pattern for correctness and debuggability (resource loading is the canonical example).    
- **Avoid bespoke threads per system**: consolidate async into shared infrastructure (job system / task system) to reduce complexity and contention (engine architecture principle; implementation details are our design choices).

---

### What we implement now (tight scope)

#### A) Job System (Engine/Jobs)
1. **JobSystem** with a fixed worker thread pool.
2. A way to **enqueue** work items (`JobHandle` returned).
3. A way to **wait** on a job (for tests and controlled shutdown).
4. A per-frame **pump point** (optional now, but we’ll include `BeginFrame()` to support future budgeting).

#### B) ResourceManager refactor
1. Remove the dedicated loader thread and its condition-variable loop.
2. On request, schedule the resource load pipeline using jobs:
   - **Job 1:** VFS read bytes (and/or decode/parse, depending on your current typed loader rules).
   - **Completion record:** push to `completed` queue for main thread.
3. `ResourceManager::Update()` remains the sole finalizer that flips READY/FAILED and publishes resource payloads.

**Non-goals (explicitly out of scope for Phase 6):**
- work stealing
- fibers / job continuation stacks
- per-job priorities beyond a small enum stub
- hard budgeting / frame time slicing
- CPU affinity / NUMA tuning

**Design choice (not directly from the book):**
- Start with a simple MPMC queue + worker threads + `JobHandle` built on an atomic counter (or a small shared state). We will evolve it later if needed.

---

### Execution & ownership model (locked for Phase 6)

#### Threads
- **Main thread**:
  - owns subsystem `Init/Shutdown`
  - calls `Engine::Tick()` and `ResourceManager::Update()`
  - does **finalize/publish** only
- **Job worker threads**:
  - perform background tasks (I/O, parse, CPU transforms)
  - never touch Win32 windowing APIs
  - never call into renderer (future rule)
  - never mutate global engine state except via thread-safe queues

#### Resource pipeline (job-backed)
1. Request creates/finds a record in cache.
2. If new, transition to `Loading` and enqueue a job that:
   - reads bytes from VFS
   - runs loader decode (if Phase 5 allows decode off-thread)
   - writes a **Completion** struct into a thread-safe `completed` queue
3. Main thread `Update()`:
   - drains `completed`
   - validates handle + generation
   - commits payload to the resource record
   - marks `Ready` or `Failed`

This preserves the “completion/finalize model” you already have from Phase 4/5.  

---

### Implementation steps

#### 1) Add Jobs module scaffolding
- Create: `Engine/Jobs/`
  - `JobSystem.h/.cpp`
  - `JobHandle.h`
  - `JobQueue.h/.cpp` (or internal-only inside JobSystem)
  - `JobPriority.h` (stub enum)

#### 2) Implement the minimal JobSystem
- `Init(workerCount)`
- `Shutdown()`
- `Enqueue(JobDesc) -> JobHandle`
- `Wait(JobHandle)` (used only by tests/shutdown paths)
- Worker loop pops jobs until shutdown flag set.

#### 3) Engine owns JobSystem
- Add to `Engine`:
  - `JobSystem jobs_;`
  - init order: after core systems; before resources
  - shutdown order: resources before jobs (so pending resource jobs can be drained or canceled cleanly)

#### 4) Refactor ResourceManager to use jobs
- Remove:
  - loader thread creation
  - loader thread loop
  - CV-based wakeup
- Replace:
  - on resource request, enqueue a job that performs `LoadOne_()` work
  - keep the **same completion queue**
- Ensure `ResourceManager::Update()` still drains completions and finalizes.

#### 5) Verification in NocturneHost
- Existing tests must still pass:
  - load hello.txt → READY
  - cache returns same handle
  - missing file fails cleanly
- Add one additional stress test:
  - request N text files quickly (even duplicates) and confirm no deadlocks and all finalize on main thread.

---

### Verification checklist (Phase 6 done when…)

- [ ] Engine starts and shuts down cleanly with JobSystem enabled.
- [ ] ResourceManager no longer logs “loader thread started/running/exiting”.
- [ ] `hello.txt` loads successfully via job-backed work and finalizes in `Resources().Update()`.
- [ ] Missing file still transitions to FAILED with the same error surfaced to host code.
- [ ] No outstanding allocations at shutdown (same memory stats expectation as Phase 4/5).
- [ ] Rapid repeated requests don’t duplicate loads and don’t deadlock.

---

### Common pitfalls

- **Finalizing on worker threads** (breaks deterministic lifetime rules and will bite you later with GPU/OS objects).
- **Capturing raw pointers into jobs** that outlive the owning subsystem during shutdown.
- **Holding ResourceManager mutex while doing I/O** (kills concurrency and can deadlock with finalize).
- **Shutdown ordering**: if jobs stop before resources drain/cancel, completions may be lost or records left stuck in Loading.

---

### Next chat handoff (ONLY what you should say/bring next)

> “Phase 6 JobSystem is implemented and Engine owns it. Here are the logs and the changed files. ResourceManager no longer has a loader thread and hello.txt still loads. Let’s review correctness, shutdown safety, and whether decode happens on workers or main thread.”

### 6) Implementations

### `Engine/Core/Jobs/JobSystem.h`


> **Code moved to Appendix A:** see Listing 65 (from `Phase 6 — Job System & Async Infrastructure.md` — `Engine/Core/Jobs/JobSystem.h`).


### `Engine/Core/Jobs/JobSystem.cpp`


> **Code moved to Appendix A:** see Listing 66 (from `Phase 6 — Job System & Async Infrastructure.md` — `Engine/Core/Jobs/JobSystem.cpp`).


### `Engine/Runtime/Engine.h`


> **Code moved to Appendix A:** see Listing 67 (from `Phase 6 — Job System & Async Infrastructure.md` — `Engine/Runtime/Engine.h`).


### `Engine/Runtime/Engine.cpp`


> **Code moved to Appendix A:** see Listing 68 (from `Phase 6 — Job System & Async Infrastructure.md` — `Engine/Runtime/Engine.cpp`).


### `Engine/Resources/ResourceManager.h`


> **Code moved to Appendix A:** see Listing 69 (from `Phase 6 — Job System & Async Infrastructure.md` — `Engine/Resources/ResourceManager.h`).


### `Engine/Resources/ResourceManager.cpp`


> **Code moved to Appendix A:** see Listing 70 (from `Phase 6 — Job System & Async Infrastructure.md` — `Engine/Resources/ResourceManager.cpp`).


## Phase 7 — Input System (Raw Input + Snapshots + Action Mapping)


**Objective:** Add an engine-owned **InputSystem** that:

1. **Collects OS input** using **Win32 Raw Input** (keyboard + mouse) via the existing Win32 window message pump.
2. Produces a **per-frame input snapshot** (current + previous) so gameplay can query:

   * `IsDown()`, `WasPressed()`, `WasReleased()`
   * `MouseDelta()`, `WheelDelta()`
3. Provides an **ActionMap** layer that converts physical inputs into:

   * **Digital actions** (0/1)
   * **Analog actions** (float, e.g., mouse X/Y delta axes)
4. Integrates cleanly into the engine loop:

   * raw messages come in through the window pump
   * `InputSystem::BeginFrame()` runs once per frame
   * `InputSystem` update runs inside `Engine::Tick()` (same place we already “finalize/publish” work like resources).

**Design choice (not directly from the book):** We forward `WM_INPUT` from `WinWindow::WndProc` into `InputSystem` through a small platform-layer sink interface, to avoid platform → input header dependencies while keeping the message pump the single source of OS events.

---

### 2) Key concepts from the books (book-grounded)

* **Engine owns the loop and frame boundaries** (input is sampled once per frame; gameplay reads stable data).
* **Support systems live below gameplay**: platform-specific acquisition is isolated; higher layers see stable interfaces.
* **Deterministic “finalize on main thread” shape**: treat input like other producer/consumer systems—OS produces events, engine publishes a stable per-frame snapshot. (Same shape we used for resources in `Engine::Tick()`.)

---

### 3) What we implement now (tight scope)

#### A) Windows raw input acquisition

* Register raw input devices (keyboard + mouse) for the engine window.
* Handle `WM_INPUT` and decode `RAWINPUT` into small internal events.

#### B) Per-frame buffered snapshot

* `BeginFrame()`:

  * copies `current → previous`
  * resets per-frame deltas (mouse delta, wheel)
  * clears “pressed/released this frame” transient flags
* Event consumption updates `current` and transient flags.

#### C) Action mapping

* `ActionMap` supports bindings:

  * **Digital**: Key, MouseButton
  * **Analog**: MouseDeltaX/Y, MouseWheel
* Query:

  * `GetActionValue(ActionId)` → float
  * `IsActionDown(ActionId)` convenience

#### D) Integration

* `MainLoop` sets the raw input sink on the window after creation.
* `Engine::Tick()` calls input begin/update before simulation (simulation comes later).

---

### 4) Implementation steps

1. Add **Input module** scaffolding:

   * `Engine/Input/InputTypes.h`
   * `Engine/Input/ActionMap.h/.cpp`
   * `Engine/Input/InputSystem.h/.cpp`
2. Extend **WinWindow** to forward `WM_INPUT` to an interface sink:

   * `platform::IRawInputSink`
   * `WinWindow::SetRawInputSink()`
3. Register for raw input when the window exists:

   * `InputSystem::AttachToWindow(void* hwnd)` (opaque handle)
4. Integrate into runtime:

   * `Engine` owns `InputSystem input_;`
   * `Engine::Tick()` calls:

     * `input_.BeginFrame();`
     * `input_.Update();`
     * `resources_.Update();` (existing pattern) 
5. Provide a default `ActionMap` with a few example bindings (WASD, mouse look axes).

---

### 5) Verification checklist (Phase 7 done when…)

* [ ] Moving the mouse changes `MouseDelta()` every frame (and resets to 0 when idle).
* [ ] Pressing and releasing a key:

  * `IsDown()` true while held
  * `WasPressed()` true for exactly one frame on press
  * `WasReleased()` true for exactly one frame on release
* [ ] Action map returns:

  * `MoveForward` is 1.0 while `W` held
  * `LookX` changes with mouse movement
* [ ] Engine shuts down cleanly (no dangling sink pointer on window destroy).

---

### 6) Common pitfalls

* Forgetting to reset per-frame deltas (mouse delta “sticks”).
* Treating raw input events as the authoritative “current state” without buffering (gameplay reads mid-frame changes).
* Mishandling focus loss (keys can get “stuck down”).
  **We handle this by clearing state on focus loss.**

---

### 7) Next chat handoff (ONLY what you should say/bring next)

> “Phase 7 InputSystem is implemented. Raw Input is wired through WinWindow WM_INPUT, InputSystem snapshots per frame, ActionMap returns digital+analog values. Here are logs showing key press, mouse delta, and action values. Start Phase 8 — Rendering Bootstrap.”

---

## C++ Implementations (Phase 7)

Below are **full implementations** for the Input subsystem + the required engine/platform integrations.

---

### `Engine/Input/InputTypes.h`


> **Code moved to Appendix A:** see Listing 71 (from `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Input/InputTypes.h`).


---

### `Engine/Input/ActionMap.h`


> **Code moved to Appendix A:** see Listing 72 (from `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Input/ActionMap.h`).


---

### `Engine/Input/ActionMap.cpp`


> **Code moved to Appendix A:** see Listing 73 (from `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Input/ActionMap.cpp`).


---

### `Engine/Input/InputSystem.h`


> **Code moved to Appendix A:** see Listing 74 (from `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Input/InputSystem.h`).


> **Note:** `InputSystem` “is-a” `ActionMap` here to keep public surface small.
> **Design choice (not directly from the book):** you may later prefer composition (`InputSystem` owns an `ActionMap`) once you have multiple maps/profiles.

---

### `Engine/Input/InputSystem.cpp`


> **Code moved to Appendix A:** see Listing 75 (from `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Input/InputSystem.cpp`).


---

### `Engine/Platform/Win32/WinWindow.h` (UPDATED)


> **Code moved to Appendix A:** see Listing 76 (from `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Platform/Win32/WinWindow.h` (UPDATED)).


---

### `Engine/Platform/Win32/WinWindow.cpp` (UPDATED: WM_INPUT + focus)


> **Code moved to Appendix A:** see Listing 77 (from `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Platform/Win32/WinWindow.cpp` (UPDATED: WM_INPUT + focus)).


---

### `Engine/Runtime/Engine.h` (UPDATED: owns InputSystem)


> **Code moved to Appendix A:** see Listing 78 (from `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Runtime/Engine.h` (UPDATED: owns InputSystem)).


---

### `Engine/Runtime/Engine.cpp` (UPDATED: Input init + tick integration)


> **Code moved to Appendix A:** see Listing 79 (from `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Runtime/Engine.cpp` (UPDATED: Input init + tick integration)).


---

### `Engine/Runtime/MainLoop.h` (unchanged structure; shown for completeness)


> **Code moved to Appendix A:** see Listing 80 (from `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Runtime/MainLoop.h` (unchanged structure; shown for completeness)).


---

### `Engine/Runtime/MainLoop.cpp` (unchanged; shown for completeness)


> **Code moved to Appendix A:** see Listing 81 (from `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Runtime/MainLoop.cpp` (unchanged; shown for completeness)).


---

### `Apps/NocturneHost/main.cpp`
> **Code moved to Appendix A:** see Listing 82 (from `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Apps/NocturneHost/main.cpp`).

---

If you want, in the next message paste a short log snippet while pressing **W** and moving the mouse, and I’ll tell you exactly what to print (and where) to prove the snapshot + action map are behaving correctly.

## Phase 8 — Rendering Bootstrap (DirectX 12)


**Objective:** Add an engine-owned **RenderSystem** (DX12 bootstrap) that:

1. Creates a D3D12 **device** (hardware adapter), **command queue**, and **swap chain**.
2. Allocates an **RTV descriptor heap** and RTVs for the back buffers.
3. Maintains **per-frame command allocators** and a single **graphics command list**.
4. Uses **fences** to safely reuse per-frame allocators and present.
5. Integrates into the engine loop:
   - `Engine::BeginFrame()` resets command structures
   - `Engine::EndFrame()` clears the current back buffer and presents

Result: a window that clears to a solid color every frame (no triangle yet).

---

### 2) Key concepts from the books (book-grounded)

- **Engine owns the main loop and frame boundaries**; rendering is a subsystem called deterministically each frame. (Main loop orchestration pattern already established in Phase 2/7.)   
- **Systems should finalize/publish on the main thread**; Phase 4/5 established this “do work then publish at a known point” shape (we apply the same discipline to GPU submission/present).   
- **Layering + dependency rules:** Render sits below gameplay and uses platform only through narrow handles (HWND as `void*`).   

**Design choice (not directly from the book):** This phase is a single-threaded render path (main-thread submits). A dedicated render thread is deferred.

---

### 3) What we implement now (tight scope)

#### Render bootstrap only
- DXGI factory + adapter selection
- D3D12 device
- command queue
- swap chain (flip-discard, 2 buffers)
- RTV heap + back buffer RTVs
- per-frame command allocator
- one command list
- fence + event
- per-frame:
  - transition Present→RT
  - clear RTV
  - transition RT→Present
  - execute, present, signal fence

#### Not included (explicitly out of scope)
- Resize handling (we store client size for later; no swapchain resize path yet)
- Root signatures, PSOs, shaders
- GPU resource allocator, descriptor allocators beyond RTV heap
- Render graph / frame graph

---

### 4) Implementation steps

1. Add `Engine/Render/RenderSystem.h/.cpp` (DX12 bootstrap hidden in `.cpp`).
2. Extend `WinWindow` to store and expose client size (`ClientWidth/ClientHeight`) and update on `WM_SIZE`.
3. Modify `Engine`:
   - own `RenderSystem render_;`
   - init in `Engine::Init()` (lightweight; no HWND)
   - attach in `Engine::AttachWindow()` (create device/swapchain using HWND)
   - call `render_.BeginFrame()` in `Engine::BeginFrame()`
   - call `render_.EndFramePresent()` in `Engine::EndFrame()`
4. Verification: see clear color + present continuously; clean shutdown; no D3D12 debug errors (when enabled).

---

### 5) Verification checklist (Phase 8 done when…)

- [ ] Window opens and continuously clears to a solid color.
- [ ] GPU validation/debug layer can be toggled (debug builds).
- [ ] No device removed / DXGI errors during normal run.
- [ ] Closing window exits cleanly (fence wait + release is safe).
- [ ] Logs show render init success and orderly shutdown.

---

### 6) Common pitfalls

- Reusing a command allocator before the GPU is done (missing fence wait).
- Forgetting resource barriers Present↔RenderTarget.
- Creating swap chain without the correct HWND or wrong format.
- Clearing without setting the correct RTV handle for the current back buffer.

---

### 7) Next chat handoff (ONLY what you should say/bring next)

> “Phase 8 is implemented. DX12 device/queue/swapchain/RTV heap/fences are working. The window clears and presents every frame. Here are logs + a screenshot. Start Phase 9 — Rendering Engine Foundation (GPU resources + PSO/shaders + first triangle).”

---

## Implementations (Phase 8)

Below are **all files implemented/modified in Phase 8**, each with:
- `path/to/file`
- full implementation

---

### `Engine/Render/RenderSystem.h`


> **Code moved to Appendix A:** see Listing 83 (from `Phase 8 — Rendering Bootstrap (DirectX 12).md` — Implementations (Phase 8) > `Engine/Render/RenderSystem.h`).


---

### `Engine/Platform/Win32/WinWindow.h` (MODIFIED: expose client size)


> **Code moved to Appendix A:** see Listing 84 (from `Phase 8 — Rendering Bootstrap (DirectX 12).md` — Implementations (Phase 8) > `Engine/Platform/Win32/WinWindow.h` (MODIFIED: expose client size)).


---

### `Engine/Platform/Win32/WinWindow.cpp` (MODIFIED: track WM_SIZE)


> **Code moved to Appendix A:** see Listing 85 (from `Phase 8 — Rendering Bootstrap (DirectX 12).md` — Implementations (Phase 8) > `Engine/Platform/Win32/WinWindow.cpp` (MODIFIED: track WM_SIZE)).


---

### `Engine/Runtime/Engine.h` (MODIFIED: owns RenderSystem)


> **Code moved to Appendix A:** see Listing 86 (from `Phase 8 — Rendering Bootstrap (DirectX 12).md` — Implementations (Phase 8) > `Engine/Runtime/Engine.h` (MODIFIED: owns RenderSystem)).


---

### `Engine/Runtime/Engine.cpp` (MODIFIED: attach + frame integration)


> **Code moved to Appendix A:** see Listing 87 (from `Phase 8 — Rendering Bootstrap (DirectX 12).md` — Implementations (Phase 8) > `Engine/Runtime/Engine.cpp` (MODIFIED: attach + frame integration)).


---

### Notes you will likely need in your build system (FYI)

* Ensure you link: `d3d12`, `dxgi`, `dxguid` (the `.cpp` also has `#pragma comment(lib, ...)` to help on MSVC).
* Windows SDK must include D3D12 headers (VS2022 default is fine).

---

If you want, paste your current CMakeLists for the engine target and I’ll give you the exact `target_link_libraries(...)` lines (but the code above should already compile under MSVC due to the pragmas).


> **Code moved to Appendix A:** see Listing 88 (from `Phase 8 — Rendering Bootstrap (DirectX 12).md` — Implementations (Phase 8) > Notes you will likely need in your build system (FYI)).


## Phase 9 — Rendering Engine Foundation


Implement “first triangle” while keeping:

* swap chain + per-frame allocator/list reset + fences model intact 
* debug layer clean (no errors)
* RenderSystem file size under control by splitting responsibilities

### 2) Key concepts from the books

* Root signature defines what resources shaders expect; PSO creation validates root signature + shaders compatibility.
* Runtime shader compilation via `D3DCompileFromFile` (we’ll support both file and string; file is recommended long-term). 
* Minimize root signature changes; keep it small. 

### 3) What we implement now

**RenderSystem orchestration:**

* delegates to an internal DX12 renderer object

**DX12 modules (private headers):**

* `Dx12Device` — factory/adapter/device/queue
* `Dx12SwapChain` — swapchain, RTV heap, back buffers, transitions, current index
* `Dx12FrameSync` — fence/event, per-frame fence values, wait/advance
* `ShaderCompiler` — compile HLSL (string or file)
* `TrianglePass` — root signature + PSO + VB, and records the draw commands

### 4) Implementation steps

1. Add `Engine/Render/DX12/` private module files.
2. RenderSystem becomes thin wrapper: `Init/Attach/BeginFrame/EndFramePresent`.
3. `Dx12SwapChain` owns back buffers + RTV heap and provides RTV handle + transition helper.
4. `TrianglePass::Init()` creates root sig + shaders + PSO + vertex buffer.
5. `TrianglePass::Record()` records: set RT, clear, bind PSO/root sig, viewport/scissor, IA, draw.
6. Keep present + fence advance exactly as Phase 8. 

### 5) Verification checklist

* [ ] Triangle visible (not just clear)
* [ ] Debug layer: 0 errors, 0 warnings
* [ ] No device removed
* [ ] Shutdown waits for GPU and cleanly releases
* [ ] Frame indexing/fences still correct (no allocator reuse hazards) 

### 6) Common pitfalls

* Root signature mismatch with shaders → PSO creation failure
* RTV format mismatch between swapchain and PSO
* Forgetting viewport/scissor or topology
* Resource state transitions missing present↔RT (debug layer will complain) 

### 7) Next chat handoff

> “Phase 9 SRP version is implemented: RenderSystem is thin, DX12 is split into Device/SwapChain/Sync/ShaderCompiler/TrianglePass. The engine draws a triangle every frame with zero debug layer errors. Start Phase 10 — GPU Resource Foundations (default heap + upload staging + descriptor heaps + basic texture/SRV).”

---

## Implementations (Clean Phase 9)

### `Engine/Render/RenderSystem.h` (UNCHANGED)

*(Same as Phase 8; keep public header STL-free and platform-opaque.)* 


> **Code moved to Appendix A:** see Listing 89 (from `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/RenderSystem.h` (UNCHANGED)).


---

### `Engine/Render/RenderSystem.cpp` (REPLACED — now small)


> **Code moved to Appendix A:** see Listing 90 (from `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/RenderSystem.cpp` (REPLACED — now small)).


---

### `Engine/Render/DX12/Dx12Common.h` (NEW)


> **Code moved to Appendix A:** see Listing 91 (from `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12Common.h` (NEW)).


---

### `Engine/Render/DX12/Dx12Device.h` (NEW)


> **Code moved to Appendix A:** see Listing 92 (from `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12Device.h` (NEW)).


---

### `Engine/Render/DX12/Dx12Device.cpp` (NEW)


> **Code moved to Appendix A:** see Listing 93 (from `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12Device.cpp` (NEW)).


---

### `Engine/Render/DX12/Dx12FrameSync.h` (NEW)


> **Code moved to Appendix A:** see Listing 94 (from `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12FrameSync.h` (NEW)).


---

### `Engine/Render/DX12/Dx12FrameSync.cpp` (NEW)


> **Code moved to Appendix A:** see Listing 95 (from `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12FrameSync.cpp` (NEW)).


---

### `Engine/Render/DX12/Dx12SwapChain.h` (NEW)


> **Code moved to Appendix A:** see Listing 96 (from `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12SwapChain.h` (NEW)).


---

### `Engine/Render/DX12/Dx12SwapChain.cpp` (NEW)


> **Code moved to Appendix A:** see Listing 97 (from `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12SwapChain.cpp` (NEW)).


---

### `Engine/Render/DX12/ShaderCompiler.h` (NEW)


> **Code moved to Appendix A:** see Listing 98 (from `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/ShaderCompiler.h` (NEW)).


---

### `Engine/Render/DX12/ShaderCompiler.cpp` (NEW)


> **Code moved to Appendix A:** see Listing 99 (from `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/ShaderCompiler.cpp` (NEW)).


---

### `Engine/Render/DX12/TrianglePass.h` (NEW)


> **Code moved to Appendix A:** see Listing 100 (from `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/TrianglePass.h` (NEW)).


---

### `Engine/Render/DX12/TrianglePass.cpp` (NEW)


> **Code moved to Appendix A:** see Listing 101 (from `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/TrianglePass.cpp` (NEW)).


---

### `Engine/Render/DX12/Dx12Renderer.h` (NEW)


> **Code moved to Appendix A:** see Listing 102 (from `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12Renderer.h` (NEW)).


---

### `Engine/Render/DX12/Dx12Renderer.cpp` (NEW)


> **Code moved to Appendix A:** see Listing 103 (from `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12Renderer.cpp` (NEW)).


---

### Why this structure stays clean in Phase 10+

This decomposition maps naturally to what your architecture diagram implies for render responsibilities (“CreateBuffer / CreateTexture / CreatePipelineState / RenderFrame”) :

* `Dx12Device` grows into feature checks, descriptor heap managers, allocator hooks.
* `Dx12SwapChain` later gets resize + HDR choices, but stays swapchain-only.
* `TrianglePass` becomes your first “pass object” and evolves into mesh draw, constants, etc.
* `ShaderCompiler` becomes an asset-backed system later (compile offline / cache), but API stays stable.

And crucially: `RenderSystem.cpp` stays tiny, stable, and boring.
## Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources)


Build a **first-class GPU resource layer** for Nocturne Engine:

* GPU memory model (default heap + upload staging)
* reusable descriptor heaps with stable handles (no per-frame recreation)
* minimal root signature that scales to materials/textures
* runtime shader compilation from VFS-backed sources
* PSO cache (no per-pass duplication)
* asset-backed mesh upload (CPU bytes → GPU default buffers) with async “not ready yet” behavior
* GPU-safe destruction (no releasing in-flight resources)

---

### 2) Key concepts from the books

* **Deterministic handoff points**: background/async stages do work, then publish at a known point to keep behavior debuggable and avoid races—Phase 5/6 already established this for CPU assets; we apply the same discipline to GPU uploads and deferred releases.
* **Renderer responsibility separation**: submit draw work (application stage) and manage GPU state/resources cleanly; don’t leak platform/GPU details across layers. 
* **Descriptors and descriptor heaps**: views (CBV/SRV/UAV + Samplers) live in descriptor heaps and are what the GPU binds; you typically create descriptor heaps up front and allocate within them rather than recreating every frame.

---

### 3) What we implement now

#### A) GPU memory + upload path

* `GpuBuffer` abstraction supporting:

  * static vertex buffer (default heap) + upload staging
  * static index buffer (default heap) + upload staging
  * per-frame constant buffer (upload heap, 256-byte aligned allocations)
* correct state transitions during upload:

  * `COMMON/COPY_DEST → VERTEX_AND_CONSTANT_BUFFER` or `INDEX_BUFFER`

#### B) Descriptor heap infrastructure

* persistent shader-visible:

  * CBV/SRV/UAV heap (e.g., 1024 descriptors)
  * Sampler heap (e.g., 64 descriptors)
* simple linear allocator (stable handles across frames; no free yet)

#### C) Minimal future-proof root signature

* Root parameter slots (documented):

  * `0`: per-frame CBV descriptor table (b0)
  * `1`: per-draw/per-material SRV descriptor table (t0..)
  * `2`: sampler descriptor table (s0..)

#### D) VFS-backed shaders + PSO cache

* shader source loaded through ResourceManager as `TextResource` (VFS-backed)
* compile at runtime (dev-friendly)
* PSO cache keyed by “VS+PS+rootSig+rtvFormat+inputLayout”

#### E) Asset-backed mesh upload

* mesh asset loaded through ResourceManager as **Binary** (VFS-backed)
* parse a tiny Nocturne mesh format (`NMSH`) into CPU arrays
* upload to default-heap VB/IB via the upload path
* if mesh/shader not ready: pass does “clear only” (no crash, no stalls)

#### F) GPU-safe destruction

* deferred release queue tagged with “fence value that will be signaled for this frame”
* collect when fence completed value passes tag

---

### 4) Implementation steps

1. Add DX12 foundation modules:

   * `Dx12DescriptorAllocator`
   * `Dx12DeferredReleaseQueue`
   * `Dx12UploadTracker`
   * `GpuBuffer` + `GpuRingConstantBuffer`
2. Replace `TrianglePass` with `MeshPass`:

   * loads shader source (TextResource) and mesh bytes (Binary)
   * creates root signature + PSO
   * creates descriptors for per-frame CBVs
   * uploads mesh to default heap and draws indexed
3. Extend `Dx12FrameSync` with tiny accessors:

   * `FenceValueForFrame(frameIndex)`
   * `CompletedValue()`
4. Wire ResourceManager pointer into RenderSystem → Dx12Renderer (thin change).
5. Verification: run with debug layer and ensure **zero errors** while rendering a mesh from default heap.

---

## 5) Full C++ implementations (Phase 9.5)

Below are **every new or modified file** for Phase 9.5.
Each file is preceded by its **engine-relative path**.

---

### `Engine/Render/RenderSystem.h` (MODIFIED)


> **Code moved to Appendix A:** see Listing 104 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/RenderSystem.h` (MODIFIED)).


---

### `Engine/Render/RenderSystem.cpp` (MODIFIED)


> **Code moved to Appendix A:** see Listing 105 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/RenderSystem.cpp` (MODIFIED)).


---

### `Engine/Render/DX12/Dx12FrameSync.h` (MODIFIED)


> **Code moved to Appendix A:** see Listing 106 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12FrameSync.h` (MODIFIED)).


---

### `Engine/Render/DX12/Dx12FrameSync.cpp` (MODIFIED)


> **Code moved to Appendix A:** see Listing 107 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12FrameSync.cpp` (MODIFIED)).


---

### `Engine/Render/DX12/Dx12DescriptorAllocator.h` (NEW)


> **Code moved to Appendix A:** see Listing 108 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12DescriptorAllocator.h` (NEW)).


---

### `Engine/Render/DX12/Dx12DescriptorAllocator.cpp` (NEW)


> **Code moved to Appendix A:** see Listing 109 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12DescriptorAllocator.cpp` (NEW)).


---

### `Engine/Render/DX12/Dx12DeferredReleaseQueue.h` (NEW)


> **Code moved to Appendix A:** see Listing 110 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12DeferredReleaseQueue.h` (NEW)).


---

### `Engine/Render/DX12/Dx12DeferredReleaseQueue.cpp` (NEW)


> **Code moved to Appendix A:** see Listing 111 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12DeferredReleaseQueue.cpp` (NEW)).


---

### `Engine/Render/DX12/GpuBuffer.h` (NEW)


> **Code moved to Appendix A:** see Listing 112 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/GpuBuffer.h` (NEW)).


---

### `Engine/Render/DX12/GpuBuffer.cpp` (NEW)


> **Code moved to Appendix A:** see Listing 113 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/GpuBuffer.cpp` (NEW)).


---

### `Engine/Render/DX12/GpuRingConstantBuffer.h` (NEW)


> **Code moved to Appendix A:** see Listing 114 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/GpuRingConstantBuffer.h` (NEW)).


---

### `Engine/Render/DX12/GpuRingConstantBuffer.cpp` (NEW)


> **Code moved to Appendix A:** see Listing 115 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/GpuRingConstantBuffer.cpp` (NEW)).


---

### `Engine/Render/DX12/Dx12PsoCache.h` (NEW)


> **Code moved to Appendix A:** see Listing 116 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12PsoCache.h` (NEW)).


---

### `Engine/Render/DX12/Dx12PsoCache.cpp` (NEW)


> **Code moved to Appendix A:** see Listing 117 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12PsoCache.cpp` (NEW)).


---

### `Engine/Render/DX12/MeshFormat.h` (NEW)


> **Code moved to Appendix A:** see Listing 118 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/MeshFormat.h` (NEW)).


---

### `Engine/Render/DX12/MeshFormat.cpp` (NEW)


> **Code moved to Appendix A:** see Listing 119 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/MeshFormat.cpp` (NEW)).


---

### `Engine/Render/DX12/ShaderCompiler.h` (MODIFIED)


> **Code moved to Appendix A:** see Listing 120 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/ShaderCompiler.h` (MODIFIED)).


---

### `Engine/Render/DX12/ShaderCompiler.cpp` (MODIFIED)


> **Code moved to Appendix A:** see Listing 121 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/ShaderCompiler.cpp` (MODIFIED)).


---

### `Engine/Render/DX12/MeshPass.h` (NEW — replaces TrianglePass)


> **Code moved to Appendix A:** see Listing 122 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/MeshPass.h` (NEW — replaces TrianglePass)).


---

### `Engine/Render/DX12/MeshPass.cpp` (NEW)


> **Code moved to Appendix A:** see Listing 123 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/MeshPass.cpp` (NEW)).


---

### `Engine/Render/DX12/Dx12Renderer.h` (MODIFIED)


> **Code moved to Appendix A:** see Listing 124 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12Renderer.h` (MODIFIED)).


---

### `Engine/Render/DX12/Dx12Renderer.cpp` (MODIFIED)


> **Code moved to Appendix A:** see Listing 125 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12Renderer.cpp` (MODIFIED)).


---

### `Engine/Render/DX12/Dx12SwapChain.h` (MODIFIED — small additions)


> **Code moved to Appendix A:** see Listing 126 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12SwapChain.h` (MODIFIED — small additions)).


---

### `Engine/Render/DX12/Dx12SwapChain.cpp` (MODIFIED — provides helpers used by MeshPass)


> **Code moved to Appendix A:** see Listing 127 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12SwapChain.cpp` (MODIFIED — provides helpers used by MeshPass)).


---

### `Engine/Runtime/Engine.cpp` (MODIFIED — wire ResourceManager into RenderSystem)

> Keep everything else as-is; Phase 9.5 adds one call after render init.


> **Code moved to Appendix A:** see Listing 128 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Runtime/Engine.cpp` (MODIFIED — wire ResourceManager into RenderSystem)).


---

## 6) Verification checklist (Phase 9.5 done when…)

**Assets**

* [ ] You have `Data/Shaders/Basic.hlsl` (see below sample) and `Data/Meshes/triangle.nmsh` (binary in the `NMSH` format).
* [ ] Logs show resources become READY (shader text + mesh binary).

**GPU correctness**

* [ ] The triangle/mesh is drawn using **default heap** vertex/index buffers (not upload-only).
* [ ] Constant buffer updates do not stall and are aligned to 256 bytes.
* [ ] Descriptor heaps are created once and reused (no per-frame recreation).

**Synchronization & lifetime**

* [ ] No crash on shutdown; renderer waits for GPU then releases.
* [ ] No debug layer errors about:

  * resource state
  * descriptor heap binding
  * releasing in-use resources

**Debug layer**

* [ ] **0 D3D12 debug layer errors** while running and when closing the window.

---

## 7) Common pitfalls (Phase 9.5)

* **Destroying upload buffers too early**: staging resources must outlive the GPU copy. We solve this by deferred release tagged with the frame fence value that will be signaled.
* **CBV size not 256B aligned**: CBVs require 256-byte alignment; we enforce it in `GpuRingConstantBuffer` and CBV creation.
* **Forgetting to bind descriptor heaps before `SetGraphicsRootDescriptorTable`**: leads to debug errors or invalid bindings.
* **Using the wrong resource state**: default buffers must be transitioned from `COPY_DEST` to `VERTEX_AND_CONSTANT_BUFFER` / `INDEX_BUFFER`.
* **Per-frame heap recreation**: forbidden; we allocate from persistent heaps.

---

## 8) Next chat handoff (ONLY what you should say/bring next)

> “Phase 9.5 is implemented. We now have default-heap GPU buffers with upload staging, persistent CBV/SRV/UAV + sampler descriptor heaps with stable handles, a minimal root signature with descriptor tables, shader source loaded from VFS (TextResource) and compiled at runtime, a PSO cache, asset-backed mesh upload from Binary resource, and GPU-safe deferred destruction keyed off fence completion. The MeshPass draws indexed geometry with zero D3D12 debug errors. Start Phase 11 — Textures, SRVs, Samplers, and a Minimal Material System (VFS textures → GPU textures → SRV descriptors → per-material bindings).”

---

### Appendix: `Data/Shaders/Basic.hlsl` (you create this file)


> **Code moved to Appendix A:** see Listing 129 (from `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 8) Next chat handoff (ONLY what you should say/bring next) > Appendix: `Data/Shaders/Basic.hlsl` (you create this file)).


If you want, I can also give you a tiny C++ helper to **author** `triangle.nmsh` from a struct (so you don’t hand-roll the binary).

## Phase 10 — Scene Representation


* **World/scene ownership belongs to the runtime layer** (engine orchestrates systems; no global mutable state). (Gregory)
* **Scene representation needs stable object identity** (handles) and deterministic iteration for debugging. (Gregory)
* **Hierarchical transforms** should cache world transforms and use **dirty propagation** rather than recomputing everything every frame. (Gregory, Lengyel)
* **Visibility** is commonly done via **camera frustum culling against bounds** (AABB/sphere), producing a **renderable set** for the renderer. (Lengyel)
* **Renderer consumes immutable per-frame view + draw data** (separation of concerns / SRP-style separation). (Gregory)

Where this phase makes a choice not strictly mandated by the books, it is labeled explicitly as **Design choice (not directly from the book)**.

---

### What we implement now (tight scope)

* `World` under **Engine/Runtime/** with scene object ownership, stable handles, deterministic iteration.
* Transform hierarchy with cached world matrices + dirty propagation.
* Per-object bounds (AABB) and world bounds updates when transforms change.
* Camera + frustum planes + culling toggle.
* Runtime → Render handoff via a POD `RenderQueue` that contains:

  * camera view-projection
  * list of instances: `{ mesh ResourceHandle, world matrix }`
* Render changes: MeshPass becomes “instance-capable” by uploading an instance matrix buffer and drawing **N instances** of `triangle.nmsh`.
* NocturneHost demo scene: multiple objects, parent-child motion, culling on/off.

---

### Implementation steps (deterministic)

1. Add minimal math types: `Vec3`, `Quat`, `Mat4` (column-major, DX-style projection).
2. Add scene data types: `SceneObjectHandle`, `AABB`, `Frustum`, `Camera`.
3. Implement `World`:

   * handle table (index+generation), create/destroy
   * transform storage (arrays), parent/child via firstChild/nextSibling
   * dirty propagation down subtree
   * cached world matrix + world bounds update
   * renderable binding (mesh handle + local bounds)
   * build `RenderQueue` using **FrameArena** (no per-frame heap allocs)
4. Introduce `Engine/Render/RenderQueue.h` (POD handoff).
5. Update engine loop:

   * `Engine::Tick()` advances `World`
   * `Engine::EndFrame()` builds queue + passes to renderer (no Render→Runtime dependency)
6. Update DX12 renderer + MeshPass to consume `RenderQueue`:

   * per-frame instance matrix upload buffer (mapped once; reused)
   * SRV descriptor per frame to that buffer
   * `DrawIndexedInstanced(indexCount, instanceCount, ...)`
7. Update `Basic.hlsl` to use `SV_InstanceID` and matrix buffer.
8. Add NocturneHost test scene.

---

## FULL C++ IMPLEMENTATIONS (every new/modified file)

> Notes:
>
> * Public engine headers remain STL-free.
> * Frame allocations for render submission use `Engine::FrameArena()`.
> * All Render changes preserve: frame indexing model, fences, allocator/list lifetime rules, swap chain ownership, RenderSystem→Dx12Renderer orchestration, SRP separation.

---

### `Engine/Core/Math/MathTypes.h` (NEW)


> **Code moved to Appendix A:** see Listing 130 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Core/Math/MathTypes.h` (NEW)).


---

### `Engine/Runtime/Bounds.h` (NEW)


> **Code moved to Appendix A:** see Listing 131 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/Bounds.h` (NEW)).


---

### `Engine/Runtime/Frustum.h` (NEW)


> **Code moved to Appendix A:** see Listing 132 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/Frustum.h` (NEW)).


---

### `Engine/Runtime/SceneObject.h` (NEW)


> **Code moved to Appendix A:** see Listing 133 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/SceneObject.h` (NEW)).


---

### `Engine/Runtime/Camera.h` (NEW)


> **Code moved to Appendix A:** see Listing 134 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/Camera.h` (NEW)).


---

### `Engine/Render/RenderQueue.h` (NEW)


> **Code moved to Appendix A:** see Listing 135 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Render/RenderQueue.h` (NEW)).


---

### `Engine/Runtime/World.h` (NEW)


> **Code moved to Appendix A:** see Listing 136 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/World.h` (NEW)).


---

### `Engine/Runtime/World.cpp` (NEW)


> **Code moved to Appendix A:** see Listing 137 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/World.cpp` (NEW)).


---

### `Engine/Render/RenderSystem.h` (MODIFIED)


> **Code moved to Appendix A:** see Listing 138 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Render/RenderSystem.h` (MODIFIED)).


---

### `Engine/Render/RenderSystem.cpp` (MODIFIED)


> **Code moved to Appendix A:** see Listing 139 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Render/RenderSystem.cpp` (MODIFIED)).


---

### `Engine/Render/DX12/Dx12Renderer.h` (MODIFIED)


> **Code moved to Appendix A:** see Listing 140 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Render/DX12/Dx12Renderer.h` (MODIFIED)).


---

### `Engine/Render/DX12/Dx12Renderer.cpp` (MODIFIED)


> **Code moved to Appendix A:** see Listing 141 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Render/DX12/Dx12Renderer.cpp` (MODIFIED)).


---

### `Engine/Render/DX12/MeshPass.h` (MODIFIED)


> **Code moved to Appendix A:** see Listing 142 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Render/DX12/MeshPass.h` (MODIFIED)).


---

### `Engine/Render/DX12/MeshPass.cpp` (MODIFIED)


> **Code moved to Appendix A:** see Listing 143 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Render/DX12/MeshPass.cpp` (MODIFIED)).


---

### `Engine/Runtime/Engine.h` (MODIFIED: owns World)


> **Code moved to Appendix A:** see Listing 144 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/Engine.h` (MODIFIED: owns World)).


---

### `Engine/Runtime/Engine.cpp` (MODIFIED: tick world + render queue)


> **Code moved to Appendix A:** see Listing 145 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/Engine.cpp` (MODIFIED: tick world + render queue)).


---

### `Apps/NocturneHost/main.cpp` (MODIFIED: test scene)


> **Code moved to Appendix A:** see Listing 146 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Apps/NocturneHost/main.cpp` (MODIFIED: test scene)).


---

### `Data/Shaders/Basic.hlsl` (MODIFIED: instancing + viewProj + matrix buffer)


> **Code moved to Appendix A:** see Listing 147 (from `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Data/Shaders/Basic.hlsl` (MODIFIED: instancing + viewProj + matrix buffer)).


---

## Verification checklist

* [x] Engine boots with **0 D3D12 debug layer errors/warnings**.
* [x] World creates multiple objects; parent-child transform works (children move with parent).
* [x] Render shows **N instances** of the triangle at different transforms.
* [x] Scene tolerates mesh asset not ready yet:

  * objects exist
  * renderer simply draws fewer (or none) until ready; no stalls/crashes
* [x] Toggle culling off → far object becomes visible (or count increases).
* [x] Toggle culling on → far object culled; near objects remain.
* [x] No per-frame heap allocations for render submission (RenderQueue uses FrameArena).

---

## Common pitfalls (Phase 10)

* **Dirty propagation bugs**

  * Setting parent but not marking subtree dirty → world matrices/bounds stale.
  * Detach/attach child incorrectly → cycles or orphaned siblings.
* **Handle lifetime bugs**

  * Not bumping generation on destroy → use-after-free via stale handle.
* **Bounds not updated**

  * Culling “randomly hides” objects because world bounds never recomputed.
* **Frustum plane extraction mistakes**

  * Incorrect matrix convention leads to everything culled (or nothing culled).
* **Dependency violations**

  * Renderer querying Runtime objects directly (don’t; only consume `RenderQueue`).

---

### Next chat handoff (Phase 11 — Asset Import Pipeline)

Say this next:

> “Phase 10 is implemented: Runtime owns `World` with stable handles, transform hierarchy with dirty propagation and cached world matrices, AABB bounds updates, camera + frustum culling (toggleable), and a per-frame `RenderQueue` handoff allocated from FrameArena. Render consumes only the queue; MeshPass draws N instances of `triangle.nmsh` via instancing with zero D3D12 debug errors. Start Phase 11 — Asset Import Pipeline (importers, intermediate formats, metadata, dependency graph) per `nocturne_engine_architecture.md`.”



## Appendices

## Appendix A — Implementation Listings
> All implementation code blocks extracted from the phase documents are consolidated here.
### Listing 1 — `nocturne_engine_architecture.md` — Nocturne Engine — Architecture Overview > 2. High-Level Layering
```

Nocturne/
├── Engine/
│   ├── Core/
│   ├── Platform/
│   ├── Runtime/
│   ├── Resources/
│   ├── Render/
│   ├── Audio/
│   ├── Physics/
│   ├── Input/
│   └── Gameplay/
│
├── Game/
│   └── HorrorGame/
│
└── Tools/

```

### Listing 2 — `nocturne_engine_architecture.md` — Nocturne Engine — Architecture Overview > 3. Engine Layers > 3.3 Engine/Runtime
```

while (engineRunning)
{
processOSMessages();
pollInput();
serviceScheduledSystems();   // multi-rate stepping
renderStage();
}

```

### Listing 3 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 2. Final Folder Structure (After Phase 1)
```
Nocturne/
├── Engine/
│   ├── Core/
│   │   ├── Assert.h / .cpp
│   │   ├── BuildConfig.h
│   │   ├── Log.h / .cpp
│   │   ├── Clock.h / .cpp        // renamed from Time.h (CRT collision)
│   │   ├── Memory/
│   │   │   ├── Allocator.h / .cpp
│   │   │   ├── DebugAlloc.h / .cpp
│   │   │   └── LinearArena.h / .cpp
│   │   └── Subsystems/
│   │       ├── Subsystem.h
│   │       └── SubsystemRegistry.h / .cpp
│   │
│   ├── Platform/
│   │   └── Win32/
│   │       └── WinPlatform.h / .cpp
│   │
│   └── Runtime/
│       └── Engine.h / .cpp
│
├── Apps/
│   └── NocturneHost/
│       └── main.cpp
│
└── Docs/
    ├── Nocturne Engine Architecture.md
    └── Phase 1 — Core Systems.md
```

### Listing 4 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 3. Subsystem Model (Final) > 3.1 SubsystemDesc
```cpp
struct SubsystemDesc
{
    const char* name;
    std::span<const char* const> dependencies;

    bool (*startup)(void* ctx);
    void (*shutdown)(void* ctx);
};
```

### Listing 5 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 5. Logging System > Key Types
```cpp
enum class LogLevel { Trace, Debug, Info, Warn, Error, Fatal };
class Logger;
```

### Listing 6 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 5. Logging System > Usage
```cpp
NOC_LOG_INFO("Core", "Message %d", value);
```

### Listing 7 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 7. Clock / Time System > Key Classes
```cpp
class HiResClock;
class TimeSystem;
```

### Listing 8 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 9. Engine Class (Runtime) > Public Accessors
```cpp
IAllocator& Allocator();
LinearArena& FrameArena();
```

### Listing 9 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 9. Engine Class (Runtime) > Internal Lifecycle
```cpp
bool InitMemory();
void ShutdownMemory();
```

### Listing 10 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 11. Negative Test Results (Verified) > Missing Dependency
```
[ERROR][Core] Missing dependency 'DoesNotExist' required by 'Memory'
```

### Listing 11 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 11. Negative Test Results (Verified) > Cycle Detection
```
[ERROR][Core] Subsystem dependency cycle detected:
  -> Log
  -> Memory
  -> Log
```

### Listing 12 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/BuildConfig.h`
```cpp
#pragma once

// Exactly one must be defined by the build system
// NOC_DEBUG
// NOC_DEV
// NOC_SHIP

#if defined(NOC_DEBUG)
    #define NOC_ENABLE_ASSERTS 1
    #define NOC_ENABLE_LOGGING 1
#elif defined(NOC_DEV)
    #define NOC_ENABLE_ASSERTS 1
    #define NOC_ENABLE_LOGGING 1
#elif defined(NOC_SHIP)
    #define NOC_ENABLE_ASSERTS 0
    #define NOC_ENABLE_LOGGING 0
#else
    #error "No build configuration defined"
#endif
```

### Listing 13 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Assert.h`
```cpp
#pragma once
#include "BuildConfig.h"

namespace noc {

    using AssertFailHandler = void(*)(const char* expr,
        const char* file,
        int line,
        const char* msg);

    void SetAssertFailHandler(AssertFailHandler handler);
    void DefaultAssertFailHandler(const char* expr,
        const char* file,
        int line,
        const char* msg);

} // namespace noc

#if NOC_ENABLE_ASSERTS

#if defined(_MSC_VER)
#define NOC_DEBUG_BREAK() __debugbreak()
#else
#define NOC_DEBUG_BREAK() ((void)0)
#endif

#define NOC_ASSERT(expr) \
        do { \
            if (!(expr)) { \
                ::noc::DefaultAssertFailHandler(#expr, __FILE__, __LINE__, nullptr); \
                NOC_DEBUG_BREAK(); \
            } \
        } while (0)

#define NOC_ASSERT_MSG(expr, msg) \
        do { \
            if (!(expr)) { \
                ::noc::DefaultAssertFailHandler(#expr, __FILE__, __LINE__, (msg)); \
                NOC_DEBUG_BREAK(); \
            } \
        } while (0)

#else

#define NOC_ASSERT(expr)        do { (void)sizeof(expr); } while (0)
#define NOC_ASSERT_MSG(expr, msg) do { (void)sizeof(expr); (void)(msg); } while (0)

#endif

// Design choice: VERIFY evaluates in all builds
#define NOC_VERIFY(expr) \
    do { \
        if (!(expr)) { \
            NOC_ASSERT(expr); \
        } \
    } while (0)

```

### Listing 14 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Assert.cpp`
```cpp
#include "Assert.h"
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace noc {

    static AssertFailHandler g_handler = nullptr;

    void SetAssertFailHandler(AssertFailHandler handler)
    {
        g_handler = handler;
    }

    void DefaultAssertFailHandler(const char* expr,
        const char* file,
        int line,
        const char* msg)
    {
        if (g_handler)
        {
            g_handler(expr, file, line, msg);
            return;
        }

        char buffer[1024];
        if (msg)
            std::snprintf(buffer, sizeof(buffer), "ASSERT FAILED: %s\n%s(%d)\nMSG: %s\n", expr, file, line, msg);
        else
            std::snprintf(buffer, sizeof(buffer), "ASSERT FAILED: %s\n%s(%d)\n", expr, file, line);

#if defined(_WIN32)
        OutputDebugStringA(buffer);
#endif

        std::fputs(buffer, stderr);
        std::fflush(stderr);

        // In case no debugger is attached and execution continues, fail hard.
        std::abort();
    }

} // namespace noc
```

### Listing 15 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Log.h`
```cpp
#pragma once
#include "BuildConfig.h"
#include <cstdint>

namespace noc {

    enum class LogLevel : uint8_t
    {
        Trace, Debug, Info, Warn, Error, Fatal
    };

    struct LogMessage
    {
        LogLevel level;
        const char* channel;
        const char* text; // formatted final text (Phase 1: transient)
    };

    class Logger
    {
    public:
        void Init();
        void Shutdown();

        void Write(LogLevel level, const char* channel, const char* fmt, ...);
    };

    Logger& GetLogger();

} // namespace noc

#if NOC_ENABLE_LOGGING

#define NOC_LOG_TRACE(ch, fmt, ...) ::noc::GetLogger().Write(::noc::LogLevel::Trace, (ch), (fmt), __VA_ARGS__)
#define NOC_LOG_DEBUG(ch, fmt, ...) ::noc::GetLogger().Write(::noc::LogLevel::Debug, (ch), (fmt), __VA_ARGS__)
#define NOC_LOG_INFO(ch, fmt, ...)  ::noc::GetLogger().Write(::noc::LogLevel::Info,  (ch), (fmt), __VA_ARGS__)
#define NOC_LOG_WARN(ch, fmt, ...)  ::noc::GetLogger().Write(::noc::LogLevel::Warn,  (ch), (fmt), __VA_ARGS__)
#define NOC_LOG_ERROR(ch, fmt, ...) ::noc::GetLogger().Write(::noc::LogLevel::Error, (ch), (fmt), __VA_ARGS__)
#define NOC_LOG_FATAL(ch, fmt, ...) ::noc::GetLogger().Write(::noc::LogLevel::Fatal, (ch), (fmt), __VA_ARGS__)

#else

#define NOC_LOG_TRACE(...) do{}while(0)
#define NOC_LOG_DEBUG(...) do{}while(0)
#define NOC_LOG_INFO(...)  do{}while(0)
#define NOC_LOG_WARN(...)  do{}while(0)
#define NOC_LOG_ERROR(...) do{}while(0)
#define NOC_LOG_FATAL(...) do{}while(0)

#endif

```

### Listing 16 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Log.cpp`
```cpp
#include "Log.h"
#include <cstdarg>
#include <cstdio>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace noc {

    static Logger g_logger;

    Logger& GetLogger()
    {
        return g_logger;
    }

    void Logger::Init()
    {
        // Phase 1: nothing heavy
    }

    void Logger::Shutdown()
    {
        // Phase 1: nothing heavy
    }

    static const char* ToString(LogLevel lvl)
    {
        switch (lvl)
        {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        default:              return "LOG";
        }
    }

    void Logger::Write(LogLevel level, const char* channel, const char* fmt, ...)
    {
        char msg[1024];

        va_list args;
        va_start(args, fmt);
        std::vsnprintf(msg, sizeof(msg), fmt, args);
        va_end(args);

        char line[1200];
        std::snprintf(line, sizeof(line), "[%s][%s] %s\n", ToString(level), channel ? channel : "General", msg);

#if defined(_WIN32)
        OutputDebugStringA(line);
#endif

        std::fputs(line, stdout);
        std::fflush(stdout);
    }

} // namespace noc
```

### Listing 17 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Clock.h`
```cpp
#pragma once
#include <cstdint>

namespace noc {

    class HiResClock
    {
    public:
        void Init();
        uint64_t NowTicks() const;
        uint64_t Frequency() const { return freq_; }
        double   TicksToSeconds(uint64_t dtTicks) const;

    private:
        uint64_t freq_ = 0;
    };

    class TimeSystem
    {
    public:
        void Init();

        // Call once per frame
        void BeginFrame();
        void EndFrame();

        float  DeltaSeconds() const { return dtSeconds_; }
        double SecondsSinceStart() const;

        void SetMaxDeltaSeconds(float maxDt) { maxDtSeconds_ = maxDt; }
        void SetBreakpointFallbackDelta(float dt) { fallbackDtSeconds_ = dt; }

    private:
        HiResClock clock_;
        uint64_t startTicks_ = 0;
        uint64_t frameBeginTicks_ = 0;
        uint64_t lastFrameEndTicks_ = 0;

        float dtSeconds_ = 0.0f;

        // Gregory-style breakpoint protection:
        float maxDtSeconds_ = 1.0f;        // if dt > this, assume breakpoint pause
        float fallbackDtSeconds_ = 1.0f / 60.0f;
    };

    TimeSystem& GetTime();

} // namespace noc
```

### Listing 18 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Clock.cpp`
```cpp
#include "Clock.h"
#include "Assert.h"
#include "Platform/Win32/WinPlatform.h"

namespace noc {

    static TimeSystem g_time;

    TimeSystem& GetTime()
    {
        return g_time;
    }

    // ----- HiResClock -----

    void HiResClock::Init()
    {
        freq_ = platform::QueryHiResFrequency();
        NOC_ASSERT(freq_ != 0);
    }

    uint64_t HiResClock::NowTicks() const
    {
        return platform::QueryHiResCounter();
    }

    double HiResClock::TicksToSeconds(uint64_t dtTicks) const
    {
        return static_cast<double>(dtTicks) / static_cast<double>(freq_);
    }

    // ----- TimeSystem -----

    void TimeSystem::Init()
    {
        clock_.Init();
        startTicks_ = clock_.NowTicks();
        frameBeginTicks_ = startTicks_;
        lastFrameEndTicks_ = startTicks_;

        dtSeconds_ = fallbackDtSeconds_;
    }

    void TimeSystem::BeginFrame()
    {
        frameBeginTicks_ = clock_.NowTicks();
    }

    void TimeSystem::EndFrame()
    {
        const uint64_t endTicks = clock_.NowTicks();
        const uint64_t dtTicks = endTicks - lastFrameEndTicks_;

        double dt = clock_.TicksToSeconds(dtTicks);

        // Clamp huge spikes (debugger breakpoints, stalls)
        if (dt > static_cast<double>(maxDtSeconds_))
            dt = static_cast<double>(fallbackDtSeconds_);

        dtSeconds_ = static_cast<float>(dt);
        lastFrameEndTicks_ = endTicks;
    }

    double TimeSystem::SecondsSinceStart() const
    {
        const uint64_t now = clock_.NowTicks();
        const uint64_t dtTicks = now - startTicks_;
        return clock_.TicksToSeconds(dtTicks);
    }

} // namespace noc
```

### Listing 19 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Apps/NocturneHost/main.cpp`
```cpp
#include "Runtime/Engine.h"

int main()
{
    noc::Engine engine;

    if (!engine.Init())
        return -1;

    // Phase 1: single tick just to validate systems
    engine.TickOnce();

    engine.Shutdown();
    return 0;
}
```

### Listing 20 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Platform/Win32/WinPlatform.h`
```cpp
#pragma once
#include <cstdint>

namespace noc::platform {

uint64_t QueryHiResCounter();
uint64_t QueryHiResFrequency();

} // namespace noc::platform
```

### Listing 21 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Platform/Win32/WinPlatform.cpp`
```cpp
#include "WinPlatform.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace noc::platform {

uint64_t QueryHiResCounter()
{
    LARGE_INTEGER v;
    ::QueryPerformanceCounter(&v);
    return static_cast<uint64_t>(v.QuadPart);
}

uint64_t QueryHiResFrequency()
{
    LARGE_INTEGER v;
    ::QueryPerformanceFrequency(&v);
    return static_cast<uint64_t>(v.QuadPart);
}

} // namespace noc::platform
```

### Listing 22 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Memory/Allocator.h`
```cpp
#pragma once
#include <cstddef>

namespace noc {

class IAllocator
{
public:
    virtual ~IAllocator() = default;
    virtual void* Allocate(std::size_t size, std::size_t alignment) = 0;
    virtual void  Deallocate(void* p) = 0;
};

class MallocAllocator final : public IAllocator
{
public:
    void* Allocate(std::size_t size, std::size_t alignment) override;
    void  Deallocate(void* p) override;
};

inline void* Alloc(IAllocator& a, std::size_t size, std::size_t alignment = alignof(std::max_align_t))
{
    return a.Allocate(size, alignment);
}

inline void Free(IAllocator& a, void* p)
{
    a.Deallocate(p);
}

} // namespace noc
```

### Listing 23 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Memory/Allocator.cpp`
```cpp
#include "Allocator.h"
#include "../Assert.h"

#include <new>
#include <cstdlib>

namespace noc {

    void* MallocAllocator::Allocate(std::size_t size, std::size_t alignment)
    {
        NOC_ASSERT(size > 0);
        NOC_ASSERT((alignment & (alignment - 1)) == 0); // power of two

#if defined(_MSC_VER)
        void* p = _aligned_malloc(size, alignment);
        NOC_ASSERT(p != nullptr);
        return p;
#else
        // Fallback (shouldn't be hit on Windows/MSVC)
        void* p = ::operator new(size, std::align_val_t(alignment));
        return p;
#endif
    }

    void MallocAllocator::Deallocate(void* p)
    {
        if (!p) return;

#if defined(_MSC_VER)
        _aligned_free(p);
#else
        ::operator delete(p);
#endif
    }

} // namespace noc

```

### Listing 24 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Memory/LinearArena.h`
```cpp
#pragma once
#include <cstddef>
#include <cstdint>

namespace noc {

class LinearArena
{
public:
    void Init(void* backingMemory, std::size_t bytes);
    void Reset();

    void* Allocate(std::size_t size, std::size_t alignment);

    std::size_t Capacity() const { return capacity_; }
    std::size_t Used() const { return offset_; }

private:
    std::byte* base_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t offset_ = 0;
};

} // namespace noc
```

### Listing 25 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Memory/LinearArena.cpp`
```cpp
#include "LinearArena.h"
#include "../Assert.h"

namespace noc {

static std::size_t AlignUp(std::size_t value, std::size_t alignment)
{
    const std::size_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

void LinearArena::Init(void* backingMemory, std::size_t bytes)
{
    NOC_ASSERT(backingMemory != nullptr);
    NOC_ASSERT(bytes > 0);

    base_ = static_cast<std::byte*>(backingMemory);
    capacity_ = bytes;
    offset_ = 0;
}

void LinearArena::Reset()
{
    offset_ = 0;
}

void* LinearArena::Allocate(std::size_t size, std::size_t alignment)
{
    NOC_ASSERT(base_ != nullptr);
    NOC_ASSERT(size > 0);
    NOC_ASSERT((alignment & (alignment - 1)) == 0);

    const std::size_t alignedOffset = AlignUp(offset_, alignment);
    const std::size_t end = alignedOffset + size;

    NOC_ASSERT(end <= capacity_);

    void* p = base_ + alignedOffset;
    offset_ = end;
    return p;
}

} // namespace noc
```

### Listing 26 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Memory/DebugAlloc.h`
```cpp
#pragma once
#include "Allocator.h"

#include <atomic>
#include <cstddef>

namespace noc {

class DebugAlloc final : public IAllocator
{
public:
    explicit DebugAlloc(IAllocator& backing);

    void* Allocate(std::size_t size, std::size_t alignment) override;
    void  Deallocate(void* p) override;

    std::size_t TotalAllocatedBytes() const { return totalAllocated_.load(); }
    std::size_t OutstandingBytes() const { return outstanding_.load(); }
    std::size_t AllocationCount() const { return allocCount_.load(); }

private:
    IAllocator& backing_;

    std::atomic<std::size_t> totalAllocated_{0};
    std::atomic<std::size_t> outstanding_{0};
    std::atomic<std::size_t> allocCount_{0};
};

} // namespace noc
```

### Listing 27 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Memory/DebugAlloc.cpp`
```cpp
#include "DebugAlloc.h"
#include "../Assert.h"

#include <cstdint>

namespace noc {

    // Header placed immediately before the user pointer (at userPtr - sizeof(Header)).
    struct AllocHeader
    {
        void* raw;          // pointer returned by backing allocator
        std::size_t size;   // requested user size
    };

    DebugAlloc::DebugAlloc(IAllocator& backing)
        : backing_(backing)
    {
    }

    void* DebugAlloc::Allocate(std::size_t size, std::size_t alignment)
    {
        NOC_ASSERT(size > 0);
        NOC_ASSERT((alignment & (alignment - 1)) == 0);

        const std::size_t headerSize = sizeof(AllocHeader);

        // Reserve enough space so we can:
        // 1) keep 'raw' somewhere
        // 2) align the returned user pointer to 'alignment'
        const std::size_t total = size + headerSize + alignment;

        void* raw = backing_.Allocate(total, alignof(std::max_align_t));
        NOC_ASSERT(raw != nullptr);

        std::uintptr_t rawAddr = reinterpret_cast<std::uintptr_t>(raw);
        std::uintptr_t userAddr = rawAddr + headerSize;

        const std::uintptr_t mask = static_cast<std::uintptr_t>(alignment - 1);
        userAddr = (userAddr + mask) & ~mask;

        auto* header = reinterpret_cast<AllocHeader*>(userAddr - headerSize);
        header->raw = raw;
        header->size = size;

        totalAllocated_.fetch_add(size);
        outstanding_.fetch_add(size);
        allocCount_.fetch_add(1);

        return reinterpret_cast<void*>(userAddr);
    }

    void DebugAlloc::Deallocate(void* p)
    {
        if (!p) return;

        const std::size_t headerSize = sizeof(AllocHeader);
        std::uintptr_t userAddr = reinterpret_cast<std::uintptr_t>(p);

        auto* header = reinterpret_cast<AllocHeader*>(userAddr - headerSize);
        outstanding_.fetch_sub(header->size);

        backing_.Deallocate(header->raw);
    }

} // namespace noc
```

### Listing 28 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Subsystems/Subsystem.h`
```cpp
#pragma once
#include <span>

namespace noc {

using SubsystemStartupFn  = bool (*)(void* ctx);
using SubsystemShutdownFn = void (*)(void* ctx);

struct SubsystemDesc
{
    const char* name;
    std::span<const char* const> dependencies;

    SubsystemStartupFn  startup;
    SubsystemShutdownFn shutdown;
};

} // namespace noc
```

### Listing 29 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Subsystems/SubsystemRegistry.h`
```cpp
#pragma once
#include "Subsystem.h"

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace noc {

class SubsystemRegistry
{
public:
    void Register(const SubsystemDesc& desc);

    bool StartupAll(void* ctx);
    void ShutdownAll(void* ctx);

private:
    enum class VisitState : uint8_t { Unvisited, Visiting, Visited };

    bool BuildStartupOrder();

    bool Visit(SubsystemDesc* s,
               std::unordered_map<std::string_view, VisitState>& state,
               std::vector<std::string_view>& stack);

    SubsystemDesc* Find(std::string_view name);

private:
    std::vector<SubsystemDesc> subsystems_;
    std::vector<SubsystemDesc*> startupOrder_;
};

} // namespace noc
```

### Listing 30 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Core/Subsystems/SubsystemRegistry.cpp`
```cpp
#include "SubsystemRegistry.h"
#include "../Log.h"
#include "../Assert.h"

#include <algorithm>
#include <string>

namespace noc {

    void SubsystemRegistry::Register(const SubsystemDesc& desc)
    {
        // Detect duplicates early
        for (const auto& s : subsystems_)
        {
            if (std::string_view{ s.name } == std::string_view{ desc.name })
            {
                NOC_LOG_ERROR("Core", "Duplicate subsystem name: %s", desc.name);
                NOC_ASSERT_MSG(false, "Duplicate subsystem name");
            }
        }

        subsystems_.push_back(desc);
    }

    SubsystemDesc* SubsystemRegistry::Find(std::string_view name)
    {
        for (auto& s : subsystems_)
        {
            if (std::string_view{ s.name } == name)
                return &s;
        }
        return nullptr;
    }

    bool SubsystemRegistry::Visit(SubsystemDesc* s,
        std::unordered_map<std::string_view, VisitState>& state,
        std::vector<std::string_view>& stack)
    {
        const std::string_view name{ s->name };
        auto it = state.find(name);
        if (it != state.end())
        {
            if (it->second == VisitState::Visiting)
            {
                // Cycle detected: print the stack
                NOC_LOG_ERROR("Core", "Subsystem dependency cycle detected:");
                for (auto sv : stack)
                    NOC_LOG_ERROR("Core", "  -> %.*s", (int)sv.size(), sv.data());
                NOC_LOG_ERROR("Core", "  -> %.*s", (int)name.size(), name.data());
                return false;
            }
            if (it->second == VisitState::Visited)
                return true;
        }

        state[name] = VisitState::Visiting;
        stack.push_back(name);

        // Visit dependencies first
        for (const char* depCStr : s->dependencies)
        {
            const std::string_view dep{ depCStr };
            SubsystemDesc* d = Find(dep);
            if (!d)
            {
                NOC_LOG_ERROR("Core", "Missing dependency '%.*s' required by '%.*s'",
                    (int)dep.size(), dep.data(),
                    (int)name.size(), name.data());
                return false;
            }

            if (!Visit(d, state, stack))
                return false;
        }

        // Done exploring
        stack.pop_back();
        state[name] = VisitState::Visited;

        // Add to order after deps (post-order)
        startupOrder_.push_back(s);
        return true;
    }

    bool SubsystemRegistry::BuildStartupOrder()
    {
        startupOrder_.clear();
        startupOrder_.reserve(subsystems_.size());

        std::unordered_map<std::string_view, VisitState> state;
        state.reserve(subsystems_.size());

        std::vector<std::string_view> stack;
        stack.reserve(subsystems_.size());

        // Visit all subsystems
        for (auto& s : subsystems_)
        {
            const std::string_view name{ s.name };
            if (state[name] == VisitState::Visited)
                continue;

            if (!Visit(&s, state, stack))
                return false;
        }

        // startupOrder_ currently has each node appended after its deps
        // This produces a valid topological order already.

        return true;
    }

    bool SubsystemRegistry::StartupAll(void* ctx)
    {
        if (!BuildStartupOrder())
            return false;

        // Start in sorted order
        for (SubsystemDesc* s : startupOrder_)
        {
            if (s->startup && !s->startup(ctx))
            {
                NOC_LOG_ERROR("Core", "Subsystem startup failed: %s", s->name);

                // Shutdown those already started (reverse)
                ShutdownAll(ctx);
                return false;
            }
        }

        return true;
    }

    void SubsystemRegistry::ShutdownAll(void* ctx)
    {
        // shutdown in reverse startup order
        for (auto it = startupOrder_.rbegin(); it != startupOrder_.rend(); ++it)
        {
            if ((*it)->shutdown)
                (*it)->shutdown(ctx);
        }
        startupOrder_.clear();
    }

} // namespace noc
```

### Listing 31 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Runtime/Engine.h`
```cpp
#pragma once
#include "Core/BuildConfig.h"
#include "Core/Subsystems/SubsystemRegistry.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/LinearArena.h"

#if NOC_ENABLE_ASSERTS
#include "Core/Memory/DebugAlloc.h"
#endif

namespace noc {

    class Engine
    {
    public:
        bool Init();
        void TickOnce();
        void Shutdown();

        IAllocator& Allocator();
        LinearArena& FrameArena();

        // Internal lifecycle used by subsystems (keeps members private)
        bool InitMemory();
        void KillMemory();

    private:
        SubsystemRegistry registry_;

        MallocAllocator baseAlloc_;

#if NOC_ENABLE_ASSERTS
        DebugAlloc debugAlloc_{ baseAlloc_ };
        IAllocator* alloc_ = &debugAlloc_;
#else
        IAllocator* alloc_ = &baseAlloc_;
#endif

        void* frameArenaMem_ = nullptr;
        LinearArena frameArena_;
    };

} // namespace noc
```

### Listing 32 — `phase_1_core_systems.md` — Phase 1 — Core Systems (Final, Updated) > 14. Implementations > `Engine/Runtime/Engine.cpp`
```cpp
#include "Engine.h"
#include "Core/Log.h"
#include "Core/Assert.h"
#include "Core/Clock.h"


namespace noc {

	IAllocator& Engine::Allocator() { return *alloc_; }
	LinearArena& Engine::FrameArena() { return frameArena_; }

	bool Engine::InitMemory()
	{
		constexpr std::size_t kFrameArenaBytes = 8 * 1024 * 1024; // Design choice
		frameArenaMem_ = Allocator().Allocate(kFrameArenaBytes, 64);
		frameArena_.Init(frameArenaMem_, kFrameArenaBytes);

		NOC_LOG_INFO("Core", "Memory system initialized (frame arena=%zu bytes)", kFrameArenaBytes);
		return true;
	}

	void Engine::KillMemory()
	{
		// Free frame arena first
		if (frameArenaMem_)
		{
			Allocator().Deallocate(frameArenaMem_);
			frameArenaMem_ = nullptr;
		}

#if NOC_ENABLE_ASSERTS
		NOC_LOG_INFO("Core", "Memory stats: total=%zu outstanding=%zu allocs=%zu",
			debugAlloc_.TotalAllocatedBytes(),
			debugAlloc_.OutstandingBytes(),
			debugAlloc_.AllocationCount());
#endif
	}


	static bool StartupLog(void*)
	{
		noc::GetLogger().Init();
		NOC_LOG_INFO("Core", "Logger initialized");
		return true;
	}

	static void ShutdownLog(void*)
	{
		NOC_LOG_INFO("Core", "Logger shutting down");
		noc::GetLogger().Shutdown();
	}

	static bool StartupTime(void*)
	{
		noc::GetTime().Init();
		NOC_LOG_INFO("Core", "Time system initialized");
		return true;
	}

	static void ShutdownTime(void*)
	{
		NOC_LOG_INFO("Core", "Time system shutting down");
	}

	static bool StartupAssert(void*)
	{
		NOC_LOG_INFO("Core", "Assert system initialized");
		return true;
	}

	static void ShutdownAssert(void*)
	{
		NOC_LOG_INFO("Core", "Assert system shutting down");
	}

	static bool StartupMemory(void* ctx)
	{
		auto* e = static_cast<noc::Engine*>(ctx);
		return e->InitMemory();
	}

	static void ShutdownMemory(void* ctx)
	{
		NOC_LOG_INFO("Core", "Memory system shutting down");
		auto* e = static_cast<noc::Engine*>(ctx);
		e->KillMemory();
	}


	bool Engine::Init()
	{
		std::span<const char* const> depsLog{}; // empty: no dependencies

		static const char* kDepsNeedLog[] = { "Log" };
		std::span<const char* const> depsNeedLog{ kDepsNeedLog, 1 };

		registry_.Register(SubsystemDesc{ "Log",    depsLog,     &StartupLog,    &ShutdownLog });
		registry_.Register(SubsystemDesc{ "Time",   depsNeedLog, &StartupTime,   &ShutdownTime });
		registry_.Register(SubsystemDesc{ "Memory", depsNeedLog, &StartupMemory, &ShutdownMemory });
		registry_.Register(SubsystemDesc{ "Assert", depsNeedLog, &StartupAssert, &ShutdownAssert });


		return registry_.StartupAll(this);
	}


	void Engine::TickOnce()
	{
		GetTime().BeginFrame();

		FrameArena().Reset();

		// Allocate a few blocks
		void* a = FrameArena().Allocate(256, 16);
		void* b = FrameArena().Allocate(1024, 64);
		(void)a; (void)b;

		GetTime().EndFrame();

		NOC_LOG_INFO("Core", "TickOnce() dt=%.6f sec arenaUsed=%zu bytes",
			GetTime().DeltaSeconds(),
			FrameArena().Used());

	}


	void Engine::Shutdown()
	{
		registry_.ShutdownAll(this);
	}

} // namespace noc
```

### Listing 33 — `Phase 2 - Window & Main Loop.md` — 4. Updated Folder Structure (After Phase 2)
```
Nocturne/
├── Engine/
│   ├── Core/                          // unchanged from Phase 1
│   │   ├── Assert.h
│   │   ├── Assert.cpp
│   │   ├── BuildConfig.h
│   │   ├── Log.h
│   │   ├── Log.cpp
│   │   ├── Clock.h
│   │   ├── Clock.cpp
│   │   ├── Memory/
│   │   │   ├── Allocator.h
│   │   │   ├── Allocator.cpp
│   │   │   ├── DebugAlloc.h
│   │   │   ├── DebugAlloc.cpp
│   │   │   ├── LinearArena.h
│   │   │   └── LinearArena.cpp
│   │   └── Subsystems/
│   │       ├── Subsystem.h
│   │       └── SubsystemRegistry.h / .cpp
│   │
│   ├── Platform/
│   │   └── Win32/
│   │       ├── WinPlatform.h
│   │       ├── WinPlatform.cpp
│   │       ├── WinWindow.h              // NEW (Phase 2)
│   │       └── WinWindow.cpp            // NEW (Phase 2)
│   │
│   └── Runtime/
│       ├── Engine.h
│       ├── Engine.cpp                   // UPDATED (Phase 2)
│       ├── MainLoop.h                   // NEW (Phase 2)
│       └── MainLoop.cpp                 // NEW (Phase 2)
│
├── Apps/
│   └── NocturneHost/
│       ├── main.cpp                     // UPDATED (Phase 2)
│       └── AppConfig.h                  // NEW (Phase 2, window params)
│
└── Docs/
    ├── Nocturne Engine Architecture.md
    ├── Phase 1 — Core Systems.md
    └── Phase 2 — Window & Main Loop.md
```

### Listing 34 — `Phase 2 - Window & Main Loop.md` — 5. New Subsystems Introduced > 5.2 Main Loop (Runtime-Owned)
```cpp
while (engineRunning)
{
    PumpOSMessages();   // non-blocking
    BeginFrame();       // TimeSystem::BeginFrame
    Tick();             // empty for now
    EndFrame();         // TimeSystem::EndFrame
}
```

### Listing 35 — `Phase 2 - Window & Main Loop.md` — 7. Message Pump Design > Non-Blocking Rule (Mandatory)
```cpp
while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
{
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}
```

### Listing 36 — `Phase 2 - Window & Main Loop.md` — 9. Relationships & Dependency Graph (Phase 2)
```
Win32 OS
   ↓
Platform/WinWindow
   ↓
Runtime/MainLoop
   ↓
Runtime/Engine
   ↓
Core (Time, Log, Memory, Assert)
```

### Listing 37 — `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Apps/NocturneHost/AppConfig.h`
```cpp
#pragma once

namespace noc::app
{
    struct AppConfig
    {
        const wchar_t* windowTitle = L"NocturneHost";
        int windowWidth = 1280;
        int windowHeight = 720;
        bool resizable = true;
    };
}
```

### Listing 38 — `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Apps/NocturneHost/main.cpp`
```cpp
#include "Engine/Runtime/Engine.h"

int main()
{
    noc::Engine engine;
    if (!engine.Init())
        return -1;

    const int rc = engine.Run();
    engine.Shutdown();
    return rc;
}
```

### Listing 39 — `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Engine/Platform/Win32/WinWindow.h`
```cpp
#pragma once

#include <windows.h>

namespace noc
{
    struct WinWindowDesc
    {
        const wchar_t* title = L"Nocturne";
        int width = 1280;
        int height = 720;
        bool resizable = true;
    };

    class WinWindow
    {
    public:
        bool Create(const WinWindowDesc& desc);
        void Destroy();

        HWND Handle() const { return hwnd_; }

        bool ShouldQuit() const { return shouldQuit_; }
        void RequestQuit() { shouldQuit_ = true; }

    private:
        static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        bool RegisterClassOnce();
        void ApplyClientSize(int clientW, int clientH, bool resizable);

    private:
        HWND hwnd_ = nullptr;
        bool classRegistered_ = false;
        bool shouldQuit_ = false;
    };
} // namespace noc
```

### Listing 40 — `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Engine/Platform/Win32/WinWindow.cpp`
```cpp
#include "WinWindow.h"

#include "Engine/Core/Log.h"
#include "Engine/Core/Assert.h"

namespace noc
{
    static const wchar_t* kWndClassName = L"NocturneWindowClass";

    bool WinWindow::RegisterClassOnce()
    {
        if (classRegistered_)
            return true;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &WinWindow::WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kWndClassName;

        if (!RegisterClassExW(&wc))
        {
            NOC_LOG_ERROR("Win32", "RegisterClassExW failed (err=%lu)", GetLastError());
            return false;
        }

        classRegistered_ = true;
        return true;
    }

    void WinWindow::ApplyClientSize(int clientW, int clientH, bool resizable)
    {
        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        if (resizable)
            style |= WS_THICKFRAME | WS_MAXIMIZEBOX;

        RECT r{ 0, 0, clientW, clientH };
        AdjustWindowRect(&r, style, FALSE);

        const int winW = r.right - r.left;
        const int winH = r.bottom - r.top;

        SetWindowLongPtrW(hwnd_, GWL_STYLE, (LONG_PTR)style);
        SetWindowPos(hwnd_, nullptr, 100, 100, winW, winH, SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    bool WinWindow::Create(const WinWindowDesc& desc)
    {
        if (!RegisterClassOnce())
            return false;

        HINSTANCE hInst = GetModuleHandleW(nullptr);

        // Create with a temporary style. We'll correct it after AdjustWindowRect.
        DWORD style = WS_OVERLAPPEDWINDOW;

        hwnd_ = CreateWindowExW(
            0,
            kWndClassName,
            desc.title,
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            desc.width, desc.height,
            nullptr, nullptr,
            hInst,
            this // pass pointer for association
        );

        if (!hwnd_)
        {
            NOC_LOG_ERROR("Win32", "CreateWindowExW failed (err=%lu)", GetLastError());
            return false;
        }

        ApplyClientSize(desc.width, desc.height, desc.resizable);

        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);

        NOC_LOG_INFO("Win32", "Window created: %dx%d", desc.width, desc.height);
        return true;
    }

    void WinWindow::Destroy()
    {
        if (hwnd_)
        {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
    }

    LRESULT CALLBACK WinWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // Associate the WinWindow* with the HWND on WM_NCCREATE.
        if (msg == WM_NCCREATE)
        {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* win = reinterpret_cast<WinWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(win));
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }

        auto* win = reinterpret_cast<WinWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

        switch (msg)
        {
        case WM_CLOSE:
            if (win) win->RequestQuit();
            return 0;

        case WM_DESTROY:
            if (win) win->RequestQuit();
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }
    }
} // namespace noc
```

### Listing 41 — `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Engine/Runtime/MainLoop.h`
```cpp
#pragma once

namespace noc
{
    class Engine;
    class WinWindow;

    class MainLoop
    {
    public:
        void Run(Engine& engine, WinWindow& window);

    private:
        void PumpMessagesNonBlocking(WinWindow& window);
    };
} // namespace noc
```

### Listing 42 — `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Engine/Runtime/MainLoop.cpp`
```cpp
#include "MainLoop.h"

#include <windows.h>

#include "Engine/Runtime/Engine.h"
#include "Engine/Platform/Win32/WinWindow.h"
#include "Engine/Core/Log.h"

namespace noc
{
    void MainLoop::PumpMessagesNonBlocking(WinWindow& window)
    {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            // Defensive: respect WM_QUIT too (e.g., PostQuitMessage)
            if (msg.message == WM_QUIT)
            {
                window.RequestQuit();
                return;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void MainLoop::Run(Engine& engine, WinWindow& window)
    {
        NOC_LOG_INFO("Runtime", "MainLoop starting");

        while (!window.ShouldQuit())
        {
            PumpMessagesNonBlocking(window);

            engine.BeginFrame();
            engine.Tick();      // intentionally empty in Phase 2
            engine.EndFrame();
        }

        NOC_LOG_INFO("Runtime", "MainLoop exiting");
    }
} // namespace noc
```

### Listing 43 — `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Engine/Runtime/Engine.h`
```cpp
#pragma once
#include "Core/BuildConfig.h"
#include "Core/Subsystems/SubsystemRegistry.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/LinearArena.h"

#if NOC_ENABLE_ASSERTS
#include "Core/Memory/DebugAlloc.h"
#endif

namespace noc {

    class WinWindow;
    class MainLoop;

    class Engine
    {
    public:
        bool Init();
        void TickOnce();
        void Shutdown();

        int Run();                 // creates window + runs loop
        void BeginFrame();
        void Tick();
        void EndFrame();

        IAllocator& Allocator();
        LinearArena& FrameArena();

        // Internal lifecycle used by subsystems (keeps members private)
        bool InitMemory();
        void KillMemory();

    private:
        SubsystemRegistry registry_;

        MallocAllocator baseAlloc_;

#if NOC_ENABLE_ASSERTS
        DebugAlloc debugAlloc_{ baseAlloc_ };
        IAllocator* alloc_ = &debugAlloc_;
#else
        IAllocator* alloc_ = &baseAlloc_;
#endif

        void* frameArenaMem_ = nullptr;
        LinearArena frameArena_;
    };

} // namespace noc
```

### Listing 44 — `Phase 2 - Window & Main Loop.md` — 14. Implementations > `Engine/Runtime/Engine.cpp`
```cpp
#include "Engine.h"
#include "Core/Log.h"
#include "Core/Assert.h"
#include "Core/Clock.h"
#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h"


namespace noc {

	IAllocator& Engine::Allocator() { return *alloc_; }
	LinearArena& Engine::FrameArena() { return frameArena_; }

	bool Engine::InitMemory()
	{
		constexpr std::size_t kFrameArenaBytes = 8 * 1024 * 1024; // Design choice
		frameArenaMem_ = Allocator().Allocate(kFrameArenaBytes, 64);
		frameArena_.Init(frameArenaMem_, kFrameArenaBytes);

		NOC_LOG_INFO("Core", "Memory system initialized (frame arena=%zu bytes)", kFrameArenaBytes);
		return true;
	}

	void Engine::KillMemory()
	{
		// Free frame arena first
		if (frameArenaMem_)
		{
			Allocator().Deallocate(frameArenaMem_);
			frameArenaMem_ = nullptr;
		}

#if NOC_ENABLE_ASSERTS
		NOC_LOG_INFO("Core", "Memory stats: total=%zu outstanding=%zu allocs=%zu",
			debugAlloc_.TotalAllocatedBytes(),
			debugAlloc_.OutstandingBytes(),
			debugAlloc_.AllocationCount());
#endif
	}


	static bool StartupLog(void*)
	{
		noc::GetLogger().Init();
		NOC_LOG_INFO("Core", "Logger initialized");
		return true;
	}

	static void ShutdownLog(void*)
	{
		NOC_LOG_INFO("Core", "Logger shutting down");
		noc::GetLogger().Shutdown();
	}

	static bool StartupTime(void*)
	{
		noc::GetTime().Init();
		NOC_LOG_INFO("Core", "Time system initialized");
		return true;
	}

	static void ShutdownTime(void*)
	{
		NOC_LOG_INFO("Core", "Time system shutting down");
	}

	static bool StartupAssert(void*)
	{
		NOC_LOG_INFO("Core", "Assert system initialized");
		return true;
	}

	static void ShutdownAssert(void*)
	{
		NOC_LOG_INFO("Core", "Assert system shutting down");
	}

	static bool StartupMemory(void* ctx)
	{
		auto* e = static_cast<noc::Engine*>(ctx);
		return e->InitMemory();
	}

	static void ShutdownMemory(void* ctx)
	{
		NOC_LOG_INFO("Core", "Memory system shutting down");
		auto* e = static_cast<noc::Engine*>(ctx);
		e->KillMemory();
	}

	static bool StartupWindow(void* ctx)
	{
		auto* e = static_cast<Engine*>(ctx);
		(void)e; // engine-owned window is created in Engine::Run (not in subsystem) — see note below.
		NOC_LOG_INFO("Win32", "Window subsystem ready");
		return true;
	}

	static void ShutdownWindow(void*)
	{
		NOC_LOG_INFO("Win32", "Window subsystem shutdown");
	}


	bool Engine::Init()
	{
		std::span<const char* const> depsLog{}; // empty: no dependencies

		static const char* kDepsNeedLog[] = { "Log" };
		std::span<const char* const> depsNeedLog{ kDepsNeedLog, 1 };

		registry_.Register(SubsystemDesc{ "Log",    depsLog,     &StartupLog,    &ShutdownLog });
		registry_.Register(SubsystemDesc{ "Time",   depsNeedLog, &StartupTime,   &ShutdownTime });
		registry_.Register(SubsystemDesc{ "Memory", depsNeedLog, &StartupMemory, &ShutdownMemory });
		registry_.Register(SubsystemDesc{ "Assert", depsNeedLog, &StartupAssert, &ShutdownAssert });
		registry_.Register(SubsystemDesc{ "Window", depsNeedLog, &StartupWindow, &ShutdownWindow });


		return registry_.StartupAll(this);
	}

	int Engine::Run()
	{
		// Create the actual native window here (engine-owned lifetime),
		// after subsystems are up, before entering loop.
		// Design choice: explicit ownership in Engine::Run (simplifies future editor/game split).

		WinWindow window;
		WinWindowDesc wd{};
		wd.title = L"NocturneHost";
		wd.width = 1280;
		wd.height = 720;
		wd.resizable = true;

		if (!window.Create(wd))
		{
			NOC_LOG_FATAL("Win32", "Failed to create window");
			return -1;
		}

		MainLoop loop;
		loop.Run(*this, window);

		window.Destroy();
		return 0;
	}

	void Engine::BeginFrame()
	{
		GetTime().BeginFrame();
		FrameArena().Reset();
	}

	void Engine::Tick()
	{
		// Phase 2: intentionally empty.
		// We keep a tiny log sample at low frequency if desired, but avoid per-frame spam.
	}

	void Engine::EndFrame()
	{
		GetTime().EndFrame();
		// Optional: log dt occasionally; do NOT spam every frame.
	}

	void Engine::TickOnce()
	{
		GetTime().BeginFrame();

		FrameArena().Reset();

		// Allocate a few blocks
		void* a = FrameArena().Allocate(256, 16);
		void* b = FrameArena().Allocate(1024, 64);
		(void)a; (void)b;

		GetTime().EndFrame();

		NOC_LOG_INFO("Core", "TickOnce() dt=%.6f sec arenaUsed=%zu bytes",
			GetTime().DeltaSeconds(),
			FrameArena().Used());

	}


	void Engine::Shutdown()
	{
		registry_.ShutdownAll(this);
	}

} // namespace noc
```

### Listing 45 — `Phase 3 — Resources & Virtual File System.md` — 4. Updated Folder Structure (After Phase 3)
```

Nocturne/
├── Engine/
│   ├── Core/                          // unchanged from Phase 1
│   │   ├── Assert.h
│   │   ├── Assert.cpp
│   │   ├── BuildConfig.h
│   │   ├── Log.h
│   │   ├── Log.cpp
│   │   ├── Clock.h
│   │   ├── Clock.cpp
│   │   ├── Memory/
│   │   │   ├── Allocator.h
│   │   │   ├── Allocator.cpp
│   │   │   ├── DebugAlloc.h
│   │   │   ├── DebugAlloc.cpp
│   │   │   ├── LinearArena.h
│   │   │   └── LinearArena.cpp
│   │   └── Subsystems/
│   │       ├── Subsystem.h
│   │       └── SubsystemRegistry.h / .cpp
│   │
│   ├── Platform/
│   │   └── Win32/
│   │       ├── WinPlatform.h
│   │       ├── WinPlatform.cpp
│   │       ├── WinWindow.h
│   │       ├── WinWindow.cpp
│   │       ├── WinFileSystem.h          // NEW (Phase 3)
│   │       └── WinFileSystem.cpp        // NEW (Phase 3)
│   │
│   ├── Resources/
│   │   ├── VirtualFileSystem.h          // NEW (Phase 3)
│   │   ├── VirtualFileSystem.cpp
│   │   ├── VPath.h                      // NEW (Phase 3)
│   │   ├── VPath.cpp
│   │   ├── FileHandle.h                 // NEW (Phase 3)
│   │   ├── IFileMount.h                 // NEW (Phase 3)
│   │   ├── LooseFileMount.h             // NEW (Phase 3)
│   │   ├── LooseFileMount.cpp
│   │   ├── ArchiveFileMount.h           // NEW (Phase 3)
│   │   └── ArchiveFileMount.cpp
│   │
│   └── Runtime/
│       ├── Engine.h
│       ├── Engine.cpp                   // UPDATED (Phase 3)
│       ├── MainLoop.h
│       └── MainLoop.cpp
│
├── Apps/
│   └── NocturneHost/
│       ├── main.cpp
│       └── AppConfig.h
│
└── Docs/
├── Nocturne Engine Architecture.md
├── Phase 1 — Core Systems.md
├── Phase 2 — Window & Main Loop.md
└── Phase 3 — Resources & Virtual File System.md

```

### Listing 46 — `Phase 3 — Resources & Virtual File System.md` — 5. Virtual Paths
```

textures/ui/crosshair.dds
models/characters/doctor.mesh
audio/ambient/hallway_loop.wav

```

### Listing 47 — `Phase 3 — Resources & Virtual File System.md` — 6. Mount System Design > Resolution Rules (LOCKED)
```
archive → content → override
```

### Listing 48 — `Phase 3 — Resources & Virtual File System.md` — 8. Platform Isolation (Mandatory)
```

Engine/Platform/Win32/WinFileSystem.*

```

### Listing 49 — `Phase 3 — Resources & Virtual File System.md` — 9. VFS Ownership & Lifetime
```

Engine
↓
VirtualFileSystem
↓
FileMounts
↓
Platform File Handles

````

Rules:

- Engine owns the VFS
- VFS owns mounts
- Mounts own backend state
- File handles are opaque and short-lived

---

## 10. Canonical VFS API (Conceptual)

```cpp
class VirtualFileSystem
{
public:
    bool MountLooseDirectory(const char* physicalPath);
    bool MountArchive(const char* archivePath);

    FileHandle OpenRead(const char* virtualPath);
    void Close(FileHandle& handle);

    size_t Read(FileHandle& handle, void* dst, size_t bytes);
    size_t Size(const FileHandle& handle) const;
};
````

This API is intentionally **minimal** and **synchronous**.

Async I/O comes later.

---

## 11. Error Handling Rules

* Missing file → log error, return invalid handle
* Partial reads allowed
* No exceptions
* No crashes on I/O failure
* Engine continues running

---

## 12. Verification Checklist (Phase 3 Is Done When…)

- [x] Engine mounts a loose data directory at startup
- [x] Files open correctly via virtual paths
- [x] Archive and loose mounts coexist
- [x] Stored-entry archive reads verified
- [x] Mount priority resolves deterministically
- [x] No OS file APIs are used outside Platform layer
- [x] Clean shutdown with no leaked file handles


---

## 13. Common Pitfalls (Phase 3)

* Letting OS paths leak above Platform
* Mixing resource identity with file location
* Designing async APIs too early
* Hardcoding archive assumptions into higher layers

---

## 14. Phase 3 Completion Criteria

Phase 3 is **complete**.

The engine now provides:

- A fully functional Virtual File System
- Deterministic virtual path resolution
- Multiple mount support with explicit override policy
- Platform-isolated file I/O
- Verified loose and archive-backed reads

This layer is stable and becomes the **foundation for all resource loading, streaming, and asset management** in subsequent phases.


---

## 15. Next Phase Handoff

When Phase 3 is complete, say:

> **“Phase 3 is complete. The Virtual File System is working. Start Phase 4: Resource Manager.”**

Phase 4 will introduce:

* Resource IDs
* Central resource registry
* Asynchronous loading
* Streaming-aware lifetime management

---

## 16. Implementation Notes (Locked)

- ZIP archive support in Phase 3 is **read-only** and **stored-entry only**.
- Compression and decompression are intentionally deferred.
- All file I/O is synchronous by design.
- Resource identity is fully decoupled from physical storage.

## 17. Implementations

Below are **all files implemented/modified in Phase 2**, each with:

* **`path/to/file`**
* **full implementation**

`Engine/Platform/Win32/WinFileSystem.h`

```cpp
#pragma once

#include <cstdint>
#include <cstddef>

namespace noc::platform
{
    // Opaque native file handle wrapper.
    struct WinFile
    {
        void* handle = nullptr; // HANDLE
    };

    enum class FileOpenMode : uint8_t
    {
        ReadOnly
    };

    struct FileStat
    {
        uint64_t sizeBytes = 0;
    };

    // Opens a file (UTF-8 path). Returns true on success.
    bool OpenFile(WinFile& outFile, const char* utf8Path, FileOpenMode mode);

    // Closes file if open.
    void CloseFile(WinFile& file);

    // Reads from current cursor. Returns bytes read (0 on EOF or failure).
    size_t ReadFile(WinFile& file, void* dst, size_t bytes);

    // Seeks to absolute offset from beginning. Returns true on success.
    bool SeekFile(WinFile& file, uint64_t absoluteOffset);

    // Gets file size. Returns true on success.
    bool GetFileStat(WinFile& file, FileStat& outStat);

    // Utilities

    bool GetExecutableDirectoryUtf8(char* outBuf, size_t outBufBytes); // null-terminated
    bool JoinPathUtf8(char* outBuf, size_t outBufBytes, const char* a, const char* b); // "a/b"
}
````
---

`Engine/Platform/Win32/WinFileSystem.cpp`
```cpp
#include "WinFileSystem.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstring>

namespace noc::platform
{
    static bool Utf8ToWide(const char* utf8, wchar_t* outWide, int outWideCount)
    {
        if (!utf8 || !outWide || outWideCount <= 0)
            return false;

        const int needed = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
        if (needed <= 0 || needed > outWideCount)
            return false;

        const int written = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, outWide, outWideCount);
        return written > 0;
    }

    bool OpenFile(WinFile& outFile, const char* utf8Path, FileOpenMode mode)
    {
        outFile.handle = nullptr;

        wchar_t widePath[MAX_PATH * 4]{};
        if (!Utf8ToWide(utf8Path, widePath, (int)(sizeof(widePath) / sizeof(widePath[0]))))
            return false;

        DWORD access = 0;
        DWORD share = FILE_SHARE_READ;

        switch (mode)
        {
        case FileOpenMode::ReadOnly:
            access = GENERIC_READ;
            break;
        default:
            access = GENERIC_READ;
            break;
        }

        HANDLE h = ::CreateFileW(
            widePath,
            access,
            share,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (h == INVALID_HANDLE_VALUE)
            return false;

        outFile.handle = (void*)h;
        return true;
    }

    void CloseFile(WinFile& file)
    {
        if (file.handle)
        {
            ::CloseHandle((HANDLE)file.handle);
            file.handle = nullptr;
        }
    }

    size_t ReadFile(WinFile& file, void* dst, size_t bytes)
    {
        if (!file.handle || !dst || bytes == 0)
            return 0;

        DWORD read = 0;
        const DWORD toRead = (bytes > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (DWORD)bytes;

        if (!::ReadFile((HANDLE)file.handle, dst, toRead, &read, nullptr))
            return 0;

        return (size_t)read;
    }

    bool SeekFile(WinFile& file, uint64_t absoluteOffset)
    {
        if (!file.handle)
            return false;

        LARGE_INTEGER li{};
        li.QuadPart = (LONGLONG)absoluteOffset;

        return ::SetFilePointerEx((HANDLE)file.handle, li, nullptr, FILE_BEGIN) != 0;
    }

    bool GetFileStat(WinFile& file, FileStat& outStat)
    {
        outStat = {};

        if (!file.handle)
            return false;

        LARGE_INTEGER sz{};
        if (!::GetFileSizeEx((HANDLE)file.handle, &sz))
            return false;

        outStat.sizeBytes = (uint64_t)sz.QuadPart;
        return true;
    }

    bool GetExecutableDirectoryUtf8(char* outBuf, size_t outBufBytes)
    {
        if (!outBuf || outBufBytes == 0)
            return false;

        wchar_t wpath[MAX_PATH]{};
        const DWORD len = ::GetModuleFileNameW(nullptr, wpath, (DWORD)(sizeof(wpath) / sizeof(wpath[0])));
        if (len == 0 || len >= (DWORD)(sizeof(wpath) / sizeof(wpath[0])))
            return false;

        // Strip filename.
        for (int i = (int)len - 1; i >= 0; --i)
        {
            if (wpath[i] == L'\\' || wpath[i] == L'/')
            {
                wpath[i] = 0;
                break;
            }
        }

        const int needed = ::WideCharToMultiByte(CP_UTF8, 0, wpath, -1, nullptr, 0, nullptr, nullptr);
        if (needed <= 0 || (size_t)needed > outBufBytes)
            return false;

        const int written = ::WideCharToMultiByte(CP_UTF8, 0, wpath, -1, outBuf, (int)outBufBytes, nullptr, nullptr);
        return written > 0;
    }

    bool JoinPathUtf8(char* outBuf, size_t outBufBytes, const char* a, const char* b)
    {
        if (!outBuf || outBufBytes == 0 || !a || !b)
            return false;

        const size_t aLen = std::strlen(a);
        const size_t bLen = std::strlen(b);

        // Worst case: a + "/" + b + "\0"
        if (aLen + 1 + bLen + 1 > outBufBytes)
            return false;

        std::memcpy(outBuf, a, aLen);
        size_t pos = aLen;

        if (pos > 0 && outBuf[pos - 1] != '/' && outBuf[pos - 1] != '\\')
            outBuf[pos++] = '/';

        // Copy b
        std::memcpy(outBuf + pos, b, bLen);
        pos += bLen;
        outBuf[pos] = 0;

        // Normalize slashes to '/'
        for (size_t i = 0; i < pos; ++i)
        {
            if (outBuf[i] == '\\')
                outBuf[i] = '/';
        }

        return true;
    }
}
````
---

`Engine/Resources/VirtualFileSystem.h`
```cpp
#pragma once

#include "FileHandle.h"
#include "IFileMount.h"
#include <memory>
#include <string_view>
#include <vector>

namespace noc
{
    class IFileMount;

    class VirtualFileSystem
    {
    public:
        VirtualFileSystem() = default;

        // FIX: out-of-line destructor (defined in .cpp where IFileMount is complete)
        ~VirtualFileSystem();

        bool MountLooseDirectory(const char* physicalRootUtf8);
        bool MountArchive(const char* archivePathUtf8);

        FileHandle OpenRead(std::string_view virtualPath);
        void Close(FileHandle& h);

        size_t Read(FileHandle& h, void* dst, size_t bytes);
        uint64_t Size(const FileHandle& h) const;

        size_t MountCount() const { return mounts_.size(); }

    private:
        std::vector<std::unique_ptr<IFileMount>> mounts_;
    };
}
````
---

`Engine/Resources/VirtualFileSystem.cpp`
```cpp
#include "VirtualFileSystem.h"

#include "VPath.h"
#include "IFileMount.h"
#include "LooseFileMount.h"
#include "ArchiveFileMount.h"

#include "Core/Log.h"

#include <cstring>   // memcpy
#include <limits>    // numeric_limits

namespace noc
{
    VirtualFileSystem::~VirtualFileSystem() = default;

    bool VirtualFileSystem::MountLooseDirectory(const char* physicalRootUtf8)
    {
        if (!physicalRootUtf8 || physicalRootUtf8[0] == 0)
            return false;

        mounts_.push_back(std::make_unique<LooseFileMount>(physicalRootUtf8));
        NOC_LOG_INFO("VFS", "Mounted loose directory: %s (priority=%zu)", physicalRootUtf8, mounts_.size() - 1);
        return true;
    }

    bool VirtualFileSystem::MountArchive(const char* archivePathUtf8)
    {
        if (!archivePathUtf8 || archivePathUtf8[0] == 0)
            return false;

        auto mount = std::make_unique<ArchiveFileMount>(archivePathUtf8);
        if (!mount->BuildIndex())
        {
            NOC_LOG_ERROR("VFS", "Failed to mount archive: %s", archivePathUtf8);
            return false;
        }

        mounts_.push_back(std::move(mount));
        NOC_LOG_INFO("VFS", "Mounted archive: %s (priority=%zu)", archivePathUtf8, mounts_.size() - 1);
        return true;
    }

    FileHandle VirtualFileSystem::OpenRead(std::string_view virtualPath)
    {
        FileHandle h{};

        char norm[512]{};
        if (!noc::vpath::NormalizeToRelative(norm, sizeof(norm), virtualPath))
        {
            NOC_LOG_ERROR("VFS", "Invalid virtual path: %.*s", (int)virtualPath.size(), virtualPath.data());
            return h;
        }

        const std::string_view normalized{ norm };

        // Later mounts override earlier mounts -> search in reverse order.
        for (size_t i = mounts_.size(); i-- > 0; )
        {
            auto& m = mounts_[i];
            if (!m)
                continue;

            if (m->OpenRead(normalized, h))
            {
                h.valid = true;
                return h;
            }
        }

        NOC_LOG_ERROR("VFS", "File not found in any mount: %s", norm);
        return {};
    }


    void VirtualFileSystem::Close(FileHandle& h)
    {
        if (!h.valid || !h.mount)
            return;

        h.mount->Close(h);

        // Defensive: make double-close safe.
        h.valid = false;
        h.mount = nullptr;
        h.backend = nullptr;
        h.sizeBytes = 0;
        h.cursor = 0;
    }


    size_t VirtualFileSystem::Read(FileHandle& h, void* dst, size_t bytes)
    {
        if (!h.valid || !h.mount)
            return 0;

        return h.mount->Read(h, dst, bytes);
    }

    uint64_t VirtualFileSystem::Size(const FileHandle& h) const
    {
        if (!h.valid || !h.mount)
            return 0;

        return h.mount->Size(h);
    }

    // ------------------------------------------------------------
    // Convenience APIs
    // ------------------------------------------------------------

    uint8_t* VirtualFileSystem::ReadAllBytes(const char* virtualPath, size_t& outSize, IAllocator& alloc)
    {
        outSize = 0;

        if (!virtualPath || virtualPath[0] == 0)
            return nullptr;

        FileHandle h = this->OpenRead(std::string_view{ virtualPath });
        if (!h.valid)
            return nullptr;

        const uint64_t size64 = this->Size(h);
        if (size64 == 0)
        {
            // Zero-length file is valid; return a non-null pointer only if you want.
            // Design choice: return nullptr for empty file, but treat as success.
            this->Close(h);
            outSize = 0;
            return nullptr;
        }

        if (size64 > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
        {
            NOC_LOG_ERROR("VFS", "ReadAllBytes too large for size_t: %s (size=%llu)",
                virtualPath, (unsigned long long)size64);
            this->Close(h);
            return nullptr;
        }

        const size_t size = static_cast<size_t>(size64);
        void* mem = alloc.Allocate(size, 16);
        if (!mem)
        {
            NOC_LOG_ERROR("VFS", "ReadAllBytes allocation failed: %s (size=%zu)", virtualPath, size);
            this->Close(h);
            return nullptr;
        }

        uint8_t* data = static_cast<uint8_t*>(mem);

        size_t totalRead = 0;
        while (totalRead < size)
        {
            const size_t toRead = size - totalRead;
            const size_t got = this->Read(h, data + totalRead, toRead);
            if (got == 0)
            {
                NOC_LOG_ERROR("VFS", "ReadAllBytes short read: %s (got=%zu expected=%zu)",
                    virtualPath, totalRead, size);
                alloc.Deallocate(data);
                this->Close(h);
                return nullptr;
            }
            totalRead += got;
        }

        this->Close(h);
        outSize = size;
        return data;
    }

    bool VirtualFileSystem::ReadAllText(const char* virtualPath, IAllocator& alloc, char*& outText)
    {
        outText = nullptr;

        if (!virtualPath || virtualPath[0] == 0)
            return false;

        FileHandle h = this->OpenRead(std::string_view{ virtualPath });
        if (!h.valid)
            return false;

        const uint64_t size64 = this->Size(h);
        if (size64 > static_cast<uint64_t>(std::numeric_limits<size_t>::max() - 1))
        {
            NOC_LOG_ERROR("VFS", "ReadAllText too large for size_t: %s (size=%llu)",
                virtualPath, (unsigned long long)size64);
            this->Close(h);
            return false;
        }

        const size_t size = static_cast<size_t>(size64);

        // +1 for '\0'
        char* text = static_cast<char*>(alloc.Allocate(size + 1, 16));
        if (!text)
        {
            NOC_LOG_ERROR("VFS", "ReadAllText allocation failed: %s (size=%zu)", virtualPath, size + 1);
            this->Close(h);
            return false;
        }

        size_t totalRead = 0;
        while (totalRead < size)
        {
            const size_t toRead = size - totalRead;
            const size_t got = this->Read(h, text + totalRead, toRead);
            if (got == 0)
            {
                NOC_LOG_ERROR("VFS", "ReadAllText short read: %s (got=%zu expected=%zu)",
                    virtualPath, totalRead, size);
                alloc.Deallocate(text);
                this->Close(h);
                return false;
            }
            totalRead += got;
        }

        text[size] = '\0';

        this->Close(h);
        outText = text;
        return true;
    }
}
````
---

`Engine/Resources/VPath.h`
```cpp
#pragma once

#include <string_view>

namespace noc::vpath
{
    // Returns true if p is a legal virtual path:
    // - not empty
    // - uses '/' only (no '\')
    // - no drive letters
    // - no leading "//"
    // - no ".." segments
    // - no absolute OS paths
    bool IsValid(std::string_view p);

    // Normalizes a path into outBuf:
    // - converts '\' to '/'
    // - removes leading '/'
    // - collapses consecutive '/'
    // - rejects ".." traversal (returns false)
    //
    // Output is null-terminated on success.
    bool NormalizeToRelative(char* outBuf, size_t outBufBytes, std::string_view p);
}
````

`Engine/Resources/VPath.cpp`
```cpp
#include "VPath.h"

#include <cctype>
#include <cstring>

namespace noc::vpath
{
    static bool IsDriveLetterPath(std::string_view p)
    {
        // "C:\..." or "C:/..."
        if (p.size() >= 2 && std::isalpha((unsigned char)p[0]) && p[1] == ':')
            return true;
        return false;
    }

    bool IsValid(std::string_view p)
    {
        if (p.empty())
            return false;

        if (IsDriveLetterPath(p))
            return false;

        // Reject UNC-ish or double leading slashes
        if (p.size() >= 2 && (p[0] == '/' || p[0] == '\\') && (p[1] == '/' || p[1] == '\\'))
            return false;

        // Reject backslashes anywhere
        for (char c : p)
        {
            if (c == '\\')
                return false;
        }

        // Reject ".." segments
        // We check token-by-token split on '/'
        size_t i = 0;
        while (i < p.size())
        {
            // Skip '/'
            while (i < p.size() && p[i] == '/')
                ++i;

            size_t start = i;
            while (i < p.size() && p[i] != '/')
                ++i;

            const size_t len = i - start;
            if (len == 2 && p[start] == '.' && p[start + 1] == '.')
                return false;
        }

        return true;
    }

    bool NormalizeToRelative(char* outBuf, size_t outBufBytes, std::string_view p)
    {
        if (!outBuf || outBufBytes == 0)
            return false;

        // Convert '\' to '/' for the purpose of normalization
        // but reject drive letters and ".." segments.
        if (IsDriveLetterPath(p))
            return false;

        // Reject ".." segments even if backslashes exist
        // (we normalize slashes in a copy loop).
        // Also reject UNC style.
        if (p.size() >= 2 && (p[0] == '/' || p[0] == '\\') && (p[1] == '/' || p[1] == '\\'))
            return false;

        // Build normalized output:
        // - remove leading '/'
        // - collapse multiple '/'
        // - convert '\' to '/'
        size_t out = 0;
        bool lastWasSlash = false;

        for (size_t i = 0; i < p.size(); ++i)
        {
            char c = p[i];
            if (c == '\\') c = '/';

            if (c == '/')
            {
                // skip leading slash
                if (out == 0)
                    continue;

                if (lastWasSlash)
                    continue;

                if (out + 1 >= outBufBytes)
                    return false;

                outBuf[out++] = '/';
                lastWasSlash = true;
                continue;
            }

            // normal char
            if (out + 1 >= outBufBytes)
                return false;

            outBuf[out++] = c;
            lastWasSlash = false;
        }

        // Trim trailing '/'
        while (out > 0 && outBuf[out - 1] == '/')
            --out;

        if (out == 0)
            return false;

        outBuf[out] = 0;

        // Now validate segments for ".."
        std::string_view norm(outBuf, out);
        if (!IsValid(norm))
            return false;

        return true;
    }
}
````
---
`Engine/Resources/FileHandle.h`
```cpp
#pragma once

#include <cstdint>

namespace noc
{
    class IFileMount;

    struct FileHandle
    {
        IFileMount* mount = nullptr;
        void* backend = nullptr;     // mount-defined opaque state (e.g., WinFile*)
        uint64_t sizeBytes = 0;
        uint64_t cursor = 0;
        bool valid = false;
    };
}
````

---

`Engine/Resources/IFileMount.h`
```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "FileHandle.h"

namespace noc
{
    class IFileMount
    {
    public:
        virtual ~IFileMount() = default;

        // Returns true if this mount can open the file for read.
        virtual bool OpenRead(std::string_view normalizedRelativeVPath, FileHandle& out) = 0;

        virtual void Close(FileHandle& h) = 0;

        // Read from current cursor; advances cursor.
        virtual size_t Read(FileHandle& h, void* dst, size_t bytes) = 0;

        // Returns file size in bytes.
        virtual uint64_t Size(const FileHandle& h) const = 0;
    };
}
````

---

`Engine/Resources/LooseFileMount.h`
```cpp
#pragma once

#include "IFileMount.h"

namespace noc
{
    class LooseFileMount final : public IFileMount
    {
    public:
        explicit LooseFileMount(const char* physicalRootUtf8);

        bool OpenRead(std::string_view normalizedRelativeVPath, FileHandle& out) override;
        void Close(FileHandle& h) override;
        size_t Read(FileHandle& h, void* dst, size_t bytes) override;
        uint64_t Size(const FileHandle& h) const override;

    private:
        char root_[512]{}; // UTF-8 root path (normalized to use '/')
    };
}
````

---

`Engine/Resources/LooseFileMount.cpp`
```cpp
#include "LooseFileMount.h"

#include "Platform/Win32/WinFileSystem.h"
#include "Core/Log.h"

#include <cstring>
#include <string>

namespace noc
{
    struct LooseBackend
    {
        noc::platform::WinFile file{};
        uint64_t size = 0;
    };

    static void NormalizeRoot(char* dst, size_t dstBytes, const char* src)
    {
#if defined(_MSC_VER)
        strncpy_s(dst, dstBytes, src ? src : "", _TRUNCATE);
#else
        std::strncpy(dst, src ? src : "", dstBytes - 1);
        dst[dstBytes - 1] = 0;
#endif
        for (size_t i = 0; dst[i] != 0; ++i)
        {
            if (dst[i] == '\\')
                dst[i] = '/';
        }
        // trim trailing '/'
        size_t len = std::strlen(dst);
        while (len > 0 && dst[len - 1] == '/')
        {
            dst[len - 1] = 0;
            --len;
        }
    }

    LooseFileMount::LooseFileMount(const char* physicalRootUtf8)
    {
        NormalizeRoot(root_, sizeof(root_), physicalRootUtf8 ? physicalRootUtf8 : "");
    }

    bool LooseFileMount::OpenRead(std::string_view normalizedRelativeVPath, FileHandle& out)
    {
        out = {};

        char fullPath[1024]{};

        // FIX: JoinPathUtf8(out, outBytes, root, relative)
        const std::string rel(normalizedRelativeVPath);
        if (!noc::platform::JoinPathUtf8(fullPath, sizeof(fullPath), root_, rel.c_str()))
            return false;

        auto* backend = new LooseBackend();

        if (!noc::platform::OpenFile(backend->file, fullPath, noc::platform::FileOpenMode::ReadOnly))
        {
            delete backend;
            return false;
        }

        noc::platform::FileStat st{};
        if (!noc::platform::GetFileStat(backend->file, st))
        {
            noc::platform::CloseFile(backend->file);
            delete backend;
            return false;
        }

        backend->size = st.sizeBytes;

        out.mount = this;
        out.backend = backend;
        out.sizeBytes = backend->size;
        out.cursor = 0;
        out.valid = true;

        return true;
    }

    void LooseFileMount::Close(FileHandle& h)
    {
        if (!h.valid || h.mount != this || !h.backend)
            return;

        auto* backend = reinterpret_cast<LooseBackend*>(h.backend);
        noc::platform::CloseFile(backend->file);
        delete backend;

        h = {};
    }

    size_t LooseFileMount::Read(FileHandle& h, void* dst, size_t bytes)
    {
        if (!h.valid || h.mount != this || !h.backend || bytes == 0)
            return 0;

        auto* backend = reinterpret_cast<LooseBackend*>(h.backend);

        if (!noc::platform::SeekFile(backend->file, h.cursor))
            return 0;

        const size_t read = noc::platform::ReadFile(backend->file, dst, bytes);
        h.cursor += (uint64_t)read;
        return read;
    }

    uint64_t LooseFileMount::Size(const FileHandle& h) const
    {
        return h.sizeBytes;
    }
}
````

---

`Engine/Resources/ArchiveFileMount.h`
```cpp
#pragma once

#include "IFileMount.h"

#include <unordered_map>

#include "Platform/Win32/WinFileSystem.h"

#include <string>

namespace noc
{
    // ZIP read-only mount (Phase 3):
    // - Supports "stored" entries (compression method 0) only.
    // - Logs and refuses compressed entries.
    class ArchiveFileMount final : public IFileMount
    {
    public:
        explicit ArchiveFileMount(const char* archivePathUtf8);

        // Must be called once after construction; returns false if archive can't be indexed.
        bool BuildIndex();

        bool OpenRead(std::string_view normalizedRelativeVPath, FileHandle& out) override;
        void Close(FileHandle& h) override;
        size_t Read(FileHandle& h, void* dst, size_t bytes) override;
        uint64_t Size(const FileHandle& h) const override;

    private:
        struct Entry
        {
            uint32_t method = 0;            // 0 = stored
            uint64_t uncompressedSize = 0;
            uint64_t compressedSize = 0;
            uint64_t dataOffset = 0;        // absolute offset in archive to file data
        };

        struct ArchiveBackend
        {
            noc::platform::WinFile file{};
            Entry entry{};
        };

        bool FindEntry(std::string_view normalizedRelativeVPath, Entry& outEntry) const;

        bool ComputeEntryDataOffset(uint64_t localHeaderOffset, uint64_t& outDataOffset) const;

        bool ReadAt(uint64_t offset, void* dst, size_t bytes, size_t& outRead) const;

    private:
        char archivePath_[1024]{};
        mutable noc::platform::WinFile indexFile_{}; // used only during indexing

        std::unordered_map<std::string, Entry> entries_;
    };
}
````

---

`Engine/Resources/ArchiveFileMount.cpp`
```cpp
#include "ArchiveFileMount.h"

#include "Platform/Win32/WinFileSystem.h"
#include "Core/Log.h"

#include <cstring>
#include <vector>

namespace noc
{
#pragma pack(push, 1)
    struct ZipEOCD
    {
        uint32_t signature;           // 0x06054b50
        uint16_t diskNumber;
        uint16_t centralDirDisk;
        uint16_t centralDirRecordsOnDisk;
        uint16_t centralDirRecordsTotal;
        uint32_t centralDirSize;
        uint32_t centralDirOffset;
        uint16_t commentLength;
        // comment follows
    };

    struct ZipCentralDirHeader
    {
        uint32_t signature;           // 0x02014b50
        uint16_t versionMadeBy;
        uint16_t versionNeeded;
        uint16_t flags;
        uint16_t compressionMethod;
        uint16_t modTime;
        uint16_t modDate;
        uint32_t crc32;
        uint32_t compressedSize;
        uint32_t uncompressedSize;
        uint16_t fileNameLen;
        uint16_t extraLen;
        uint16_t commentLen;
        uint16_t diskStart;
        uint16_t internalAttrs;
        uint32_t externalAttrs;
        uint32_t localHeaderOffset;
        // fileName + extra + comment follow
    };

    struct ZipLocalHeader
    {
        uint32_t signature;           // 0x04034b50
        uint16_t versionNeeded;
        uint16_t flags;
        uint16_t compressionMethod;
        uint16_t modTime;
        uint16_t modDate;
        uint32_t crc32;
        uint32_t compressedSize;
        uint32_t uncompressedSize;
        uint16_t fileNameLen;
        uint16_t extraLen;
        // fileName + extra follow, then file data
    };
#pragma pack(pop)

    static constexpr uint32_t kEOCDSig = 0x06054b50u;
    static constexpr uint32_t kCDSig = 0x02014b50u;
    static constexpr uint32_t kLHSig = 0x04034b50u;

    static void CopyPath(char* dst, size_t dstBytes, const char* src)
    {
#if defined(_MSC_VER)
        strncpy_s(dst, dstBytes, src ? src : "", _TRUNCATE);
#else
        std::strncpy(dst, src ? src : "", dstBytes - 1);
        dst[dstBytes - 1] = 0;
#endif
        for (size_t i = 0; dst[i] != 0; ++i)
        {
            if (dst[i] == '\\')
                dst[i] = '/';
        }
    }


    ArchiveFileMount::ArchiveFileMount(const char* archivePathUtf8)
    {
        CopyPath(archivePath_, sizeof(archivePath_), archivePathUtf8);
    }

    bool ArchiveFileMount::ReadAt(uint64_t offset, void* dst, size_t bytes, size_t& outRead) const
    {
        outRead = 0;

        // Use a temporary open file handle to avoid shared cursor issues during indexing.
        // For indexing we keep indexFile_ open; for safety we still seek before read.
        if (!indexFile_.handle)
            return false;

        if (!noc::platform::SeekFile(indexFile_, offset))
            return false;

        outRead = noc::platform::ReadFile(indexFile_, dst, bytes);
        return outRead == bytes;
    }

    bool ArchiveFileMount::ComputeEntryDataOffset(uint64_t localHeaderOffset, uint64_t& outDataOffset) const
    {
        ZipLocalHeader lh{};
        size_t read = 0;
        if (!ReadAt(localHeaderOffset, &lh, sizeof(lh), read))
            return false;

        if (lh.signature != kLHSig)
            return false;

        outDataOffset = localHeaderOffset + sizeof(ZipLocalHeader) + lh.fileNameLen + lh.extraLen;
        return true;
    }

    bool ArchiveFileMount::BuildIndex()
    {
        entries_.clear();

        if (!noc::platform::OpenFile(indexFile_, archivePath_, noc::platform::FileOpenMode::ReadOnly))
        {
            NOC_LOG_ERROR("VFS", "Archive open failed: %s", archivePath_);
            return false;
        }

        noc::platform::FileStat st{};
        if (!noc::platform::GetFileStat(indexFile_, st))
        {
            noc::platform::CloseFile(indexFile_);
            NOC_LOG_ERROR("VFS", "Archive stat failed: %s", archivePath_);
            return false;
        }

        const uint64_t fileSize = st.sizeBytes;
        if (fileSize < sizeof(ZipEOCD))
        {
            noc::platform::CloseFile(indexFile_);
            NOC_LOG_ERROR("VFS", "Archive too small: %s", archivePath_);
            return false;
        }

        // EOCD can be up to 64KB comment + record size. We'll search backwards within that window.
        const uint64_t maxSearch = 64ull * 1024ull + sizeof(ZipEOCD);
        const uint64_t searchStart = (fileSize > maxSearch) ? (fileSize - maxSearch) : 0;

        std::vector<uint8_t> tail((size_t)(fileSize - searchStart));
        if (!noc::platform::SeekFile(indexFile_, searchStart))
        {
            noc::platform::CloseFile(indexFile_);
            return false;
        }

        const size_t got = noc::platform::ReadFile(indexFile_, tail.data(), tail.size());
        if (got != tail.size())
        {
            noc::platform::CloseFile(indexFile_);
            return false;
        }

        // Find EOCD signature from end
        int64_t eocdPos = -1;
        for (int64_t i = (int64_t)tail.size() - (int64_t)sizeof(ZipEOCD); i >= 0; --i)
        {
            const uint32_t sig = *(const uint32_t*)(tail.data() + i);
            if (sig == kEOCDSig)
            {
                eocdPos = i;
                break;
            }
        }

        if (eocdPos < 0)
        {
            noc::platform::CloseFile(indexFile_);
            NOC_LOG_ERROR("VFS", "EOCD not found in archive: %s", archivePath_);
            return false;
        }

        ZipEOCD eocd{};
        std::memcpy(&eocd, tail.data() + eocdPos, sizeof(eocd));

        const uint64_t cdOffset = (uint64_t)eocd.centralDirOffset;
        const uint64_t cdSize = (uint64_t)eocd.centralDirSize;

        if (cdOffset + cdSize > fileSize)
        {
            noc::platform::CloseFile(indexFile_);
            NOC_LOG_ERROR("VFS", "Central directory out of range: %s", archivePath_);
            return false;
        }

        // Read entire central directory
        std::vector<uint8_t> cd((size_t)cdSize);
        if (!noc::platform::SeekFile(indexFile_, cdOffset))
        {
            noc::platform::CloseFile(indexFile_);
            return false;
        }

        const size_t cdGot = noc::platform::ReadFile(indexFile_, cd.data(), cd.size());
        if (cdGot != cd.size())
        {
            noc::platform::CloseFile(indexFile_);
            return false;
        }

        size_t cursor = 0;
        while (cursor + sizeof(ZipCentralDirHeader) <= cd.size())
        {
            auto* hdr = (ZipCentralDirHeader*)(cd.data() + cursor);
            if (hdr->signature != kCDSig)
                break;

            cursor += sizeof(ZipCentralDirHeader);

            if (cursor + hdr->fileNameLen + hdr->extraLen + hdr->commentLen > cd.size())
                break;

            const char* namePtr = (const char*)(cd.data() + cursor);
            std::string name(namePtr, namePtr + hdr->fileNameLen);

            cursor += hdr->fileNameLen + hdr->extraLen + hdr->commentLen;

            // Normalize slashes in stored name
            for (char& c : name)
                if (c == '\\') c = '/';

            // Skip directory entries
            if (!name.empty() && name.back() == '/')
                continue;

            Entry e{};
            e.method = hdr->compressionMethod;
            e.compressedSize = hdr->compressedSize;
            e.uncompressedSize = hdr->uncompressedSize;

            uint64_t dataOffset = 0;
            if (!ComputeEntryDataOffset((uint64_t)hdr->localHeaderOffset, dataOffset))
                continue;

            e.dataOffset = dataOffset;

            entries_.emplace(std::move(name), e);
        }

        NOC_LOG_INFO("VFS", "Archive indexed: %s entries=%zu", archivePath_, entries_.size());

        // Keep indexFile_ open only for indexing; close now to avoid holding handles.
        noc::platform::CloseFile(indexFile_);
        return true;
    }

    bool ArchiveFileMount::FindEntry(std::string_view normalizedRelativeVPath, Entry& outEntry) const
    {
        auto it = entries_.find(std::string(normalizedRelativeVPath));
        if (it == entries_.end())
            return false;

        outEntry = it->second;
        return true;
    }

    bool ArchiveFileMount::OpenRead(std::string_view normalizedRelativeVPath, FileHandle& out)
    {
        out = {};

        Entry entry{};
        if (!FindEntry(normalizedRelativeVPath, entry))
            return false;

        if (entry.method != 0)
        {
            // Stored only for Phase 3.
            NOC_LOG_WARN("VFS", "Archive entry compressed (method=%u) not supported yet: %.*s",
                entry.method, (int)normalizedRelativeVPath.size(), normalizedRelativeVPath.data());
            return false;
        }

        auto* backend = new ArchiveBackend();
        if (!noc::platform::OpenFile(backend->file, archivePath_, noc::platform::FileOpenMode::ReadOnly))
        {
            delete backend;
            return false;
        }

        backend->entry = entry;

        // Seek to entry data start
        if (!noc::platform::SeekFile(backend->file, entry.dataOffset))
        {
            noc::platform::CloseFile(backend->file);
            delete backend;
            return false;
        }

        out.mount = this;
        out.backend = backend;
        out.sizeBytes = entry.uncompressedSize;
        out.cursor = 0;
        out.valid = true;
        return true;
    }

    void ArchiveFileMount::Close(FileHandle& h)
    {
        if (!h.valid || h.mount != this || !h.backend)
            return;

        auto* backend = reinterpret_cast<ArchiveBackend*>(h.backend);
        noc::platform::CloseFile(backend->file);
        delete backend;
        h = {};
    }

    size_t ArchiveFileMount::Read(FileHandle& h, void* dst, size_t bytes)
    {
        if (!h.valid || h.mount != this || !h.backend || bytes == 0)
            return 0;

        auto* backend = reinterpret_cast<ArchiveBackend*>(h.backend);

        // Clamp to remaining
        const uint64_t remaining = (h.cursor < h.sizeBytes) ? (h.sizeBytes - h.cursor) : 0;
        if (remaining == 0)
            return 0;

        size_t toRead = bytes;
        if ((uint64_t)toRead > remaining)
            toRead = (size_t)remaining;

        // Seek to absolute position in archive
        const uint64_t abs = backend->entry.dataOffset + h.cursor;
        if (!noc::platform::SeekFile(backend->file, abs))
            return 0;

        const size_t read = noc::platform::ReadFile(backend->file, dst, toRead);
        h.cursor += (uint64_t)read;
        return read;
    }

    uint64_t ArchiveFileMount::Size(const FileHandle& h) const
    {
        return h.sizeBytes;
    }
}
````

---

`Engine/Runtime/Engine.cpp` (updated)
```cpp
#include "Engine.h"
#include "Core/Log.h"
#include "Core/Assert.h"
#include "Core/Clock.h"
#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h"

namespace noc {

	IAllocator& Engine::Allocator() { return *alloc_; }
	LinearArena& Engine::FrameArena() { return frameArena_; }

	EngineConfig& Engine::ConfigMutable()
	{
		if (initialized_)
		{
			NOC_LOG_ERROR("Runtime", "EngineConfig is frozen after Init(). Modify config before calling Init().");
			return cfg_; // returns current config for inspection; do not mutate.
		}
		return cfg_;
	}

	bool Engine::SetContentRoot(const char* path)
	{
		if (!IsConfigMutable())
		{
			NOC_LOG_ERROR("Runtime", "SetContentRoot() called after Init(); ignored.");
			return false;
		}
		cfg_.contentRoot = path;
		return true;
	}

	bool Engine::SetOverrideRoot(const char* path)
	{
		if (!IsConfigMutable())
		{
			NOC_LOG_ERROR("Runtime", "SetOverrideRoot() called after Init(); ignored.");
			return false;
		}
		cfg_.overrideRoot = path;
		return true;
	}

	bool Engine::SetArchivePath(const char* path)
	{
		if (!IsConfigMutable())
		{
			NOC_LOG_ERROR("Runtime", "SetArchivePath() called after Init(); ignored.");
			return false;
		}
		cfg_.archivePath = path;
		return true;
	}

	bool Engine::InitMemory()
	{
		constexpr std::size_t kFrameArenaBytes = 8 * 1024 * 1024; // Design choice
		frameArenaMem_ = Allocator().Allocate(kFrameArenaBytes, 64);
		frameArena_.Init(frameArenaMem_, kFrameArenaBytes);

		NOC_LOG_INFO("Core", "Memory system initialized (frame arena=%zu bytes)", kFrameArenaBytes);
		return true;
	}

	void Engine::KillMemory()
	{
		if (frameArenaMem_)
		{
			Allocator().Deallocate(frameArenaMem_);
			frameArenaMem_ = nullptr;
		}

#if NOC_ENABLE_ASSERTS
		NOC_LOG_INFO("Core", "Memory stats: total=%zu outstanding=%zu allocs=%zu",
			debugAlloc_.TotalAllocatedBytes(),
			debugAlloc_.OutstandingBytes(),
			debugAlloc_.AllocationCount());
#endif
	}

	static bool StartupLog(void*)
	{
		noc::GetLogger().Init();
		NOC_LOG_INFO("Core", "Logger initialized");
		return true;
	}

	static void ShutdownLog(void*)
	{
		NOC_LOG_INFO("Core", "Logger shutting down");
		noc::GetLogger().Shutdown();
	}

	static bool StartupTime(void*)
	{
		noc::GetTime().Init();
		NOC_LOG_INFO("Core", "Time system initialized");
		return true;
	}

	static void ShutdownTime(void*)
	{
		NOC_LOG_INFO("Core", "Time system shutting down");
	}

	static bool StartupAssert(void*)
	{
		NOC_LOG_INFO("Core", "Assert system initialized");
		return true;
	}

	static void ShutdownAssert(void*)
	{
		NOC_LOG_INFO("Core", "Assert system shutting down");
	}

	static bool StartupMemory(void* ctx)
	{
		auto* e = static_cast<noc::Engine*>(ctx);
		return e->InitMemory();
	}

	static void ShutdownMemory(void* ctx)
	{
		NOC_LOG_INFO("Core", "Memory system shutting down");
		auto* e = static_cast<noc::Engine*>(ctx);
		e->KillMemory();
	}

	static bool StartupWindow(void*)
	{
		NOC_LOG_INFO("Win32", "Window subsystem ready");
		return true;
	}

	static void ShutdownWindow(void*)
	{
		NOC_LOG_INFO("Win32", "Window subsystem shutdown");
	}

	bool Engine::Init()
	{
		// Freeze config from this point on.
		initialized_ = true;

		std::span<const char* const> depsLog{};
		static const char* kDepsNeedLog[] = { "Log" };
		std::span<const char* const> depsNeedLog{ kDepsNeedLog, 1 };

		registry_.Register(SubsystemDesc{ "Log",    depsLog,     &StartupLog,    &ShutdownLog });
		registry_.Register(SubsystemDesc{ "Time",   depsNeedLog, &StartupTime,   &ShutdownTime });
		registry_.Register(SubsystemDesc{ "Memory", depsNeedLog, &StartupMemory, &ShutdownMemory });
		registry_.Register(SubsystemDesc{ "Assert", depsNeedLog, &StartupAssert, &ShutdownAssert });
		registry_.Register(SubsystemDesc{ "Window", depsNeedLog, &StartupWindow, &ShutdownWindow });

		if (!registry_.StartupAll(this))
			return false;

		// Phase 3: VFS mount policy from engine config.
		//
		// Mount priority rule: later mounts override earlier mounts.
		// Desired priority (highest last):
		//   overrideRoot > contentRoot > archive
		//
		// So mount in this order:
		//   1) archivePath
		//   2) contentRoot
		//   3) overrideRoot

		if (cfg_.archivePath && cfg_.archivePath[0] != 0)
		{
			if (!vfs_.MountArchive(cfg_.archivePath))
				NOC_LOG_WARN("VFS", "Failed to mount archivePath: %s", cfg_.archivePath);
		}

		if (cfg_.contentRoot && cfg_.contentRoot[0] != 0)
		{
			if (!vfs_.MountLooseDirectory(cfg_.contentRoot))
				NOC_LOG_WARN("VFS", "Failed to mount contentRoot: %s", cfg_.contentRoot);
		}

		if (cfg_.overrideRoot && cfg_.overrideRoot[0] != 0)
		{
			if (!vfs_.MountLooseDirectory(cfg_.overrideRoot))
				NOC_LOG_WARN("VFS", "Failed to mount overrideRoot: %s", cfg_.overrideRoot);
		}


		return true;
	}

	int Engine::Run()
	{
		WinWindow window;
		WinWindowDesc wd{};
		wd.title = L"NocturneHost"; // Phase 2 behavior unchanged (window config is separate from EngineConfig)
		wd.width = 1280;
		wd.height = 720;
		wd.resizable = true;

		if (!window.Create(wd))
		{
			NOC_LOG_FATAL("Win32", "Failed to create window");
			return -1;
		}

		MainLoop loop;
		loop.Run(*this, window);

		window.Destroy();
		return 0;
	}

	void Engine::BeginFrame()
	{
		GetTime().BeginFrame();
		FrameArena().Reset();
	}

	void Engine::Tick()
	{
		// Phase 3: still intentionally empty.
	}

	void Engine::EndFrame()
	{
		GetTime().EndFrame();
	}

	void Engine::TickOnce()
	{
		GetTime().BeginFrame();
		FrameArena().Reset();

		void* a = FrameArena().Allocate(256, 16);
		void* b = FrameArena().Allocate(1024, 64);
		(void)a; (void)b;

		GetTime().EndFrame();

		NOC_LOG_INFO("Core", "TickOnce() dt=%.6f sec arenaUsed=%zu bytes",
			GetTime().DeltaSeconds(),
			FrameArena().Used());
	}

	void Engine::Shutdown()
	{
		registry_.ShutdownAll(this);
	}

} // namespace noc
````

---

`Engine/Runtime/Engine.h` (updated)
```cpp
#pragma once
#include "Core/BuildConfig.h"
#include "Core/Subsystems/SubsystemRegistry.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/LinearArena.h"

#if NOC_ENABLE_ASSERTS
#include "Core/Memory/DebugAlloc.h"
#endif

#include "Resources/VirtualFileSystem.h"
#include "EngineConfig.h"

namespace noc {

    class WinWindow;
    class MainLoop;

    class Engine
    {
    public:
        // Config access: only valid BEFORE Init().
        // If you need to modify config after Init, that becomes a different system later.
        EngineConfig& ConfigMutable();
        const EngineConfig& Config() const { return cfg_; }

        // Convenience pre-init setters (return false if called too late).
        bool SetContentRoot(const char* path);
        bool SetOverrideRoot(const char* path);
        bool SetArchivePath(const char* path);

        bool Init();
        void TickOnce();
        void Shutdown();

        int Run();                 // creates window + runs loop
        void BeginFrame();
        void Tick();
        void EndFrame();

        IAllocator& Allocator();
        LinearArena& FrameArena();

        VirtualFileSystem& VFS() { return vfs_; }

        bool InitMemory();
        void KillMemory();

    private:
        bool IsConfigMutable() const { return !initialized_; }

    private:
        SubsystemRegistry registry_;

        MallocAllocator baseAlloc_;

#if NOC_ENABLE_ASSERTS
        DebugAlloc debugAlloc_{ baseAlloc_ };
        IAllocator* alloc_ = &debugAlloc_;
#else
        IAllocator* alloc_ = &baseAlloc_;
#endif

        void* frameArenaMem_ = nullptr;
        LinearArena frameArena_;

        VirtualFileSystem vfs_;

        EngineConfig cfg_{};
        bool initialized_ = false;
    };

} // namespace noc
````

### Listing 50 — `Phase 4 — Resource Manager.md` — Verification (Phase 4) > Verified Log Output
```txt
[INFO][VFS] Mounted loose directory: D:/Projects/Nocturne/Data (priority=0)
[INFO][Res] ResourceManager initialized (loader thread started)
[INFO][Res] Loader thread running
[INFO][Res] READY: hello.txt (11 bytes)
[INFO][Host] hello.txt loaded (11 bytes)
[INFO][Host] Preview (first 11 bytes): afafafafasf
[INFO][Host] Handle1 = (0, 1)
[INFO][Host] Handle2 = (0, 1)
[INFO][Res] Loader thread exiting
[INFO][Res] ResourceManager shutdown complete
[INFO][Core] Memory stats: total=8388619 outstanding=0 allocs=2
```

### Listing 51 — `Phase 4 — Resource Manager.md` — Implementations > `Engine/Resources/ResourceID.h`
```cpp
#pragma once
#include <cstdint>
#include <string_view>

namespace noc
{
    // Design choice (not directly from the book):
    // A 64-bit hash of the normalized virtual path.
    struct ResourceID
    {
        uint64_t value = 0;

        constexpr bool IsValid() const { return value != 0; }

        friend constexpr bool operator==(ResourceID a, ResourceID b) { return a.value == b.value; }
        friend constexpr bool operator!=(ResourceID a, ResourceID b) { return a.value != b.value; }
    };

    // Design choice (not directly from the book): 64-bit FNV-1a.
    inline constexpr uint64_t Fnv1a64(const char* data, size_t len)
    {
        uint64_t h = 1469598103934665603ull;
        for (size_t i = 0; i < len; ++i)
        {
            h ^= static_cast<uint8_t>(data[i]);
            h *= 1099511628211ull;
        }
        return h;
    }

    inline ResourceID MakeResourceID(std::string_view normalizedVPath)
    {
        ResourceID id{};
        id.value = Fnv1a64(normalizedVPath.data(), normalizedVPath.size());

        // Avoid 0 being a valid ID (tiny guard; extremely unlikely anyway).
        if (id.value == 0) id.value = 1;
        return id;
    }

} // namespace noc
```

### Listing 52 — `Phase 4 — Resource Manager.md` — Implementations > `Engine/Resources/ResourceHandle.h`
```cpp
#pragma once
#include <cstdint>

namespace noc
{
    // Design choice (not directly from the book):
    // index + generation to detect stale handles.
    struct ResourceHandle
    {
        uint32_t index = 0xFFFFFFFFu;
        uint32_t generation = 0;

        constexpr bool IsValid() const { return index != 0xFFFFFFFFu; }

        friend constexpr bool operator==(ResourceHandle a, ResourceHandle b)
        {
            return a.index == b.index && a.generation == b.generation;
        }

        friend constexpr bool operator!=(ResourceHandle a, ResourceHandle b)
        {
            return !(a == b);
        }
    };

} // namespace noc
```

### Listing 53 — `Phase 4 — Resource Manager.md` — Implementations > `Engine/Resources/ResourceManager.h`
```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "Resources/ResourceHandle.h"

namespace noc
{
    class Engine;
    class VirtualFileSystem;

    class ResourceManager
    {
    public:
        ResourceManager() = default;
        ~ResourceManager();

        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;

        bool Init(Engine& engine, VirtualFileSystem& vfs);
        void Shutdown();

        // Call once per frame on the main thread.
        void Update();

        // ---- Binary blob API (Phase 4 scope) ----
        ResourceHandle RequestBinary(std::string_view vpath);

        bool IsReady(ResourceHandle h) const;
        bool HasFailed(ResourceHandle h) const;

        const uint8_t* GetBytes(ResourceHandle h) const;
        size_t GetSize(ResourceHandle h) const;

        // Optional helper for host-side testing.
        bool WaitUntilReady(ResourceHandle h, uint32_t timeoutMs);

    private:
        bool ValidateHandle_(ResourceHandle h, uint32_t* outIndex) const;
        void LoaderThreadMain_();

    private:
        Engine* engine_ = nullptr;
        VirtualFileSystem* vfs_ = nullptr;

        // Internal opaque state (allocated in .cpp)
        void* state_ = nullptr;

        void* loaderThread_ = nullptr;
        bool running_ = false;
    };

} // namespace noc
```

### Listing 54 — `Phase 4 — Resource Manager.md` — Implementations > `Engine/Resources/ResourceManager.cpp`
```cpp
#include "ResourceManager.h"

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <memory>
#include <vector>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Resources/ResourceID.h"
#include "Resources/VirtualFileSystem.h"
#include "Resources/VPath.h"
#include "Resources/FileHandle.h"
#include "Runtime/Engine.h"

namespace noc
{
    enum class ResourceState : uint8_t
    {
        Empty = 0,
        Requested,
        Loading,
        Ready,
        Failed
    };

    struct Record
    {
        ResourceID id{};
        std::string vpathNormalized;
        std::atomic<ResourceState> state{ ResourceState::Empty };

        uint32_t generation = 1;

        uint8_t* data = nullptr;
        size_t size = 0;

        std::string error;

        Record() = default;
        Record(const Record&) = delete;
        Record& operator=(const Record&) = delete;
        Record(Record&&) = delete;
        Record& operator=(Record&&) = delete;
    };

    struct PendingReq { uint32_t index = 0; };

    struct CompletedReq
    {
        uint32_t index = 0;
        bool ok = false;
        std::vector<uint8_t> bytes;
        std::string error;
    };

    struct InternalState
    {
        std::mutex mtx;
        std::condition_variable cv;
        std::queue<PendingReq> pending;
        std::queue<CompletedReq> completed;

        std::unordered_map<uint64_t, uint32_t> idToIndex;

        std::vector<std::unique_ptr<Record>> records;
    };

    static std::string NormalizeVPath_(std::string_view vpath)
    {
        char buf[512]{};
        if (!noc::vpath::NormalizeToRelative(buf, sizeof(buf), vpath))
            return {};
        return std::string(buf);
    }

    ResourceManager::~ResourceManager()
    {
        Shutdown();
    }

    bool ResourceManager::Init(Engine& engine, VirtualFileSystem& vfs)
    {
        if (running_)
            return true;

        engine_ = &engine;
        vfs_ = &vfs;

        auto* st = new InternalState();
        st->records.reserve(256);
        state_ = st;

        running_ = true;
        loaderThread_ = new std::thread([this]() { LoaderThreadMain_(); });

        NOC_LOG_INFO("Res", "ResourceManager initialized (loader thread started)");
        return true;
    }

    void ResourceManager::Shutdown()
    {
        if (!running_)
            return;

        running_ = false;

        auto* st = static_cast<InternalState*>(state_);
        st->cv.notify_all();

        if (loaderThread_)
        {
            auto* t = static_cast<std::thread*>(loaderThread_);
            if (t->joinable())
                t->join();
            delete t;
            loaderThread_ = nullptr;
        }

        // Free blobs
        for (auto& rp : st->records)
        {
            if (rp && rp->data)
            {
                engine_->Allocator().Deallocate(rp->data);
                rp->data = nullptr;
                rp->size = 0;
            }
        }

        delete st;
        state_ = nullptr;

        engine_ = nullptr;
        vfs_ = nullptr;

        NOC_LOG_INFO("Res", "ResourceManager shutdown complete");
    }

    bool ResourceManager::ValidateHandle_(ResourceHandle h, uint32_t* outIndex) const
    {
        if (!h.IsValid() || !state_)
            return false;

        auto* st = static_cast<InternalState*>(state_);
        if (h.index >= st->records.size())
            return false;

        const Record* r = st->records[h.index].get();
        if (!r)
            return false;

        if (r->generation != h.generation)
            return false;

        *outIndex = h.index;
        return true;
    }

    ResourceHandle ResourceManager::RequestBinary(std::string_view vpath)
    {
        if (!running_ || !state_)
            return {};

        auto* st = static_cast<InternalState*>(state_);

        const std::string norm = NormalizeVPath_(vpath);
        if (norm.empty())
        {
            NOC_LOG_ERROR("Res", "Invalid vpath: %.*s", (int)vpath.size(), vpath.data());
            return {};
        }

        const ResourceID id = MakeResourceID(norm);

        // Look up existing
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            auto it = st->idToIndex.find(id.value);
            if (it != st->idToIndex.end())
            {
                const uint32_t idx = it->second;
                Record* r = st->records[idx].get();
                return ResourceHandle{ idx, r->generation };
            }
        }

        // Create new record (stable index)
        auto rec = std::make_unique<Record>();
        rec->id = id;
        rec->vpathNormalized = norm;
        rec->state.store(ResourceState::Requested, std::memory_order_release);

        uint32_t index = 0;
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            index = (uint32_t)st->records.size();
            st->records.push_back(std::move(rec));
            st->idToIndex.emplace(id.value, index);
        }

        // Enqueue load
        Record* r = st->records[index].get();
        r->state.store(ResourceState::Loading, std::memory_order_release);

        {
            std::lock_guard<std::mutex> lock(st->mtx);
            st->pending.push(PendingReq{ index });
        }
        st->cv.notify_one();

        return ResourceHandle{ index, r->generation };
    }

    void ResourceManager::Update()
    {
        if (!running_ || !state_)
            return;

        auto* st = static_cast<InternalState*>(state_);

        for (;;)
        {
            CompletedReq c{};
            {
                std::lock_guard<std::mutex> lock(st->mtx);
                if (st->completed.empty())
                    break;
                c = std::move(st->completed.front());
                st->completed.pop();
            }

            if (c.index >= st->records.size())
                continue;

            Record& r = *st->records[c.index];

            if (!c.ok)
            {
                r.error = std::move(c.error);
                r.state.store(ResourceState::Failed, std::memory_order_release);
                NOC_LOG_ERROR("Res", "FAILED: %s (%s)", r.vpathNormalized.c_str(), r.error.c_str());
                continue;
            }

            // Replace any existing blob
            if (r.data)
            {
                engine_->Allocator().Deallocate(r.data);
                r.data = nullptr;
                r.size = 0;
            }

            r.size = c.bytes.size();
            if (r.size > 0)
            {
                r.data = static_cast<uint8_t*>(engine_->Allocator().Allocate(r.size, 16));
                std::memcpy(r.data, c.bytes.data(), r.size);
            }

            r.state.store(ResourceState::Ready, std::memory_order_release);
            NOC_LOG_INFO("Res", "READY: %s (%zu bytes)", r.vpathNormalized.c_str(), r.size);
        }
    }

    bool ResourceManager::IsReady(ResourceHandle h) const
    {
        uint32_t idx = 0;
        if (!ValidateHandle_(h, &idx))
            return false;

        auto* st = static_cast<InternalState*>(state_);
        return st->records[idx]->state.load(std::memory_order_acquire) == ResourceState::Ready;
    }

    bool ResourceManager::HasFailed(ResourceHandle h) const
    {
        uint32_t idx = 0;
        if (!ValidateHandle_(h, &idx))
            return true;

        auto* st = static_cast<InternalState*>(state_);
        return st->records[idx]->state.load(std::memory_order_acquire) == ResourceState::Failed;
    }

    const uint8_t* ResourceManager::GetBytes(ResourceHandle h) const
    {
        uint32_t idx = 0;
        if (!ValidateHandle_(h, &idx))
            return nullptr;

        auto* st = static_cast<InternalState*>(state_);
        const Record& r = *st->records[idx];
        if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
            return nullptr;

        return r.data;
    }

    size_t ResourceManager::GetSize(ResourceHandle h) const
    {
        uint32_t idx = 0;
        if (!ValidateHandle_(h, &idx))
            return 0;

        auto* st = static_cast<InternalState*>(state_);
        const Record& r = *st->records[idx];
        if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
            return 0;

        return r.size;
    }

    bool ResourceManager::WaitUntilReady(ResourceHandle h, uint32_t timeoutMs)
    {
        const auto start = std::chrono::steady_clock::now();
        while (true)
        {
            Update();

            if (IsReady(h))
                return true;
            if (HasFailed(h))
                return false;

            if (timeoutMs != 0)
            {
                const auto now = std::chrono::steady_clock::now();
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                if ((uint32_t)elapsed >= timeoutMs)
                    return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void ResourceManager::LoaderThreadMain_()
    {
        NOC_LOG_INFO("Res", "Loader thread running");

        auto* st = static_cast<InternalState*>(state_);

        while (running_)
        {
            PendingReq req{};
            {
                std::unique_lock<std::mutex> lock(st->mtx);
                st->cv.wait(lock, [&]() { return !running_ || !st->pending.empty(); });

                if (!running_)
                    break;

                req = st->pending.front();
                st->pending.pop();
            }

            CompletedReq done{};
            done.index = req.index;

            // Load via Phase 3 VFS API (OpenRead returns FileHandle)
            Record* r = nullptr;
            {
                std::lock_guard<std::mutex> lock(st->mtx);
                if (req.index < st->records.size())
                    r = st->records[req.index].get();
            }

            if (!r)
            {
                done.ok = false;
                done.error = "Invalid record index";
            }
            else
            {
                FileHandle fh = vfs_->OpenRead(r->vpathNormalized);
                if (!fh.valid)
                {
                    done.ok = false;
                    done.error = "OpenRead failed";
                }
                else
                {
                    const uint64_t sz64 = vfs_->Size(fh);
                    const size_t sz = (sz64 > SIZE_MAX) ? SIZE_MAX : (size_t)sz64;

                    done.bytes.resize(sz);

                    if (sz > 0)
                    {
                        const size_t read = vfs_->Read(fh, done.bytes.data(), sz);
                        if (read != sz)
                        {
                            done.ok = false;
                            done.error = "Short read";
                            done.bytes.clear();
                        }
                        else
                        {
                            done.ok = true;
                        }
                    }
                    else
                    {
                        done.ok = true;
                    }

                    vfs_->Close(fh);
                }
            }

            {
                std::lock_guard<std::mutex> lock(st->mtx);
                st->completed.push(std::move(done));
            }
        }

        NOC_LOG_INFO("Res", "Loader thread exiting");
    }

} // namespace noc
```

### Listing 55 — `Phase 4 — Resource Manager.md` — Implementations > `Engine/Runtime/Engine.h` (updated)
```cpp
#pragma once
#include "Core/BuildConfig.h"
#include "Core/Subsystems/SubsystemRegistry.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/LinearArena.h"

#if NOC_ENABLE_ASSERTS
#include "Core/Memory/DebugAlloc.h"
#endif

#include "Resources/VirtualFileSystem.h"
#include "Resources/ResourceManager.h"
#include "EngineConfig.h"

namespace noc {

    class WinWindow;
    class MainLoop;

    class Engine
    {
    public:
        // Config access: only valid BEFORE Init().
        EngineConfig& ConfigMutable();
        const EngineConfig& Config() const { return cfg_; }

        bool SetContentRoot(const char* path);
        bool SetOverrideRoot(const char* path);
        bool SetArchivePath(const char* path);

        bool Init();
        void TickOnce();
        void Shutdown();

        int Run();
        void BeginFrame();
        void Tick();
        void EndFrame();

        IAllocator& Allocator();
        LinearArena& FrameArena();

        VirtualFileSystem& VFS() { return vfs_; }
        ResourceManager& Resources() { return resources_; }

        bool InitMemory();
        void KillMemory();

    private:
        bool IsConfigMutable() const { return !initialized_; }

    private:
        SubsystemRegistry registry_;

        MallocAllocator baseAlloc_;

#if NOC_ENABLE_ASSERTS
        DebugAlloc debugAlloc_{ baseAlloc_ };
        IAllocator* alloc_ = &debugAlloc_;
#else
        IAllocator* alloc_ = &baseAlloc_;
#endif

        void* frameArenaMem_ = nullptr;
        LinearArena frameArena_;

        VirtualFileSystem vfs_;
        ResourceManager resources_;

        EngineConfig cfg_{};
        bool initialized_ = false;
    };

} // namespace noc
```

### Listing 56 — `Phase 4 — Resource Manager.md` — Implementations > `Engine/Runtime/Engine.cpp` (updated)
```cpp
#include "Engine.h"

#include <cstring>

#include "Core/Log.h"
#include "Core/Assert.h"
#include "Core/Clock.h"
#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h"

namespace noc {

    IAllocator& Engine::Allocator() { return *alloc_; }
    LinearArena& Engine::FrameArena() { return frameArena_; }

    EngineConfig& Engine::ConfigMutable()
    {
        NOC_ASSERT_MSG(IsConfigMutable(), "Engine config is immutable after Engine::Init()");
        return cfg_;
    }

    static bool SetCString_(const char*& dst, const char* src)
    {
        dst = src;
        return true;
    }

    bool Engine::SetContentRoot(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_WARN("Core", "SetContentRoot ignored (called after Init)");
            return false;
        }
        return SetCString_(cfg_.contentRoot, path);
    }

    bool Engine::SetOverrideRoot(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_WARN("Core", "SetOverrideRoot ignored (called after Init)");
            return false;
        }
        return SetCString_(cfg_.overrideRoot, path);
    }

    bool Engine::SetArchivePath(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_WARN("Core", "SetArchivePath ignored (called after Init)");
            return false;
        }
        return SetCString_(cfg_.archivePath, path);
    }

    bool Engine::InitMemory()
    {
        constexpr std::size_t kFrameArenaBytes = 8 * 1024 * 1024; // Design choice
        frameArenaMem_ = Allocator().Allocate(kFrameArenaBytes, 64);
        frameArena_.Init(frameArenaMem_, kFrameArenaBytes);

        NOC_LOG_INFO("Core", "Memory system initialized (frame arena=%zu bytes)", kFrameArenaBytes);
        return true;
    }

    void Engine::KillMemory()
    {
        if (frameArenaMem_)
        {
            Allocator().Deallocate(frameArenaMem_);
            frameArenaMem_ = nullptr;
        }

#if NOC_ENABLE_ASSERTS
        NOC_LOG_INFO("Core", "Memory stats: total=%zu outstanding=%zu allocs=%zu",
            debugAlloc_.TotalAllocatedBytes(),
            debugAlloc_.OutstandingBytes(),
            debugAlloc_.AllocationCount());
#endif
    }

    static bool StartupLog(void*)
    {
        noc::GetLogger().Init();
        NOC_LOG_INFO("Core", "Logger initialized");
        return true;
    }

    static void ShutdownLog(void*)
    {
        NOC_LOG_INFO("Core", "Logger shutting down");
        noc::GetLogger().Shutdown();
    }

    static bool StartupTime(void*)
    {
        noc::GetTime().Init();
        NOC_LOG_INFO("Core", "Time system initialized");
        return true;
    }

    static void ShutdownTime(void*)
    {
        NOC_LOG_INFO("Core", "Time system shutting down");
    }

    static bool StartupAssert(void*)
    {
        NOC_LOG_INFO("Core", "Assert system initialized");
        return true;
    }

    static void ShutdownAssert(void*)
    {
        NOC_LOG_INFO("Core", "Assert system shutting down");
    }

    static bool StartupMemory(void* ctx)
    {
        auto* e = static_cast<noc::Engine*>(ctx);
        return e->InitMemory();
    }

    static void ShutdownMemory(void* ctx)
    {
        NOC_LOG_INFO("Core", "Memory system shutting down");
        auto* e = static_cast<noc::Engine*>(ctx);
        e->KillMemory();
    }

    static bool StartupWindow(void*)
    {
        NOC_LOG_INFO("Win32", "Window subsystem ready");
        return true;
    }

    static void ShutdownWindow(void*)
    {
        NOC_LOG_INFO("Win32", "Window subsystem shutdown");
    }

    bool Engine::Init()
    {
        // Freeze config from this point on.
        initialized_ = true;

        std::span<const char* const> depsLog{};
        static const char* kDepsNeedLog[] = { "Log" };
        std::span<const char* const> depsNeedLog{ kDepsNeedLog, 1 };

        registry_.Register(SubsystemDesc{ "Log",    depsLog,     &StartupLog,    &ShutdownLog });
        registry_.Register(SubsystemDesc{ "Time",   depsNeedLog, &StartupTime,   &ShutdownTime });
        registry_.Register(SubsystemDesc{ "Memory", depsNeedLog, &StartupMemory, &ShutdownMemory });
        registry_.Register(SubsystemDesc{ "Assert", depsNeedLog, &StartupAssert, &ShutdownAssert });
        registry_.Register(SubsystemDesc{ "Window", depsNeedLog, &StartupWindow, &ShutdownWindow });

        if (!registry_.StartupAll(this))
            return false;

        // Phase 3 policy (locked): later mounts override earlier mounts.
        if (cfg_.archivePath && cfg_.archivePath[0] != 0)
        {
            if (!vfs_.MountArchive(cfg_.archivePath))
                NOC_LOG_WARN("VFS", "Failed to mount archivePath: %s", cfg_.archivePath);
        }

        if (cfg_.contentRoot && cfg_.contentRoot[0] != 0)
        {
            if (!vfs_.MountLooseDirectory(cfg_.contentRoot))
                NOC_LOG_WARN("VFS", "Failed to mount contentRoot: %s", cfg_.contentRoot);
        }

        if (cfg_.overrideRoot && cfg_.overrideRoot[0] != 0)
        {
            if (!vfs_.MountLooseDirectory(cfg_.overrideRoot))
                NOC_LOG_WARN("VFS", "Failed to mount overrideRoot: %s", cfg_.overrideRoot);
        }

        // Phase 4: ResourceManager init (must be after VFS mounts).
        if (!resources_.Init(*this, vfs_))
            return false;

        return true;
    }

    int Engine::Run()
    {
        WinWindow window;
        WinWindowDesc wd{};
        wd.title = L"NocturneHost";
        wd.width = 1280;
        wd.height = 720;
        wd.resizable = true;

        if (!window.Create(wd))
        {
            NOC_LOG_FATAL("Win32", "Failed to create window");
            return -1;
        }

        MainLoop loop;
        loop.Run(*this, window);

        window.Destroy();
        return 0;
    }

    void Engine::BeginFrame()
    {
        GetTime().BeginFrame();
        FrameArena().Reset();
    }

    void Engine::Tick()
    {
        // Phase 4: pump completed resource loads.
        resources_.Update();
    }

    void Engine::EndFrame()
    {
        GetTime().EndFrame();
    }

    void Engine::TickOnce()
    {
        GetTime().BeginFrame();
        FrameArena().Reset();

        void* a = FrameArena().Allocate(256, 16);
        void* b = FrameArena().Allocate(1024, 64);
        (void)a; (void)b;

        GetTime().EndFrame();

        NOC_LOG_INFO("Core", "TickOnce() dt=%.6f sec arenaUsed=%zu bytes",
            GetTime().DeltaSeconds(),
            FrameArena().Used());
    }

    void Engine::Shutdown()
    {
        // Phase 4: shutdown resource manager BEFORE core teardown.
        resources_.Shutdown();

        registry_.ShutdownAll(this);
    }

} // namespace noc
```

### Listing 57 — `Phase 4 — Resource Manager.md` — Implementations > `Apps/NocturneHost/main.cpp` (updated)
```cpp
#include "Runtime/Engine.h"
#include "Core/Log.h"

#include <algorithm>
#include <cctype>
#include <string>

#ifndef NOC_CONTENT_ROOT
#define NOC_CONTENT_ROOT "Data"
#endif

static void LogPreview(const uint8_t* bytes, size_t size)
{
    if (!bytes || size == 0)
    {
        NOC_LOG_INFO("Host", "File empty");
        return;
    }

    const size_t n = std::min<size_t>(size, 64);
    std::string s;
    s.reserve(n);

    for (size_t i = 0; i < n; ++i)
    {
        const char c = static_cast<char>(bytes[i]);
        s.push_back((std::isprint((unsigned char)c) ? c : '.'));
    }

    NOC_LOG_INFO("Host", "Preview (first %zu bytes): %s", n, s.c_str());
}

int main()
{
    noc::Engine engine;

    // Phase 3 config defaults to contentRoot="Data".
    // If your repo uses a different layout, you can override here before Init():
     engine.SetContentRoot(NOC_CONTENT_ROOT);
    // engine.SetOverrideRoot("DataOverrides");
    // engine.SetArchivePath("Data.pak");

    if (!engine.Init())
        return -1;

    // Phase 4 smoke test (host-side): load hello.txt via ResourceManager.
    auto h = engine.Resources().RequestBinary("hello.txt");
    if (!h.IsValid())
    {
        NOC_LOG_ERROR("Host", "Failed to request hello.txt");
    }
    else
    {
        const bool ok = engine.Resources().WaitUntilReady(h, /*timeoutMs*/ 2000);
        if (!ok)
        {
            NOC_LOG_ERROR("Host", "hello.txt did not become ready (failed or timed out)");
        }
        else
        {
            const uint8_t* bytes = engine.Resources().GetBytes(h);
            const size_t size = engine.Resources().GetSize(h);
            NOC_LOG_INFO("Host", "hello.txt loaded (%zu bytes)", size);
            LogPreview(bytes, size);
        }
    }

    auto h1 = engine.Resources().RequestBinary("hello.txt");
    auto h2 = engine.Resources().RequestBinary("hello.txt");

    NOC_LOG_INFO("Host", "Handle1 = (%u, %u)", h1.index, h1.generation);
    NOC_LOG_INFO("Host", "Handle2 = (%u, %u)", h2.index, h2.generation);


    const int rc = engine.Run();

    engine.Shutdown();
    return rc;
}
```

### Listing 58 — `Phase 5 — Typed Resources & Loader Registry.md` — Implementations > Engine/Resources/Typed/ResourceType.h
```cpp
#pragma once
#include <cstdint>

namespace noc {

enum class ResourceType : uint8_t {
    Unknown = 0,
    Binary,
    Text,
    // Future:
    // Json,
    // Image,
    // Mesh,
};

} // namespace noc
````

### Engine/Resources/Typed/ResourceHandleT.h

```cpp
#pragma once
#include "Engine/Resources/ResourceHandle.h"

namespace noc {

// Strongly-typed wrapper around an untyped ResourceHandle.
template <class T>
class ResourceHandleT {
public:
    ResourceHandleT() = default;
    explicit ResourceHandleT(ResourceHandle h) : h_(h) {}

    ResourceHandle Untyped() const { return h_; }
    bool IsValid() const { return h_.IsValid(); }

    friend bool operator==(const ResourceHandleT& a, const ResourceHandleT& b) { return a.h_ == b.h_; }
    friend bool operator!=(const ResourceHandleT& a, const ResourceHandleT& b) { return !(a == b); }

private:
    ResourceHandle h_{};
};

} // namespace noc
```

### Listing 59 — `Phase 5 — Typed Resources & Loader Registry.md` — Implementations > Engine/Resources/Typed/IResourceLoader.h
```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "Engine/Resources/Typed/ResourceType.h"

namespace noc {

struct ResourceLoadResult {
    bool ok = false;
    std::string error;

    // Type-erased pointer to decoded runtime object.
    // Owned by ResourceManager after completion; must be heap allocated.
    void* object = nullptr;
};

class IResourceLoader {
public:
    virtual ~IResourceLoader() = default;

    virtual ResourceType Type() const = 0;

    // Optional: basic sniffing based on vpath extension, etc.
    // In Phase 5 we route primarily by ResourceType at request time.
    virtual bool CanLoad(const char* /*normalizedVPath*/) const { return true; }

    // Runs on the loader thread in Phase 5.
    // Must be CPU-only and thread-safe.
    virtual ResourceLoadResult Decode(const uint8_t* bytes, size_t size) = 0;
};

} // namespace noc
```

### Listing 60 — `Phase 5 — Typed Resources & Loader Registry.md` — Implementations > Engine/Resources/Typed/ResourceLoaderRegistry.h
```cpp
#pragma once
#include <array>
#include <cstddef>

#include "Engine/Resources/Typed/IResourceLoader.h"
#include "Engine/Resources/Typed/ResourceType.h"

namespace noc {

class ResourceLoaderRegistry {
public:
    ResourceLoaderRegistry() { loaders_.fill(nullptr); }

    bool RegisterLoader(IResourceLoader* loader) {
        if (!loader) return false;
        const auto t = loader->Type();
        const auto idx = static_cast<size_t>(t);
        if (idx >= loaders_.size()) return false;
        loaders_[idx] = loader;
        return true;
    }

    IResourceLoader* FindLoader(ResourceType t) const {
        const auto idx = static_cast<size_t>(t);
        if (idx >= loaders_.size()) return nullptr;
        return loaders_[idx];
    }

private:
    // Small fixed table; expand as ResourceType grows.
    std::array<IResourceLoader*, 16> loaders_{};
};

} // namespace noc
```

### Listing 61 — `Phase 5 — Typed Resources & Loader Registry.md` — Implementations > Engine/Resources/Typed/TextResource.h
```cpp
#pragma once
#include <string>
#include <string_view>

namespace noc {

class TextResource {
public:
    explicit TextResource(std::string s) : text_(std::move(s)) {}

    std::string_view View() const { return text_; }
    const std::string& Str() const { return text_; }
    size_t Size() const { return text_.size(); }

private:
    std::string text_;
};

} // namespace noc
```

### Listing 62 — `Phase 5 — Typed Resources & Loader Registry.md` — Implementations > Engine/Resources/Typed/TextResourceLoader.h
```cpp
#pragma once
#include "Engine/Resources/Typed/IResourceLoader.h"
#include "Engine/Resources/Typed/TextResource.h"

namespace noc {

class TextResourceLoader final : public IResourceLoader {
public:
    ResourceType Type() const override { return ResourceType::Text; }

    ResourceLoadResult Decode(const uint8_t* bytes, size_t size) override {
        ResourceLoadResult r{};
        if (!bytes && size != 0) { r.ok = false; r.error = "Text decode: null bytes"; return r; }

        // Design choice (not directly from the book):
        // Treat input bytes as UTF-8 without validation (good enough for Phase 5).
        // Later: validate UTF-8 and/or support UTF-16.
        std::string s(reinterpret_cast<const char*>(bytes), reinterpret_cast<const char*>(bytes) + size);

        // Heap allocate runtime object; ResourceManager takes ownership.
        r.object = new TextResource(std::move(s));
        r.ok = true;
        return r;
    }
};

} // namespace noc
```

### Listing 63 — `Phase 5 — Typed Resources & Loader Registry.md` — Implementations > Engine/Resources/ResourceManager.h
```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "Resources/ResourceHandle.h"
#include "Resources/Typed/ResourceLoaderRegistry.h"
#include "Resources/Typed/ResourceHandleT.h"
#include "Resources/Typed/TextResource.h"
#include "Resources/Typed/ResourceType.h"

namespace noc
{
    class Engine;
    class VirtualFileSystem;

    class ResourceManager
    {
    public:
        ResourceManager() = default;
        ~ResourceManager();

        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;

        bool Init(Engine& engine, VirtualFileSystem& vfs);
        void Shutdown();

        // Call once per frame on the main thread.
        void Update();

        // ---- Binary blob API (Phase 4 scope) ----
        ResourceHandle RequestBinary(std::string_view vpath);

        bool IsReady(ResourceHandle h) const;
        bool HasFailed(ResourceHandle h) const;

        const uint8_t* GetBytes(ResourceHandle h) const;
        size_t GetSize(ResourceHandle h) const;

        // Returns nullptr if:
        // - handle invalid
        // - resource has not failed
        // Otherwise returns a stable, null-terminated error message.
        const char* GetError(ResourceHandle h) const;

        // Optional helper for host-side testing.
        bool WaitUntilReady(ResourceHandle h, uint32_t timeoutMs);

        ResourceHandleT<TextResource> RequestText(const char* vpath);

        const TextResource* GetText(ResourceHandleT<TextResource> h) const;

        ResourceLoaderRegistry& Loaders() { return loaders_; }
        const ResourceLoaderRegistry& Loaders() const { return loaders_; }

    private:
        bool ValidateHandle_(ResourceHandle h, uint32_t* outIndex) const;
        void LoaderThreadMain_();

    private:
        Engine* engine_ = nullptr;
        VirtualFileSystem* vfs_ = nullptr;

        // Internal opaque state (allocated in .cpp)
        void* state_ = nullptr;

        void* loaderThread_ = nullptr;
        bool running_ = false;

        ResourceLoaderRegistry loaders_;
    };

} // namespace noc
```

### Listing 64 — `Phase 5 — Typed Resources & Loader Registry.md` — Implementations > Engine/Resources/ResourceManager.cpp (Phase 5 additions/updates)
```cpp
#include "ResourceManager.h"

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <memory>
#include <vector>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Resources/ResourceID.h"
#include "Resources/VirtualFileSystem.h"
#include "Resources/VPath.h"
#include "Resources/FileHandle.h"
#include "Runtime/Engine.h"
#include "Resources/Typed/TextResource.h"
#include "Resources/Typed/TextResourceLoader.h"

namespace noc
{
	static TextResourceLoader g_textLoader;

	enum class ResourceState : uint8_t
	{
		Empty = 0,
		Requested,
		Loading,
		Ready,
		Failed
	};

	struct Record
	{
		ResourceID id{};
		std::string vpathNormalized;
		std::atomic<ResourceState> state{ ResourceState::Empty };

		uint32_t generation = 1;

		uint8_t* data = nullptr;
		size_t size = 0;

		std::string error;

		// -------- Phase 5 additions --------
		ResourceType type = ResourceType::Binary; // default matches Phase 4 behavior
		void* typedObject = nullptr;              // points to decoded runtime object when READY

		Record() = default;
		Record(const Record&) = delete;
		Record& operator=(const Record&) = delete;
		Record(Record&&) = delete;
		Record& operator=(Record&&) = delete;
	};


	struct PendingReq { uint32_t index = 0; };

	struct CompletedReq
	{
		uint32_t index = 0;
		bool ok = false;

		// Phase 5: what kind of payload is being completed?
		ResourceType type = ResourceType::Binary;

		// Binary payload (Phase 4)
		std::vector<uint8_t> bytes;

		// Typed payload (Phase 5): heap object produced by loader Decode()
		void* typedObject = nullptr;

		std::string error;
	};


	struct InternalState
	{
		std::mutex mtx;
		std::condition_variable cv;
		std::queue<PendingReq> pending;
		std::queue<CompletedReq> completed;

		std::unordered_map<uint64_t, uint32_t> idToIndex;

		std::vector<std::unique_ptr<Record>> records;
	};

	static std::string NormalizeVPath_(std::string_view vpath)
	{
		char buf[512]{};
		if (!noc::vpath::NormalizeToRelative(buf, sizeof(buf), vpath))
			return {};
		return std::string(buf);
	}

	static uint64_t MakeCacheKey_(uint64_t idValue, ResourceType type)
	{
		// Combine id + type into a single stable key.
		// idValue is already a hash; xor-mix the type.
		return idValue ^ (static_cast<uint64_t>(type) * 0x9E3779B97F4A7C15ull);
	}

	ResourceManager::~ResourceManager()
	{
		Shutdown();
	}

	bool ResourceManager::Init(Engine& engine, VirtualFileSystem& vfs)
	{
		if (running_)
			return true;

		engine_ = &engine;
		vfs_ = &vfs;

		auto* st = new InternalState();
		st->records.reserve(256);
		state_ = st;

		// Register built-in loaders
		loaders_.RegisterLoader(&g_textLoader);

		running_ = true;
		loaderThread_ = new std::thread([this]() { LoaderThreadMain_(); });

		NOC_LOG_INFO("Res", "ResourceManager initialized (loader thread started)");
		return true;
	}

	void ResourceManager::Shutdown()
	{
		if (!running_)
			return;

		running_ = false;

		auto* st = static_cast<InternalState*>(state_);
		st->cv.notify_all();

		if (loaderThread_)
		{
			auto* t = static_cast<std::thread*>(loaderThread_);
			if (t->joinable())
				t->join();
			delete t;
			loaderThread_ = nullptr;
		}

		// Free blobs
		for (auto& rp : st->records)
		{
			if (!rp) continue;

			if (rp->typedObject)
			{
				if (rp->type == ResourceType::Text)
					delete static_cast<TextResource*>(rp->typedObject);

				rp->typedObject = nullptr;
			}

			if (rp->data)
			{
				engine_->Allocator().Deallocate(rp->data);
				rp->data = nullptr;
				rp->size = 0;
			}
		}


		delete st;
		state_ = nullptr;

		engine_ = nullptr;
		vfs_ = nullptr;

		NOC_LOG_INFO("Res", "ResourceManager shutdown complete");
	}

	bool ResourceManager::ValidateHandle_(ResourceHandle h, uint32_t* outIndex) const
	{
		if (!h.IsValid() || !state_)
			return false;

		auto* st = static_cast<InternalState*>(state_);
		if (h.index >= st->records.size())
			return false;

		const Record* r = st->records[h.index].get();
		if (!r)
			return false;

		if (r->generation != h.generation)
			return false;

		*outIndex = h.index;
		return true;
	}

	ResourceHandle ResourceManager::RequestBinary(std::string_view vpath)
	{
		if (!running_ || !state_)
			return {};

		auto* st = static_cast<InternalState*>(state_);

		const std::string norm = NormalizeVPath_(vpath);
		if (norm.empty())
		{
			NOC_LOG_ERROR("Res", "Invalid vpath: %.*s", (int)vpath.size(), vpath.data());
			return {};
		}

		const ResourceID id = MakeResourceID(norm);

		// Get key
		const uint64_t key = MakeCacheKey_(id.value, ResourceType::Binary);

		// Look up existing
		{
			std::lock_guard<std::mutex> lock(st->mtx);

			auto it = st->idToIndex.find(key);
			if (it != st->idToIndex.end())
			{
				const uint32_t idx = it->second;
				Record* r = st->records[idx].get();
				return ResourceHandle{ idx, r->generation };
			}
		}

		// Create new record (stable index)
		auto rec = std::make_unique<Record>();
		rec->id = id;
		rec->vpathNormalized = norm;
		rec->type = ResourceType::Binary;
		rec->typedObject = nullptr;
		rec->state.store(ResourceState::Requested, std::memory_order_release);

		uint32_t index = 0;
		{
			std::lock_guard<std::mutex> lock(st->mtx);
			index = (uint32_t)st->records.size();
			st->records.push_back(std::move(rec));
			st->idToIndex.emplace(key, index);
		}

		// Enqueue load
		Record* r = st->records[index].get();
		r->state.store(ResourceState::Loading, std::memory_order_release);

		{
			std::lock_guard<std::mutex> lock(st->mtx);
			st->pending.push(PendingReq{ index });
		}
		st->cv.notify_one();

		return ResourceHandle{ index, r->generation };
	}

	void ResourceManager::Update()
	{
		if (!running_ || !state_)
			return;

		auto* st = static_cast<InternalState*>(state_);

		for (;;)
		{
			CompletedReq c{};
			{
				std::lock_guard<std::mutex> lock(st->mtx);
				if (st->completed.empty())
					break;
				c = std::move(st->completed.front());
				st->completed.pop();
			}

			if (c.index >= st->records.size())
				continue;

			// Lock while mutating record's non-atomic fields (string, pointers, sizes).
			std::lock_guard<std::mutex> lock(st->mtx);

			Record& r = *st->records[c.index];

			if (!c.ok)
			{
				if (c.typedObject)
				{
					// Phase 5 currently only has TextResource typed objects.
					delete static_cast<TextResource*>(c.typedObject);
					c.typedObject = nullptr;
				}

				r.error = std::move(c.error);
				r.state.store(ResourceState::Failed, std::memory_order_release);
				NOC_LOG_ERROR("Res", "FAILED: %s (%s)", r.vpathNormalized.c_str(), r.error.c_str());
				continue;
			}

			// success
			r.error.clear();

			if (c.type == ResourceType::Binary)
			{
				if (r.data)
				{
					engine_->Allocator().Deallocate(r.data);
					r.data = nullptr;
					r.size = 0;
				}

				r.size = c.bytes.size();
				if (r.size > 0)
				{
					r.data = static_cast<uint8_t*>(engine_->Allocator().Allocate(r.size, 16));
					std::memcpy(r.data, c.bytes.data(), r.size);
				}
			}
			else if (c.type == ResourceType::Text)
			{
				if (r.typedObject)
				{
					delete static_cast<TextResource*>(r.typedObject);
					r.typedObject = nullptr;
				}

				r.typedObject = c.typedObject;
				c.typedObject = nullptr;
			}

			r.state.store(ResourceState::Ready, std::memory_order_release);
			NOC_LOG_INFO("Res", "READY: %s", r.vpathNormalized.c_str());

		}
	}


	bool ResourceManager::IsReady(ResourceHandle h) const
	{
		uint32_t idx = 0;
		if (!ValidateHandle_(h, &idx))
			return false;

		auto* st = static_cast<InternalState*>(state_);
		return st->records[idx]->state.load(std::memory_order_acquire) == ResourceState::Ready;
	}

	bool ResourceManager::HasFailed(ResourceHandle h) const
	{
		uint32_t idx = 0;
		if (!ValidateHandle_(h, &idx))
			return true;

		auto* st = static_cast<InternalState*>(state_);
		return st->records[idx]->state.load(std::memory_order_acquire) == ResourceState::Failed;
	}

	const uint8_t* ResourceManager::GetBytes(ResourceHandle h) const
	{
		uint32_t idx = 0;
		if (!ValidateHandle_(h, &idx))
			return nullptr;

		auto* st = static_cast<InternalState*>(state_);
		const Record& r = *st->records[idx];
		if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
			return nullptr;

		return r.data;
	}

	size_t ResourceManager::GetSize(ResourceHandle h) const
	{
		uint32_t idx = 0;
		if (!ValidateHandle_(h, &idx))
			return 0;

		auto* st = static_cast<InternalState*>(state_);
		const Record& r = *st->records[idx];
		if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
			return 0;

		return r.size;
	}

	const char* ResourceManager::GetError(ResourceHandle h) const
	{
		uint32_t idx = 0;
		if (!ValidateHandle_(h, &idx))
			return nullptr;

		auto* st = static_cast<InternalState*>(state_);

		// The error string is a std::string and is mutated during Update() on failure,
		// so we must guard access with the same mutex that protects record access.
		std::lock_guard<std::mutex> lock(st->mtx);

		const Record& r = *st->records[idx];
		if (r.state.load(std::memory_order_acquire) != ResourceState::Failed)
			return nullptr;

		if (r.error.empty())
			return nullptr;

		return r.error.c_str();
	}



	bool ResourceManager::WaitUntilReady(ResourceHandle h, uint32_t timeoutMs)
	{
		const auto start = std::chrono::steady_clock::now();
		while (true)
		{
			Update();

			if (IsReady(h))
				return true;
			if (HasFailed(h))
				return false;

			if (timeoutMs != 0)
			{
				const auto now = std::chrono::steady_clock::now();
				const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
				if ((uint32_t)elapsed >= timeoutMs)
					return false;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	ResourceHandleT<TextResource> ResourceManager::RequestText(const char* vpath)
	{
		if (!running_ || !state_)
			return {};

		auto* st = static_cast<InternalState*>(state_);

		const std::string norm = NormalizeVPath_(std::string_view{ vpath ? vpath : "" });
		if (norm.empty())
		{
			NOC_LOG_ERROR("Res", "Invalid vpath: %s", vpath ? vpath : "(null)");
			return {};
		}

		const ResourceID id = MakeResourceID(norm);
		const uint64_t key = MakeCacheKey_(id.value, ResourceType::Text);

		// Look up existing
		{
			std::lock_guard<std::mutex> lock(st->mtx);
			auto it = st->idToIndex.find(key);
			if (it != st->idToIndex.end())
			{
				const uint32_t idx = it->second;
				Record* r = st->records[idx].get();
				return ResourceHandleT<TextResource>(ResourceHandle{ idx, r->generation });
			}
		}

		// Ensure loader exists (Phase 5 contract)
		if (!loaders_.FindLoader(ResourceType::Text))
		{
			NOC_LOG_ERROR("Res", "RequestText: no loader registered for ResourceType::Text");
			return {};
		}

		// Create record
		auto rec = std::make_unique<Record>();
		rec->id = id;
		rec->vpathNormalized = norm;
		rec->type = ResourceType::Text;
		rec->typedObject = nullptr;
		rec->state.store(ResourceState::Requested, std::memory_order_release);

		uint32_t index = 0;
		{
			std::lock_guard<std::mutex> lock(st->mtx);
			index = (uint32_t)st->records.size();
			st->records.push_back(std::move(rec));
			st->idToIndex.emplace(key, index);
		}

		// Enqueue load
		Record* r = st->records[index].get();
		r->state.store(ResourceState::Loading, std::memory_order_release);

		{
			std::lock_guard<std::mutex> lock(st->mtx);
			st->pending.push(PendingReq{ index });
		}
		st->cv.notify_one();

		return ResourceHandleT<TextResource>(ResourceHandle{ index, r->generation });
	}

	const TextResource* ResourceManager::GetText(ResourceHandleT<TextResource> h) const
	{
		if (!running_ || !state_)
			return nullptr;

		const ResourceHandle uh = h.Untyped();

		uint32_t idx = 0;
		if (!ValidateHandle_(uh, &idx))
			return nullptr;

		auto* st = static_cast<InternalState*>(state_);
		const Record& r = *st->records[idx];

		if (r.type != ResourceType::Text)
			return nullptr;

		if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
			return nullptr;

		return static_cast<const TextResource*>(r.typedObject);
	}


	void ResourceManager::LoaderThreadMain_()
	{
		NOC_LOG_INFO("Res", "Loader thread running");

		auto* st = static_cast<InternalState*>(state_);

		while (running_)
		{
			PendingReq req{};
			{
				std::unique_lock<std::mutex> lock(st->mtx);
				st->cv.wait(lock, [&]() { return !running_ || !st->pending.empty(); });

				if (!running_)
					break;

				req = st->pending.front();
				st->pending.pop();
			}

			CompletedReq done{};
			done.index = req.index;

			// Load via Phase 3 VFS API (OpenRead returns FileHandle)
			Record* r = nullptr;
			{
				std::lock_guard<std::mutex> lock(st->mtx);
				if (req.index < st->records.size())
					r = st->records[req.index].get();
			}

			if (!r)
			{
				done.ok = false;
				done.error = "Invalid record index";
			}
			else
			{
				FileHandle fh = vfs_->OpenRead(r->vpathNormalized);
				if (!fh.valid)
				{
					done.ok = false;
					done.error = "OpenRead failed";
				}
				else
				{
					const uint64_t sz64 = vfs_->Size(fh);
					const size_t sz = (sz64 > SIZE_MAX) ? SIZE_MAX : (size_t)sz64;

					done.bytes.resize(sz);

					if (sz > 0)
					{
						const size_t read = vfs_->Read(fh, done.bytes.data(), sz);
						if (read != sz)
						{
							done.ok = false;
							done.error = "Short read";
							done.bytes.clear();
						}
						else
						{
							done.ok = true;
							done.type = r->type;

							if (done.ok && r->type != ResourceType::Binary)
							{
								IResourceLoader* loader = loaders_.FindLoader(r->type);
								if (!loader)
								{
									done.ok = false;
									done.error = "No loader registered for requested type";
									done.bytes.clear();
								}
								else
								{
									const ResourceLoadResult rr = loader->Decode(done.bytes.data(), done.bytes.size());
									if (!rr.ok)
									{
										done.ok = false;
										done.error = rr.error;
										done.bytes.clear();
									}
									else
									{
										done.typedObject = rr.object;
										done.bytes.clear(); // no longer needed after decoding
									}
								}
							}

						}
					}
					else
					{
						done.ok = true;
					}

					vfs_->Close(fh);
				}
			}

			{
				std::lock_guard<std::mutex> lock(st->mtx);
				st->completed.push(std::move(done));
			}
		}

		NOC_LOG_INFO("Res", "Loader thread exiting");
	}

} // namespace noc
````

### NocturneHost (Phase 5 test)

Minimum acceptance behavior:

* request `"hello.txt"` as Text
* wait until ready
* log size + preview

(Integrate into wherever you already tested `RequestBinary` in Phase 4.) 

Pseudo-usage:

```cpp
auto h = engine.Resources().RequestText("hello.txt");
...
engine.Resources().Update(); // already called by Engine::Tick()
if (engine.Resources().IsReady(h.Untyped())) {
    const auto* txt = engine.Resources().GetText(h);
    NOC_LOG_INFO("Text bytes=%zu preview='%.64s'", txt->Size(), std::string(txt->View().substr(0,64)).c_str());
}
```

### Listing 65 — `Phase 6 — Job System & Async Infrastructure.md` — `Engine/Core/Jobs/JobSystem.h`
```cpp
#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace noc {

class JobSystem {
public:
    using JobFn = std::function<void()>;

    JobSystem() = default;
    ~JobSystem() { Shutdown(); }

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    bool Init(uint32_t workerCount);
    void Shutdown();

    // Fire-and-forget jobs. (Phase 6 scope)
    void Enqueue(JobFn fn);

    // For orderly shutdown / subsystems that need to wait.
    void WaitIdle();

    uint32_t WorkerCount() const { return workerCount_; }
    bool IsRunning() const { return running_.load(std::memory_order_acquire); }

private:
    void WorkerMain_(uint32_t workerIndex);

private:
    std::atomic<bool> running_{ false };

    std::mutex mtx_;
    std::condition_variable cvWork_;
    std::condition_variable cvIdle_;
    std::queue<JobFn> queue_;

    std::atomic<uint32_t> inflight_{ 0 };

    std::vector<std::thread> workers_;
    uint32_t workerCount_ = 0;
};

} // namespace noc
```

### Listing 66 — `Phase 6 — Job System & Async Infrastructure.md` — `Engine/Core/Jobs/JobSystem.cpp`
```cpp
#include "JobSystem.h"

#include "Core/Log.h"

#include <algorithm>

namespace noc {

bool JobSystem::Init(uint32_t workerCount)
{
    if (running_.load(std::memory_order_acquire))
        return true;

    // Design choice (not directly from the book): clamp worker count.
    if (workerCount == 0)
        workerCount = std::max(1u, std::thread::hardware_concurrency());

    workerCount_ = workerCount;

    running_.store(true, std::memory_order_release);

    workers_.reserve(workerCount_);
    for (uint32_t i = 0; i < workerCount_; ++i)
        workers_.emplace_back([this, i]() { WorkerMain_(i); });

    NOC_LOG_INFO("Jobs", "JobSystem initialized (%u workers)", workerCount_);
    return true;
}

void JobSystem::Shutdown()
{
    if (!running_.load(std::memory_order_acquire))
        return;

    WaitIdle();

    running_.store(false, std::memory_order_release);
    cvWork_.notify_all();

    for (auto& t : workers_)
    {
        if (t.joinable())
            t.join();
    }
    workers_.clear();

    // Drain anything that might remain (should be empty after WaitIdle).
    {
        std::lock_guard<std::mutex> lock(mtx_);
        while (!queue_.empty())
            queue_.pop();
    }

    NOC_LOG_INFO("Jobs", "JobSystem shutdown complete");
}

void JobSystem::Enqueue(JobFn fn)
{
    if (!fn)
        return;

    if (!running_.load(std::memory_order_acquire))
    {
        // If jobs are not running, execute inline (safe fallback).
        fn();
        return;
    }

    inflight_.fetch_add(1, std::memory_order_acq_rel);

    {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.push(std::move(fn));
    }

    cvWork_.notify_one();
}

void JobSystem::WaitIdle()
{
    // Wait until there are no inflight jobs and queue is empty.
    std::unique_lock<std::mutex> lock(mtx_);
    cvIdle_.wait(lock, [&]() {
        return queue_.empty() && inflight_.load(std::memory_order_acquire) == 0;
    });
}

void JobSystem::WorkerMain_(uint32_t workerIndex)
{
    (void)workerIndex;

    for (;;)
    {
        JobFn job;

        {
            std::unique_lock<std::mutex> lock(mtx_);
            cvWork_.wait(lock, [&]() {
                return !running_.load(std::memory_order_acquire) || !queue_.empty();
            });

            if (!running_.load(std::memory_order_acquire) && queue_.empty())
                break;

            if (!queue_.empty())
            {
                job = std::move(queue_.front());
                queue_.pop();
            }
        }

        if (job)
            job();

        const uint32_t left = inflight_.fetch_sub(1, std::memory_order_acq_rel) - 1;

        // Notify anyone waiting for idle when we *might* have reached it.
        if (left == 0)
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (queue_.empty())
                cvIdle_.notify_all();
        }
    }
}

} // namespace noc
```

### Listing 67 — `Phase 6 — Job System & Async Infrastructure.md` — `Engine/Runtime/Engine.h`
```cpp
#pragma once
#include "Core/BuildConfig.h"
#include "Core/Subsystems/SubsystemRegistry.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/LinearArena.h"

#if NOC_ENABLE_ASSERTS
#include "Core/Memory/DebugAlloc.h"
#endif

#include "Resources/VirtualFileSystem.h"
#include "Resources/ResourceManager.h"
#include "EngineConfig.h"
#include "Core/Jobs/JobSystem.h"

namespace noc {

class WinWindow;
class MainLoop;

class Engine
{
public:
    // Config access: only valid BEFORE Init().
    EngineConfig& ConfigMutable();
    const EngineConfig& Config() const { return cfg_; }

    bool SetContentRoot(const char* path);
    bool SetOverrideRoot(const char* path);
    bool SetArchivePath(const char* path);

    bool Init();
    void TickOnce();
    void Shutdown();

    int Run();
    void BeginFrame();
    void Tick();
    void EndFrame();

    IAllocator& Allocator();
    LinearArena& FrameArena();

    VirtualFileSystem& VFS() { return vfs_; }
    ResourceManager& Resources() { return resources_; }
    const ResourceManager& Resources() const { return resources_; }

    JobSystem& Jobs() { return jobs_; }
    const JobSystem& Jobs() const { return jobs_; }

    bool InitMemory();
    void KillMemory();

private:
    bool IsConfigMutable() const { return !initialized_; }

private:
    SubsystemRegistry registry_;

    MallocAllocator baseAlloc_;

#if NOC_ENABLE_ASSERTS
    DebugAlloc debugAlloc_{ baseAlloc_ };
    IAllocator* alloc_ = &debugAlloc_;
#else
    IAllocator* alloc_ = &baseAlloc_;
#endif

    void* frameArenaMem_ = nullptr;
    LinearArena frameArena_;

    VirtualFileSystem vfs_;
    JobSystem jobs_;
    ResourceManager resources_;

    EngineConfig cfg_{};
    bool initialized_ = false;
};

} // namespace noc
```

### Listing 68 — `Phase 6 — Job System & Async Infrastructure.md` — `Engine/Runtime/Engine.cpp`
```cpp
#include "Engine.h"

#include <span>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Time.h"

#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h"

namespace noc {

IAllocator& Engine::Allocator() { return *alloc_; }
LinearArena& Engine::FrameArena() { return frameArena_; }

EngineConfig& Engine::ConfigMutable()
{
    if (initialized_)
    {
        NOC_LOG_ERROR("Runtime", "EngineConfig is frozen after Init(). Modify config before calling Init().");
        return cfg_;
    }
    return cfg_;
}

bool Engine::SetContentRoot(const char* path)
{
    if (!IsConfigMutable())
    {
        NOC_LOG_ERROR("Runtime", "SetContentRoot() called after Init(); ignored.");
        return false;
    }
    cfg_.contentRoot = path;
    return true;
}

bool Engine::SetOverrideRoot(const char* path)
{
    if (!IsConfigMutable())
    {
        NOC_LOG_ERROR("Runtime", "SetOverrideRoot() called after Init(); ignored.");
        return false;
    }
    cfg_.overrideRoot = path;
    return true;
}

bool Engine::SetArchivePath(const char* path)
{
    if (!IsConfigMutable())
    {
        NOC_LOG_ERROR("Runtime", "SetArchivePath() called after Init(); ignored.");
        return false;
    }
    cfg_.archivePath = path;
    return true;
}

bool Engine::InitMemory()
{
    constexpr std::size_t kFrameArenaBytes = 8 * 1024 * 1024; // Design choice
    frameArenaMem_ = Allocator().Allocate(kFrameArenaBytes, 64);
    frameArena_.Init(frameArenaMem_, kFrameArenaBytes);

    NOC_LOG_INFO("Core", "Memory system initialized (frame arena=%zu bytes)", kFrameArenaBytes);
    return true;
}

void Engine::KillMemory()
{
    if (frameArenaMem_)
    {
        Allocator().Deallocate(frameArenaMem_);
        frameArenaMem_ = nullptr;
    }

#if NOC_ENABLE_ASSERTS
    NOC_LOG_INFO("Core", "Memory stats: total=%zu outstanding=%zu allocs=%zu",
        debugAlloc_.TotalAllocatedBytes(),
        debugAlloc_.OutstandingBytes(),
        debugAlloc_.AllocationCount());
#endif
}

static bool StartupLog(void*)
{
    noc::GetLogger().Init();
    NOC_LOG_INFO("Core", "Logger initialized");
    return true;
}

static void ShutdownLog(void*)
{
    NOC_LOG_INFO("Core", "Logger shutting down");
    noc::GetLogger().Shutdown();
}

static bool StartupTime(void*)
{
    noc::GetTime().Init();
    NOC_LOG_INFO("Core", "Time system initialized");
    return true;
}

static void ShutdownTime(void*)
{
    NOC_LOG_INFO("Core", "Time system shutting down");
}

static bool StartupAssert(void*)
{
    NOC_LOG_INFO("Core", "Assert system initialized");
    return true;
}

static void ShutdownAssert(void*)
{
    NOC_LOG_INFO("Core", "Assert system shutting down");
}

static bool StartupMemory(void* ctx)
{
    auto* e = static_cast<noc::Engine*>(ctx);
    return e->InitMemory();
}

static void ShutdownMemory(void* ctx)
{
    NOC_LOG_INFO("Core", "Memory system shutting down");
    auto* e = static_cast<noc::Engine*>(ctx);
    e->KillMemory();
}

static bool StartupWindow(void* ctx)
{
    auto* e = static_cast<Engine*>(ctx);
    (void)e;
    NOC_LOG_INFO("Win32", "Window subsystem ready");
    return true;
}

static void ShutdownWindow(void*)
{
    NOC_LOG_INFO("Win32", "Window subsystem shutdown");
}

static bool StartupJobs(void* ctx)
{
    auto* e = static_cast<noc::Engine*>(ctx);
    // Design choice: worker count = HW threads - 1 (leave room for main thread), clamped.
    const uint32_t hw = std::max(1u, std::thread::hardware_concurrency());
    const uint32_t workers = (hw > 1) ? (hw - 1) : 1;
    return e->Jobs().Init(workers);
}

static void ShutdownJobs(void* ctx)
{
    auto* e = static_cast<noc::Engine*>(ctx);
    e->Jobs().Shutdown();
}

bool Engine::Init()
{
    std::span<const char* const> depsLog{};

    static const char* kDepsNeedLog[] = { "Log" };
    std::span<const char* const> depsNeedLog{ kDepsNeedLog, 1 };

    static const char* kDepsJobs[] = { "Log", "Memory" };
    std::span<const char* const> depsJobs{ kDepsJobs, 2 };

    registry_.Register(SubsystemDesc{ "Log",    depsLog,     &StartupLog,    &ShutdownLog });
    registry_.Register(SubsystemDesc{ "Time",   depsNeedLog, &StartupTime,   &ShutdownTime });
    registry_.Register(SubsystemDesc{ "Memory", depsNeedLog, &StartupMemory, &ShutdownMemory });
    registry_.Register(SubsystemDesc{ "Assert", depsNeedLog, &StartupAssert, &ShutdownAssert });
    registry_.Register(SubsystemDesc{ "Window", depsNeedLog, &StartupWindow, &ShutdownWindow });
    registry_.Register(SubsystemDesc{ "Jobs",   depsJobs,    &StartupJobs,   &ShutdownJobs });

    if (!registry_.StartupAll(this))
        return false;

    // Phase 3 policy: later mounts override earlier mounts.
    if (cfg_.archivePath && cfg_.archivePath[0] != 0)
    {
        if (!vfs_.MountArchive(cfg_.archivePath))
            NOC_LOG_WARN("VFS", "Failed to mount archivePath: %s", cfg_.archivePath);
    }

    if (cfg_.contentRoot && cfg_.contentRoot[0] != 0)
    {
        if (!vfs_.MountLooseDirectory(cfg_.contentRoot))
            NOC_LOG_WARN("VFS", "Failed to mount contentRoot: %s", cfg_.contentRoot);
    }

    if (cfg_.overrideRoot && cfg_.overrideRoot[0] != 0)
    {
        if (!vfs_.MountLooseDirectory(cfg_.overrideRoot))
            NOC_LOG_WARN("VFS", "Failed to mount overrideRoot: %s", cfg_.overrideRoot);
    }

    // Phase 6: ResourceManager uses JobSystem (must be after jobs + VFS mounts).
    if (!resources_.Init(*this, vfs_))
        return false;

    initialized_ = true;
    return true;
}

int Engine::Run()
{
    WinWindow window;
    WinWindowDesc wd{};
    wd.title = L"NocturneHost";
    wd.width = 1280;
    wd.height = 720;
    wd.resizable = true;

    if (!window.Create(wd))
    {
        NOC_LOG_FATAL("Win32", "Failed to create window");
        return -1;
    }

    MainLoop loop;
    loop.Run(*this, window);

    window.Destroy();
    return 0;
}

void Engine::BeginFrame()
{
    GetTime().BeginFrame();
    FrameArena().Reset();
}

void Engine::Tick()
{
    resources_.Update();
}

void Engine::EndFrame()
{
    GetTime().EndFrame();
}

void Engine::TickOnce()
{
    GetTime().BeginFrame();
    FrameArena().Reset();

    void* a = FrameArena().Allocate(256, 16);
    void* b = FrameArena().Allocate(1024, 64);
    (void)a; (void)b;

    GetTime().EndFrame();

    NOC_LOG_INFO("Core", "TickOnce() dt=%.6f sec arenaUsed=%zu bytes",
        GetTime().DeltaSeconds(),
        FrameArena().Used());
}

void Engine::Shutdown()
{
    // Resource manager must shutdown while jobs + memory + log still exist.
    resources_.Shutdown();

    registry_.ShutdownAll(this);
}

} // namespace noc
```

### Listing 69 — `Phase 6 — Job System & Async Infrastructure.md` — `Engine/Resources/ResourceManager.h`
```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "Resources/ResourceHandle.h"
#include "Resources/Typed/ResourceLoaderRegistry.h"
#include "Resources/Typed/ResourceHandleT.h"
#include "Resources/Typed/TextResource.h"
#include "Resources/Typed/ResourceType.h"

namespace noc
{
    class Engine;
    class VirtualFileSystem;

    class ResourceManager
    {
    public:
        ResourceManager() = default;
        ~ResourceManager();

        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;

        bool Init(Engine& engine, VirtualFileSystem& vfs);
        void Shutdown();

        // Call once per frame on the main thread.
        void Update();

        // ---- Binary blob API ----
        ResourceHandle RequestBinary(std::string_view vpath);

        bool IsReady(ResourceHandle h) const;
        bool HasFailed(ResourceHandle h) const;

        const uint8_t* GetBytes(ResourceHandle h) const;
        size_t GetSize(ResourceHandle h) const;

        // Returns nullptr if:
        // - handle invalid
        // - resource has not failed
        // Otherwise returns a stable, null-terminated error message.
        const char* GetError(ResourceHandle h) const;

        // Optional helper for host-side testing.
        bool WaitUntilReady(ResourceHandle h, uint32_t timeoutMs);

        // ---- Typed API (Phase 5) ----
        ResourceHandleT<TextResource> RequestText(const char* vpath);
        const TextResource* GetText(ResourceHandleT<TextResource> h) const;

        ResourceLoaderRegistry& Loaders() { return loaders_; }
        const ResourceLoaderRegistry& Loaders() const { return loaders_; }

    private:
        bool ValidateHandle_(ResourceHandle h, uint32_t* outIndex) const;
        void EnqueueLoadJob_(uint32_t index);
        void WaitAllJobs_();

    private:
        Engine* engine_ = nullptr;
        VirtualFileSystem* vfs_ = nullptr;

        // Internal opaque state (allocated in .cpp)
        void* state_ = nullptr;

        bool running_ = false;

        ResourceLoaderRegistry loaders_;
    };

} // namespace noc
```

### Listing 70 — `Phase 6 — Job System & Async Infrastructure.md` — `Engine/Resources/ResourceManager.cpp`
```cpp
#include "ResourceManager.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Jobs/JobSystem.h"
#include "Core/Memory/Allocator.h"
#include "Resources/ResourceID.h"
#include "Resources/VirtualFileSystem.h"
#include "Resources/VPath.h"
#include "Resources/FileHandle.h"
#include "Runtime/Engine.h"

#include "Engine/Resources/Typed/TextResourceLoader.h"

namespace noc
{
    enum class ResourceState : uint8_t
    {
        Empty = 0,
        Requested,
        Loading,
        Ready,
        Failed
    };

    struct Record
    {
        ResourceID id{};
        std::string vpathNormalized;
        std::atomic<ResourceState> state{ ResourceState::Empty };

        uint32_t generation = 1;

        // Binary blob (owned by ResourceManager allocator)
        uint8_t* data = nullptr;
        size_t size = 0;

        // Typed object (type-erased)
        ResourceType type = ResourceType::Unknown;
        void* typedObject = nullptr;

        std::string error;

        Record() = default;
        Record(const Record&) = delete;
        Record& operator=(const Record&) = delete;
        Record(Record&&) = delete;
        Record& operator=(Record&&) = delete;
    };

    struct CompletedReq
    {
        uint32_t index = 0;
        ResourceType type = ResourceType::Unknown;

        bool ok = false;
        std::vector<uint8_t> bytes; // only used for Binary (or intermediate before decode)
        void* typedObject = nullptr;

        std::string error;
    };

    struct InternalState
    {
        std::mutex mtx;

        // Completion queue is produced by worker jobs, consumed by main thread Update().
        std::deque<CompletedReq> completed;

        std::unordered_map<uint64_t, uint32_t> idToIndex;
        std::vector<std::unique_ptr<Record>> records;

        std::atomic<uint32_t> jobsInflight{ 0 };
        std::condition_variable cvJobsIdle;
    };

    static TextResourceLoader g_textLoader;

    static std::string NormalizeVPath_(std::string_view vpath)
    {
        char buf[512]{};
        if (!noc::vpath::NormalizeToRelative(buf, sizeof(buf), vpath))
            return {};
        return std::string(buf);
    }

    static uint64_t MakeCacheKey_(uint64_t idValue, ResourceType type)
    {
        return idValue ^ (static_cast<uint64_t>(type) * 0x9E3779B97F4A7C15ull);
    }

    ResourceManager::~ResourceManager()
    {
        Shutdown();
    }

    bool ResourceManager::Init(Engine& engine, VirtualFileSystem& vfs)
    {
        if (running_)
            return true;

        engine_ = &engine;
        vfs_ = &vfs;

        auto* st = new InternalState();
        st->records.reserve(256);
        state_ = st;

        // Register built-in loaders (Phase 5)
        loaders_.RegisterLoader(&g_textLoader);

        running_ = true;

        NOC_LOG_INFO("Res", "ResourceManager initialized (job-backed)");
        return true;
    }

    void ResourceManager::WaitAllJobs_()
    {
        auto* st = static_cast<InternalState*>(state_);
        if (!st)
            return;

        std::unique_lock<std::mutex> lock(st->mtx);
        st->cvJobsIdle.wait(lock, [&]() {
            return st->jobsInflight.load(std::memory_order_acquire) == 0;
        });
    }

    void ResourceManager::Shutdown()
    {
        if (!running_)
            return;

        running_ = false;

        auto* st = static_cast<InternalState*>(state_);
        if (!st)
            return;

        // Wait for outstanding load jobs to finish producing completions.
        WaitAllJobs_();

        // Drain completions once (safe: no producers remain).
        Update();

        // Free blobs/typed objects
        for (auto& rp : st->records)
        {
            if (!rp) continue;

            if (rp->typedObject)
            {
                if (rp->type == ResourceType::Text)
                    delete static_cast<TextResource*>(rp->typedObject);

                rp->typedObject = nullptr;
            }

            if (rp->data)
            {
                engine_->Allocator().Deallocate(rp->data);
                rp->data = nullptr;
                rp->size = 0;
            }
        }

        delete st;
        state_ = nullptr;

        engine_ = nullptr;
        vfs_ = nullptr;

        NOC_LOG_INFO("Res", "ResourceManager shutdown complete");
    }

    bool ResourceManager::ValidateHandle_(ResourceHandle h, uint32_t* outIndex) const
    {
        if (!h.IsValid() || !state_)
            return false;

        auto* st = static_cast<InternalState*>(state_);
        if (h.index >= st->records.size())
            return false;

        const Record* r = st->records[h.index].get();
        if (!r)
            return false;

        if (r->generation != h.generation)
            return false;

        *outIndex = h.index;
        return true;
    }

    void ResourceManager::EnqueueLoadJob_(uint32_t index)
    {
        auto* st = static_cast<InternalState*>(state_);
        if (!st || !engine_ || !vfs_)
            return;

        st->jobsInflight.fetch_add(1, std::memory_order_acq_rel);

        engine_->Jobs().Enqueue([this, index]() {
            auto* stLocal = static_cast<InternalState*>(state_);
            if (!stLocal || !vfs_)
                return;

            CompletedReq done{};
            done.index = index;

            // Snapshot record pointer safely.
            Record* r = nullptr;
            {
                std::lock_guard<std::mutex> lock(stLocal->mtx);
                if (index < stLocal->records.size())
                    r = stLocal->records[index].get();
            }

            if (!running_ || !r)
            {
                done.ok = false;
                done.error = "Invalid record index or manager stopped";
            }
            else
            {
                done.type = r->type;

                FileHandle fh = vfs_->OpenRead(r->vpathNormalized);
                if (!fh.valid)
                {
                    done.ok = false;
                    done.error = "OpenRead failed";
                }
                else
                {
                    const uint64_t sz64 = vfs_->Size(fh);
                    const size_t sz = (sz64 > SIZE_MAX) ? SIZE_MAX : (size_t)sz64;

                    done.bytes.resize(sz);

                    if (sz > 0)
                    {
                        const size_t read = vfs_->Read(fh, done.bytes.data(), sz);
                        if (read != sz)
                        {
                            done.ok = false;
                            done.error = "Short read";
                            done.bytes.clear();
                        }
                        else
                        {
                            done.ok = true;

                            if (done.type != ResourceType::Binary)
                            {
                                IResourceLoader* loader = loaders_.FindLoader(done.type);
                                if (!loader)
                                {
                                    done.ok = false;
                                    done.error = "No loader registered for requested type";
                                    done.bytes.clear();
                                }
                                else
                                {
                                    const ResourceLoadResult rr = loader->Decode(done.bytes.data(), done.bytes.size());
                                    if (!rr.ok)
                                    {
                                        done.ok = false;
                                        done.error = rr.error;
                                        done.bytes.clear();
                                    }
                                    else
                                    {
                                        done.typedObject = rr.object;
                                        done.bytes.clear(); // decoded object owns data now
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        // empty file is still OK
                        done.ok = true;

                        if (done.type != ResourceType::Binary)
                        {
                            IResourceLoader* loader = loaders_.FindLoader(done.type);
                            if (!loader)
                            {
                                done.ok = false;
                                done.error = "No loader registered for requested type";
                            }
                            else
                            {
                                const ResourceLoadResult rr = loader->Decode(nullptr, 0);
                                if (!rr.ok)
                                {
                                    done.ok = false;
                                    done.error = rr.error;
                                }
                                else
                                {
                                    done.typedObject = rr.object;
                                }
                            }
                        }
                    }

                    vfs_->Close(fh);
                }
            }

            {
                std::lock_guard<std::mutex> lock(stLocal->mtx);
                stLocal->completed.push_back(std::move(done));
            }

            const uint32_t left = stLocal->jobsInflight.fetch_sub(1, std::memory_order_acq_rel) - 1;
            if (left == 0)
            {
                std::lock_guard<std::mutex> lock(stLocal->mtx);
                stLocal->cvJobsIdle.notify_all();
            }
        });
    }

    ResourceHandle ResourceManager::RequestBinary(std::string_view vpath)
    {
        if (!running_ || !state_)
            return {};

        auto* st = static_cast<InternalState*>(state_);

        const std::string norm = NormalizeVPath_(vpath);
        if (norm.empty())
        {
            NOC_LOG_ERROR("Res", "Invalid vpath: %.*s", (int)vpath.size(), vpath.data());
            return {};
        }

        const ResourceID id = MakeResourceID(norm);
        const uint64_t key = MakeCacheKey_(id.value, ResourceType::Binary);

        // Look up existing
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            auto it = st->idToIndex.find(key);
            if (it != st->idToIndex.end())
            {
                const uint32_t idx = it->second;
                Record* r = st->records[idx].get();
                return ResourceHandle{ idx, r->generation };
            }
        }

        // Create new record
        auto rec = std::make_unique<Record>();
        rec->id = id;
        rec->vpathNormalized = norm;
        rec->type = ResourceType::Binary;
        rec->typedObject = nullptr;
        rec->state.store(ResourceState::Requested, std::memory_order_release);

        uint32_t index = 0;
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            index = (uint32_t)st->records.size();
            st->records.push_back(std::move(rec));
            st->idToIndex.emplace(key, index);
        }

        // Enqueue job
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            st->records[index]->state.store(ResourceState::Loading, std::memory_order_release);
        }
        EnqueueLoadJob_(index);

        return ResourceHandle{ index, st->records[index]->generation };
    }

    ResourceHandleT<TextResource> ResourceManager::RequestText(const char* vpath)
    {
        if (!running_ || !state_)
            return {};

        auto* st = static_cast<InternalState*>(state_);

        const std::string norm = NormalizeVPath_(std::string_view{ vpath ? vpath : "" });
        if (norm.empty())
        {
            NOC_LOG_ERROR("Res", "Invalid vpath: %s", vpath ? vpath : "(null)");
            return {};
        }

        const ResourceID id = MakeResourceID(norm);
        const uint64_t key = MakeCacheKey_(id.value, ResourceType::Text);

        // Look up existing
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            auto it = st->idToIndex.find(key);
            if (it != st->idToIndex.end())
            {
                const uint32_t idx = it->second;
                Record* r = st->records[idx].get();
                return ResourceHandleT<TextResource>(ResourceHandle{ idx, r->generation });
            }
        }

        if (!loaders_.FindLoader(ResourceType::Text))
        {
            NOC_LOG_ERROR("Res", "RequestText: no loader registered for ResourceType::Text");
            return {};
        }

        // Create record
        auto rec = std::make_unique<Record>();
        rec->id = id;
        rec->vpathNormalized = norm;
        rec->type = ResourceType::Text;
        rec->typedObject = nullptr;
        rec->state.store(ResourceState::Requested, std::memory_order_release);

        uint32_t index = 0;
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            index = (uint32_t)st->records.size();
            st->records.push_back(std::move(rec));
            st->idToIndex.emplace(key, index);
        }

        // Enqueue job
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            st->records[index]->state.store(ResourceState::Loading, std::memory_order_release);
        }
        EnqueueLoadJob_(index);

        Record* r = st->records[index].get();
        return ResourceHandleT<TextResource>(ResourceHandle{ index, r->generation });
    }

    void ResourceManager::Update()
    {
        if (!state_)
            return;

        auto* st = static_cast<InternalState*>(state_);

        for (;;)
        {
            CompletedReq c{};
            {
                std::lock_guard<std::mutex> lock(st->mtx);
                if (st->completed.empty())
                    break;
                c = std::move(st->completed.front());
                st->completed.pop_front();
            }

            if (c.index >= st->records.size())
                continue;

            Record& r = *st->records[c.index];

            if (!c.ok)
            {
                r.error = std::move(c.error);
                r.state.store(ResourceState::Failed, std::memory_order_release);
                NOC_LOG_ERROR("Res", "FAILED: %s (%s)", r.vpathNormalized.c_str(), r.error.c_str());
                continue;
            }

            // Success path:
            r.error.clear();

            if (r.type == ResourceType::Binary)
            {
                // Replace blob
                if (r.data)
                {
                    engine_->Allocator().Deallocate(r.data);
                    r.data = nullptr;
                    r.size = 0;
                }

                r.size = c.bytes.size();
                if (r.size > 0)
                {
                    r.data = static_cast<uint8_t*>(engine_->Allocator().Allocate(r.size, 16));
                    std::memcpy(r.data, c.bytes.data(), r.size);
                }

                r.state.store(ResourceState::Ready, std::memory_order_release);
                NOC_LOG_INFO("Res", "READY: %s (%zu bytes)", r.vpathNormalized.c_str(), r.size);
            }
            else
            {
                // Replace typed object
                if (r.typedObject)
                {
                    if (r.type == ResourceType::Text)
                        delete static_cast<TextResource*>(r.typedObject);
                    r.typedObject = nullptr;
                }

                r.typedObject = c.typedObject;
                r.state.store(ResourceState::Ready, std::memory_order_release);
                NOC_LOG_INFO("Res", "READY: %s (typed=%u)", r.vpathNormalized.c_str(), (unsigned)r.type);
            }
        }
    }

    bool ResourceManager::IsReady(ResourceHandle h) const
    {
        uint32_t idx = 0;
        if (!ValidateHandle_(h, &idx))
            return false;

        auto* st = static_cast<InternalState*>(state_);
        return st->records[idx]->state.load(std::memory_order_acquire) == ResourceState::Ready;
    }

    bool ResourceManager::HasFailed(ResourceHandle h) const
    {
        uint32_t idx = 0;
        if (!ValidateHandle_(h, &idx))
            return true;

        auto* st = static_cast<InternalState*>(state_);
        return st->records[idx]->state.load(std::memory_order_acquire) == ResourceState::Failed;
    }

    const uint8_t* ResourceManager::GetBytes(ResourceHandle h) const
    {
        uint32_t idx = 0;
        if (!ValidateHandle_(h, &idx))
            return nullptr;

        auto* st = static_cast<InternalState*>(state_);
        const Record& r = *st->records[idx];
        if (r.type != ResourceType::Binary)
            return nullptr;

        if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
            return nullptr;

        return r.data;
    }

    size_t ResourceManager::GetSize(ResourceHandle h) const
    {
        uint32_t idx = 0;
        if (!ValidateHandle_(h, &idx))
            return 0;

        auto* st = static_cast<InternalState*>(state_);
        const Record& r = *st->records[idx];
        if (r.type != ResourceType::Binary)
            return 0;

        if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
            return 0;

        return r.size;
    }

    const char* ResourceManager::GetError(ResourceHandle h) const
    {
        uint32_t idx = 0;
        if (!ValidateHandle_(h, &idx))
            return "Invalid handle";

        auto* st = static_cast<InternalState*>(state_);
        const Record& r = *st->records[idx];
        if (r.state.load(std::memory_order_acquire) != ResourceState::Failed)
            return nullptr;

        return r.error.c_str();
    }

    bool ResourceManager::WaitUntilReady(ResourceHandle h, uint32_t timeoutMs)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

        while (std::chrono::steady_clock::now() < deadline)
        {
            Update();

            if (IsReady(h))
                return true;

            if (HasFailed(h))
                return false;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return IsReady(h);
    }

    const TextResource* ResourceManager::GetText(ResourceHandleT<TextResource> h) const
    {
        if (!state_)
            return nullptr;

        const ResourceHandle uh = h.Untyped();

        uint32_t idx = 0;
        if (!ValidateHandle_(uh, &idx))
            return nullptr;

        auto* st = static_cast<InternalState*>(state_);
        const Record& r = *st->records[idx];

        if (r.type != ResourceType::Text)
            return nullptr;

        if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
            return nullptr;

        return static_cast<const TextResource*>(r.typedObject);
    }

} // namespace noc
```

### Listing 71 — `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Input/InputTypes.h`
```cpp
#pragma once
#include <cstdint>

namespace noc
{
	// Keep public headers STL-free (project rule in architecture doc). 

	// --- Keys (minimal set to start; extend as needed) ---
	enum class Key : uint16_t
	{
		Unknown = 0,

		Escape,

		W, A, S, D,
		Q, E,
		Space,
		LeftShift,
		LeftCtrl,

		MouseLeft,
		MouseRight,
		MouseMiddle,

		Count
	};

	struct MouseDelta
	{
		int dx = 0;
		int dy = 0;
	};

	// 32-bit stable ID for actions (hash of name).
	using ActionId = uint32_t;

	inline constexpr ActionId InvalidAction = 0;

	// Simple FNV-1a hash for action names (compile-time friendly if needed later).
	inline constexpr ActionId HashActionName(const char* s)
	{
		uint32_t h = 2166136261u;
		if (!s) return 0;
		while (*s)
		{
			h ^= static_cast<uint8_t>(*s++);
			h *= 16777619u;
		}
		return h;
	}
}
```

### Listing 72 — `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Input/ActionMap.h`
```cpp
#pragma once
#include "InputTypes.h"
#include <cstdint>

namespace noc
{
	class InputSystem;

	// Small, fixed-capacity action map (no STL in public header).
	class ActionMap
	{
	public:
		enum class SourceType : uint8_t
		{
			DigitalKey = 0,
			DigitalMouseButton,

			AnalogMouseDeltaX,
			AnalogMouseDeltaY,
			AnalogMouseWheel
		};

		struct Binding
		{
			ActionId    action = InvalidAction;
			SourceType  type = SourceType::DigitalKey;

			// For DigitalKey / DigitalMouseButton.
			Key         key = Key::Unknown;

			// Scale for analog sources.
			float       scale = 1.0f;

			// For digital, optional: treat as "negative" contribution (e.g., S on MoveForward).
			bool        negate = false;
		};

	public:
		void Clear();

		// Returns false if capacity exceeded.
		bool Bind(const Binding& b);

		// Rebind by action + source type (simple, first match). Returns true if replaced.
		bool Rebind(ActionId action, SourceType type, const Binding& replacement);

		// Call once per frame after InputSystem has updated its snapshot.
		void Update(const InputSystem& input);

		// Query
		float GetActionValue(ActionId action) const;
		bool  IsActionDown(ActionId action) const { return GetActionValue(action) != 0.0f; }

	private:
		static constexpr uint32_t kMaxBindings = 64;
		static constexpr uint32_t kMaxActionsCached = 64;

		Binding bindings_[kMaxBindings]{};
		uint32_t bindingCount_ = 0;

		// Cached computed values (per frame).
		struct ActionValue
		{
			ActionId action = InvalidAction;
			float    value = 0.0f;
		};
		ActionValue values_[kMaxActionsCached]{};
		uint32_t valueCount_ = 0;

	private:
		void SetValue_(ActionId action, float v);
		float GetValue_(ActionId action) const;
	};
}
```

### Listing 73 — `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Input/ActionMap.cpp`
```cpp
#include "ActionMap.h"
#include "InputSystem.h"
#include <cmath>

namespace noc
{
	void ActionMap::Clear()
	{
		bindingCount_ = 0;
		valueCount_ = 0;
	}

	bool ActionMap::Bind(const Binding& b)
	{
		if (bindingCount_ >= kMaxBindings)
			return false;
		bindings_[bindingCount_++] = b;
		return true;
	}

	bool ActionMap::Rebind(ActionId action, SourceType type, const Binding& replacement)
	{
		for (uint32_t i = 0; i < bindingCount_; ++i)
		{
			if (bindings_[i].action == action && bindings_[i].type == type)
			{
				bindings_[i] = replacement;
				return true;
			}
		}
		return false;
	}

	void ActionMap::Update(const InputSystem& input)
	{
		// Clear cached values.
		valueCount_ = 0;

		for (uint32_t i = 0; i < bindingCount_; ++i)
		{
			const Binding& b = bindings_[i];
			if (b.action == InvalidAction)
				continue;

			float v = 0.0f;

			switch (b.type)
			{
			case SourceType::DigitalKey:
			case SourceType::DigitalMouseButton:
			{
				const bool down = input.IsDown(b.key);
				v = down ? 1.0f : 0.0f;
				break;
			}

			case SourceType::AnalogMouseDeltaX:
			{
				const MouseDelta md = input.GetMouseDelta();
				v = static_cast<float>(md.dx) * b.scale;
				break;
			}
			case SourceType::AnalogMouseDeltaY:
			{
				const MouseDelta md = input.GetMouseDelta();
				v = static_cast<float>(md.dy) * b.scale;
				break;
			}
			case SourceType::AnalogMouseWheel:
			{
				v = static_cast<float>(input.GetWheelDelta()) * b.scale;
				break;
			}
			default:
				break;
			}

			if (b.negate)
				v = -v;

			// Accumulate contributions (e.g., W + S bindings).
			const float prev = GetValue_(b.action);
			SetValue_(b.action, prev + v);
		}

		// Optional: clamp near-zero to 0 for stability
		for (uint32_t i = 0; i < valueCount_; ++i)
		{
			if (std::fabs(values_[i].value) < 1e-6f)
				values_[i].value = 0.0f;
		}
	}

	float ActionMap::GetActionValue(ActionId action) const
	{
		return GetValue_(action);
	}

	void ActionMap::SetValue_(ActionId action, float v)
	{
		for (uint32_t i = 0; i < valueCount_; ++i)
		{
			if (values_[i].action == action)
			{
				values_[i].value = v;
				return;
			}
		}

		if (valueCount_ < kMaxActionsCached)
		{
			values_[valueCount_++] = ActionValue{ action, v };
		}
	}

	float ActionMap::GetValue_(ActionId action) const
	{
		for (uint32_t i = 0; i < valueCount_; ++i)
		{
			if (values_[i].action == action)
				return values_[i].value;
		}
		return 0.0f;
	}
}
```

### Listing 74 — `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Input/InputSystem.h`
```cpp
#pragma once
#include "InputTypes.h"
#include "ActionMap.h"

namespace noc
{
	namespace platform
	{
		// Platform-side sink interface is declared in WinWindow.h (platform module).
		struct RawMouseEvent;
		struct RawKeyboardEvent;
		struct IRawInputSink;
	}

	class Engine;

	class InputSystem final : public ActionMap
	{
	public:
		InputSystem() = default;

		// Engine-owned lifecycle (init does not require HWND; attach does).
		bool Init(Engine& engine);
		void Shutdown();

		// Call once after window is created (in Engine::Run / MainLoop setup).
		bool AttachToWindow(void* nativeHwnd);
		void DetachFromWindow();

		// Frame lifecycle
		void BeginFrame();
		void Update(); // consumes queued raw events and updates snapshot + action values

		// Queries
		bool IsDown(Key k) const;
		bool WasPressed(Key k) const;
		bool WasReleased(Key k) const;

		MouseDelta GetMouseDelta() const { return mouseDelta_; }
		int        GetWheelDelta() const { return wheelDelta_; }

		// Action queries (ActionMap methods)
		float GetActionValue(ActionId action) const { return ActionMap::GetActionValue(action); }

		// Platform hook: returns sink pointer that WinWindow will call.
		platform::IRawInputSink* RawSink();

		// Convenience: create a default FPS-ish binding set.
		void BuildDefaultBindings();

		// Call on focus loss to prevent stuck keys.
		void ClearAllState();

	private:
		Engine* engine_ = nullptr;
		void* hwnd_ = nullptr;

		// Snapshot states
		static constexpr uint32_t kKeyCount = static_cast<uint32_t>(Key::Count);

		bool curr_[kKeyCount]{};
		bool prev_[kKeyCount]{};

		MouseDelta mouseDelta_{};
		int wheelDelta_ = 0;

		// Raw event queue (implementation hidden in .cpp)
		struct Impl;
		Impl* impl_ = nullptr;

	private:
		void ApplyKey_(Key k, bool down);
	};
}
```

### Listing 75 — `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Input/InputSystem.cpp`
```cpp
#include "InputSystem.h"
#include "Runtime/Engine.h"
#include "Core/Log.h"
#include "Core/Assert.h"

#include "Platform/Win32/WinWindow.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <hidusage.h>
#include <vector>

namespace noc
{
	struct InputSystem::Impl final : public platform::IRawInputSink
	{
		std::vector<platform::RawMouseEvent> mouseEvents;
		std::vector<platform::RawKeyboardEvent> keyEvents;

		InputSystem* owner = nullptr;

		void OnRawMouse(const platform::RawMouseEvent& e) override
		{
			mouseEvents.push_back(e);
		}

		void OnRawKeyboard(const platform::RawKeyboardEvent& e) override
		{
			keyEvents.push_back(e);
		}

		void OnFocusLost() override
		{
			owner->ClearAllState();
		}
	};

	// --- Key translation (minimal, extend later) ---
	static Key TranslateVKey(uint16_t vk)
	{
		switch (vk)
		{
		case VK_ESCAPE: return Key::Escape;
		case 'W': return Key::W;
		case 'A': return Key::A;
		case 'S': return Key::S;
		case 'D': return Key::D;
		case 'Q': return Key::Q;
		case 'E': return Key::E;
		case VK_SPACE: return Key::Space;
		case VK_LSHIFT: return Key::LeftShift;
		case VK_LCONTROL: return Key::LeftCtrl;
		default: return Key::Unknown;
		}
	}

	bool InputSystem::Init(Engine& engine)
	{
		engine_ = &engine;
		impl_ = new Impl();
		impl_->owner = this;

		BuildDefaultBindings();

		NOC_LOG_INFO("Input", "InputSystem initialized");
		return true;
	}

	void InputSystem::Shutdown()
	{
		DetachFromWindow();

		delete impl_;
		impl_ = nullptr;
		engine_ = nullptr;

		NOC_LOG_INFO("Input", "InputSystem shutdown");
	}

	bool InputSystem::AttachToWindow(void* nativeHwnd)
	{
		hwnd_ = nativeHwnd;
		if (!hwnd_)
			return false;

		RAWINPUTDEVICE rid[2]{};

		// Mouse
		rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
		rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
		rid[0].dwFlags = RIDEV_INPUTSINK; // receive even if not focused (optional); we still clear on focus lost
		rid[0].hwndTarget = (HWND)hwnd_;

		// Keyboard
		rid[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
		rid[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
		rid[1].dwFlags = RIDEV_INPUTSINK;
		rid[1].hwndTarget = (HWND)hwnd_;

		if (!RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE)))
		{
			NOC_LOG_ERROR("Input", "RegisterRawInputDevices failed (err=%lu)", GetLastError());
			return false;
		}

		NOC_LOG_INFO("Input", "Raw input registered");
		return true;
	}

	void InputSystem::DetachFromWindow()
	{
		hwnd_ = nullptr;
	}

	void InputSystem::BeginFrame()
	{
		// Snapshot copy
		for (uint32_t i = 0; i < kKeyCount; ++i)
			prev_[i] = curr_[i];

		// Reset deltas
		mouseDelta_ = {};
		wheelDelta_ = 0;
	}

	void InputSystem::Update()
	{
		if (!impl_)
			return;

		// Consume keyboard events
		for (const auto& e : impl_->keyEvents)
		{
			const Key k = TranslateVKey(e.vkey);
			if (k == Key::Unknown)
				continue;
			ApplyKey_(k, e.down);
		}
		impl_->keyEvents.clear();

		// Consume mouse events
		for (const auto& e : impl_->mouseEvents)
		{
			mouseDelta_.dx += e.dx;
			mouseDelta_.dy += e.dy;
			wheelDelta_ += e.wheel;

			if (e.buttonDownMask & 0x1) ApplyKey_(Key::MouseLeft, true);
			if (e.buttonDownMask & 0x2) ApplyKey_(Key::MouseRight, true);
			if (e.buttonDownMask & 0x4) ApplyKey_(Key::MouseMiddle, true);

			if (e.buttonUpMask & 0x1) ApplyKey_(Key::MouseLeft, false);
			if (e.buttonUpMask & 0x2) ApplyKey_(Key::MouseRight, false);
			if (e.buttonUpMask & 0x4) ApplyKey_(Key::MouseMiddle, false);
		}
		impl_->mouseEvents.clear();

		// Update action values from current snapshot.
		ActionMap::Update(*this);
	}

	bool InputSystem::IsDown(Key k) const
	{
		const uint32_t idx = static_cast<uint32_t>(k);
		if (idx >= kKeyCount)
			return false;
		return curr_[idx];
	}

	bool InputSystem::WasPressed(Key k) const
	{
		const uint32_t idx = static_cast<uint32_t>(k);
		if (idx >= kKeyCount)
			return false;
		return curr_[idx] && !prev_[idx];
	}

	bool InputSystem::WasReleased(Key k) const
	{
		const uint32_t idx = static_cast<uint32_t>(k);
		if (idx >= kKeyCount)
			return false;
		return !curr_[idx] && prev_[idx];
	}

	platform::IRawInputSink* InputSystem::RawSink()
	{
		return impl_;
	}

	void InputSystem::BuildDefaultBindings()
	{
		Clear();

		// Typical FPS movement: forward/back on one action (W adds +1, S adds -1).
		Bind(ActionMap::Binding{ HashActionName("MoveForward"), ActionMap::SourceType::DigitalKey, Key::W, 1.0f, false });
		Bind(ActionMap::Binding{ HashActionName("MoveForward"), ActionMap::SourceType::DigitalKey, Key::S, 1.0f, true });

		Bind(ActionMap::Binding{ HashActionName("MoveRight"), ActionMap::SourceType::DigitalKey, Key::D, 1.0f, false });
		Bind(ActionMap::Binding{ HashActionName("MoveRight"), ActionMap::SourceType::DigitalKey, Key::A, 1.0f, true });

		Bind(ActionMap::Binding{ HashActionName("Jump"), ActionMap::SourceType::DigitalKey, Key::Space, 1.0f, false });

		// Mouse look axes (scaled).
		Bind(ActionMap::Binding{ HashActionName("LookX"), ActionMap::SourceType::AnalogMouseDeltaX, Key::Unknown, 0.01f, false });
		Bind(ActionMap::Binding{ HashActionName("LookY"), ActionMap::SourceType::AnalogMouseDeltaY, Key::Unknown, 0.01f, false });

		NOC_LOG_INFO("Input", "Default action bindings created");
	}

	void InputSystem::ClearAllState()
	{
		for (uint32_t i = 0; i < kKeyCount; ++i)
		{
			curr_[i] = false;
			prev_[i] = false;
		}
		mouseDelta_ = {};
		wheelDelta_ = 0;
	}

	void InputSystem::ApplyKey_(Key k, bool down)
	{
		const uint32_t idx = static_cast<uint32_t>(k);
		if (idx >= kKeyCount)
			return;
		curr_[idx] = down;
	}
}
```

### Listing 76 — `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Platform/Win32/WinWindow.h` (UPDATED)
```cpp
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>

namespace noc
{
	namespace platform
	{
		struct RawMouseEvent
		{
			int dx = 0;
			int dy = 0;
			int wheel = 0;
			uint16_t buttonDownMask = 0; // bit0=L, bit1=R, bit2=M
			uint16_t buttonUpMask = 0;
		};

		struct RawKeyboardEvent
		{
			uint16_t vkey = 0;
			bool down = false;
		};

		struct IRawInputSink
		{
			virtual ~IRawInputSink() = default;
			virtual void OnRawMouse(const RawMouseEvent& e) = 0;
			virtual void OnRawKeyboard(const RawKeyboardEvent& e) = 0;
			virtual void OnFocusLost() = 0;
		};
	}

	struct WinWindowDesc
	{
		const wchar_t* title = L"Nocturne";
		int width = 1280;
		int height = 720;
		bool resizable = true;
	};

	class WinWindow
	{
	public:
		bool Create(const WinWindowDesc& desc);
		void Destroy();

		bool ShouldQuit() const { return shouldQuit_; }
		void RequestQuit() { shouldQuit_ = true; }

		void* Handle() const { return (void*)hwnd_; }

		// Phase 7: forward raw input to engine input system through an abstract sink.
		void SetRawInputSink(platform::IRawInputSink* sink) { rawSink_ = sink; }

	private:
		static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

		void RegisterClassOnce();
		void ApplyClientSize(int clientW, int clientH, bool resizable);

		static void DecodeRawInput_(HRAWINPUT hRawInput, WinWindow* win);
	private:
		HWND hwnd_ = nullptr;
		bool shouldQuit_ = false;

		platform::IRawInputSink* rawSink_ = nullptr;
	};
}
```

### Listing 77 — `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Platform/Win32/WinWindow.cpp` (UPDATED: WM_INPUT + focus)
```cpp
#include <malloc.h>
#include "WinWindow.h"
#include "Core/Log.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace noc
{
	static const wchar_t* kWndClassName = L"NocturneWndClass";

	void WinWindow::RegisterClassOnce()
	{
		static bool registered = false;
		if (registered)
			return;

		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = &WinWindow::WndProc;
		wc.hInstance = GetModuleHandleW(nullptr);
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		wc.lpszClassName = kWndClassName;

		if (!RegisterClassExW(&wc))
		{
			NOC_LOG_FATAL("Win32", "RegisterClassExW failed (err=%lu)", GetLastError());
		}

		registered = true;
	}

	void WinWindow::ApplyClientSize(int clientW, int clientH, bool resizable)
	{
		DWORD style = WS_OVERLAPPEDWINDOW;
		if (!resizable)
			style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

		RECT r{ 0, 0, clientW, clientH };
		AdjustWindowRect(&r, style, FALSE);

		SetWindowLongPtrW(hwnd_, GWL_STYLE, style);
		SetWindowPos(hwnd_, nullptr, 0, 0, r.right - r.left, r.bottom - r.top,
			SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}

	bool WinWindow::Create(const WinWindowDesc& desc)
	{
		RegisterClassOnce();

		const DWORD style = desc.resizable ? WS_OVERLAPPEDWINDOW : (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX));

		hwnd_ = CreateWindowExW(
			0,
			kWndClassName,
			desc.title,
			style,
			CW_USEDEFAULT, CW_USEDEFAULT,
			desc.width, desc.height,
			nullptr, nullptr,
			GetModuleHandleW(nullptr),
			this);

		if (!hwnd_)
		{
			NOC_LOG_FATAL("Win32", "CreateWindowExW failed (err=%lu)", GetLastError());
			return false;
		}

		ApplyClientSize(desc.width, desc.height, desc.resizable);

		ShowWindow(hwnd_, SW_SHOW);
		UpdateWindow(hwnd_);

		NOC_LOG_INFO("Win32", "Window created: %dx%d", desc.width, desc.height);
		return true;
	}

	void WinWindow::Destroy()
	{
		rawSink_ = nullptr; // avoid dangling sink
		if (hwnd_)
		{
			DestroyWindow(hwnd_);
			hwnd_ = nullptr;
		}
	}

	void WinWindow::DecodeRawInput_(HRAWINPUT hRawInput, WinWindow* win)
	{
		if (!win) return;

		UINT size = 0;
		GetRawInputData(hRawInput, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
		if (!size) return;

		uint8_t stackBuf[512];
		uint8_t* buf = stackBuf;

		if (size > sizeof(stackBuf))
			buf = (uint8_t*)_alloca(size);

		if (GetRawInputData(hRawInput, RID_INPUT, buf, &size, sizeof(RAWINPUTHEADER)) != size)
			return;

		const RAWINPUT* ri = reinterpret_cast<const RAWINPUT*>(buf);

		if (ri->header.dwType == RIM_TYPEMOUSE)
		{
			const RAWMOUSE& m = ri->data.mouse;
			platform::RawMouseEvent e{};
			e.dx = m.lLastX;
			e.dy = m.lLastY;

			if (m.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)  e.buttonDownMask |= 0x1;
			if (m.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)    e.buttonUpMask |= 0x1;
			if (m.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) e.buttonDownMask |= 0x2;
			if (m.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)   e.buttonUpMask |= 0x2;
			if (m.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) e.buttonDownMask |= 0x4;
			if (m.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)   e.buttonUpMask |= 0x4;

			if (m.usButtonFlags & RI_MOUSE_WHEEL)
				e.wheel = (short)m.usButtonData;

			if (win->rawSink_) win->rawSink_->OnRawMouse(e);
		}
		else if (ri->header.dwType == RIM_TYPEKEYBOARD)
		{
			const RAWKEYBOARD& k = ri->data.keyboard;

			platform::RawKeyboardEvent e{};
			e.vkey = (uint16_t)k.VKey;
			e.down = (k.Flags & RI_KEY_BREAK) == 0;

			if (win->rawSink_) win->rawSink_->OnRawKeyboard(e);
		}
	}


	LRESULT CALLBACK WinWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		// Associate the WinWindow* with the HWND on WM_NCCREATE.
		if (msg == WM_NCCREATE)
		{
			auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
			auto* win = reinterpret_cast<WinWindow*>(cs->lpCreateParams);
			SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(win));
			return DefWindowProcW(hWnd, msg, wParam, lParam);
		}

		auto* win = reinterpret_cast<WinWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

		switch (msg)
		{
		case WM_CLOSE:
			if (win) win->RequestQuit();
			return 0;

		case WM_DESTROY:
			if (win) win->RequestQuit();
			PostQuitMessage(0);
			return 0;

		case WM_INPUT:
			if (win)
				DecodeRawInput_((HRAWINPUT)lParam, win);
			return 0;

		case WM_KILLFOCUS:
			if (win && win->rawSink_)
				win->rawSink_->OnFocusLost();
			return 0;

		default:
			return DefWindowProcW(hWnd, msg, wParam, lParam);
		}
	}
}
```

### Listing 78 — `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Runtime/Engine.h` (UPDATED: owns InputSystem)
```cpp
#pragma once
#include "Core/BuildConfig.h"
#include "Core/Subsystems/SubsystemRegistry.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/LinearArena.h"

#if NOC_ENABLE_ASSERTS
#include "Core/Memory/DebugAlloc.h"
#endif

#include "Resources/VirtualFileSystem.h"
#include "Resources/ResourceManager.h"
#include "EngineConfig.h"
#include "Core/Jobs/JobSystem.h"

#include "Input/InputSystem.h"


namespace noc {

    class WinWindow;
    class MainLoop;

    class Engine
    {
    public:
        // Config access: only valid BEFORE Init().
        EngineConfig& ConfigMutable();
        const EngineConfig& Config() const { return cfg_; }

        bool SetContentRoot(const char* path);
        bool SetOverrideRoot(const char* path);
        bool SetArchivePath(const char* path);

        bool Init();
        void TickOnce();
        void Shutdown();

        int Run();
        void BeginFrame();
        void Tick();
        void EndFrame();

        IAllocator& Allocator();
        LinearArena& FrameArena();

        VirtualFileSystem& VFS() { return vfs_; }
        ResourceManager& Resources() { return resources_; }
        const ResourceManager& Resources() const { return resources_; }

		InputSystem& Input() { return input_; }
		const InputSystem& Input() const { return input_; }

        JobSystem& Jobs() { return jobs_; }
        const JobSystem& Jobs() const { return jobs_; }

        bool InitMemory();
        void KillMemory();

		bool AttachWindow(WinWindow& window);

    private:
        bool IsConfigMutable() const { return !initialized_; }

    private:
        SubsystemRegistry registry_;

        MallocAllocator baseAlloc_;

#if NOC_ENABLE_ASSERTS
        DebugAlloc debugAlloc_{ baseAlloc_ };
        IAllocator* alloc_ = &debugAlloc_;
#else
        IAllocator* alloc_ = &baseAlloc_;
#endif

        void* frameArenaMem_ = nullptr;
        LinearArena frameArena_;

        VirtualFileSystem vfs_;
        JobSystem jobs_;
        ResourceManager resources_;
        InputSystem input_;


        EngineConfig cfg_{};
        bool initialized_ = false;
    };

} // namespace noc
```

### Listing 79 — `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Runtime/Engine.cpp` (UPDATED: Input init + tick integration)
```cpp
#include "Engine.h"

#include <algorithm>
#include <span>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Clock.h"

#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h"

namespace noc {

    IAllocator& Engine::Allocator() { return *alloc_; }
    LinearArena& Engine::FrameArena() { return frameArena_; }

    EngineConfig& Engine::ConfigMutable()
    {
        if (initialized_)
        {
            NOC_LOG_ERROR("Runtime", "EngineConfig is frozen after Init(). Modify config before calling Init().");
            return cfg_;
        }
        return cfg_;
    }

    bool Engine::SetContentRoot(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_ERROR("Runtime", "SetContentRoot() called after Init(); ignored.");
            return false;
        }
        cfg_.contentRoot = path;
        return true;
    }

    bool Engine::SetOverrideRoot(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_ERROR("Runtime", "SetOverrideRoot() called after Init(); ignored.");
            return false;
        }
        cfg_.overrideRoot = path;
        return true;
    }

    bool Engine::SetArchivePath(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_ERROR("Runtime", "SetArchivePath() called after Init(); ignored.");
            return false;
        }
        cfg_.archivePath = path;
        return true;
    }

    bool Engine::InitMemory()
    {
        constexpr std::size_t kFrameArenaBytes = 8 * 1024 * 1024; // Design choice
        frameArenaMem_ = Allocator().Allocate(kFrameArenaBytes, 64);
        frameArena_.Init(frameArenaMem_, kFrameArenaBytes);

        NOC_LOG_INFO("Core", "Memory system initialized (frame arena=%zu bytes)", kFrameArenaBytes);
        return true;
    }

    void Engine::KillMemory()
    {
        if (frameArenaMem_)
        {
            Allocator().Deallocate(frameArenaMem_);
            frameArenaMem_ = nullptr;
        }

#if NOC_ENABLE_ASSERTS
        NOC_LOG_INFO("Core", "Memory stats: total=%zu outstanding=%zu allocs=%zu",
            debugAlloc_.TotalAllocatedBytes(),
            debugAlloc_.OutstandingBytes(),
            debugAlloc_.AllocationCount());
#endif
    }

    static bool StartupLog(void*)
    {
        noc::GetLogger().Init();
        NOC_LOG_INFO("Core", "Logger initialized");
        return true;
    }

    static void ShutdownLog(void*)
    {
        NOC_LOG_INFO("Core", "Logger shutting down");
        noc::GetLogger().Shutdown();
    }

    static bool StartupTime(void*)
    {
        noc::GetTime().Init();
        NOC_LOG_INFO("Core", "Time system initialized");
        return true;
    }

    static void ShutdownTime(void*)
    {
        NOC_LOG_INFO("Core", "Time system shutting down");
    }

    static bool StartupAssert(void*)
    {
        NOC_LOG_INFO("Core", "Assert system initialized");
        return true;
    }

    static void ShutdownAssert(void*)
    {
        NOC_LOG_INFO("Core", "Assert system shutting down");
    }

    static bool StartupMemory(void* ctx)
    {
        auto* e = static_cast<noc::Engine*>(ctx);
        return e->InitMemory();
    }

    static void ShutdownMemory(void* ctx)
    {
        NOC_LOG_INFO("Core", "Memory system shutting down");
        auto* e = static_cast<noc::Engine*>(ctx);
        e->KillMemory();
    }

    static bool StartupWindow(void* ctx)
    {
        auto* e = static_cast<Engine*>(ctx);
        (void)e;
        NOC_LOG_INFO("Win32", "Window subsystem ready");
        return true;
    }

    static void ShutdownWindow(void*)
    {
        NOC_LOG_INFO("Win32", "Window subsystem shutdown");
    }

    static bool StartupJobs(void* ctx)
    {
        auto* e = static_cast<noc::Engine*>(ctx);
        // Design choice: worker count = HW threads - 1 (leave room for main thread), clamped.
        const uint32_t hw = (std::max)(1u, std::thread::hardware_concurrency());
        const uint32_t workers = (hw > 1) ? (hw - 1) : 1;
        return e->Jobs().Init(workers);
    }

    static void ShutdownJobs(void* ctx)
    {
        auto* e = static_cast<noc::Engine*>(ctx);
        e->Jobs().Shutdown();
    }

    bool Engine::Init()
    {
        std::span<const char* const> depsLog{};

        static const char* kDepsNeedLog[] = { "Log" };
        std::span<const char* const> depsNeedLog{ kDepsNeedLog, 1 };

        static const char* kDepsJobs[] = { "Log", "Memory" };
        std::span<const char* const> depsJobs{ kDepsJobs, 2 };

        registry_.Register(SubsystemDesc{ "Log",    depsLog,     &StartupLog,    &ShutdownLog });
        registry_.Register(SubsystemDesc{ "Time",   depsNeedLog, &StartupTime,   &ShutdownTime });
        registry_.Register(SubsystemDesc{ "Memory", depsNeedLog, &StartupMemory, &ShutdownMemory });
        registry_.Register(SubsystemDesc{ "Assert", depsNeedLog, &StartupAssert, &ShutdownAssert });
        registry_.Register(SubsystemDesc{ "Window", depsNeedLog, &StartupWindow, &ShutdownWindow });
        registry_.Register(SubsystemDesc{ "Jobs",   depsJobs,    &StartupJobs,   &ShutdownJobs });

        if (!registry_.StartupAll(this))
            return false;

        // Phase 3 policy: later mounts override earlier mounts.
        if (cfg_.archivePath && cfg_.archivePath[0] != 0)
        {
            if (!vfs_.MountArchive(cfg_.archivePath))
                NOC_LOG_WARN("VFS", "Failed to mount archivePath: %s", cfg_.archivePath);
        }

        if (cfg_.contentRoot && cfg_.contentRoot[0] != 0)
        {
            if (!vfs_.MountLooseDirectory(cfg_.contentRoot))
                NOC_LOG_WARN("VFS", "Failed to mount contentRoot: %s", cfg_.contentRoot);
        }

        if (cfg_.overrideRoot && cfg_.overrideRoot[0] != 0)
        {
            if (!vfs_.MountLooseDirectory(cfg_.overrideRoot))
                NOC_LOG_WARN("VFS", "Failed to mount overrideRoot: %s", cfg_.overrideRoot);
        }

        // Phase 6: ResourceManager uses JobSystem (must be after jobs + VFS mounts).
        if (!resources_.Init(*this, vfs_))
            return false;

		// Phase 7: InputSystem init (HWND comes later in AttachWindow).
		if (!input_.Init(*this))
			return false;

        initialized_ = true;
        return true;
    }

    bool Engine::AttachWindow(WinWindow& window)
    {
        // Forward WM_INPUT + focus loss -> InputSystem.
        window.SetRawInputSink(input_.RawSink());

        // Register Raw Input devices (requires HWND).
        if (!input_.AttachToWindow(window.Handle()))
            return false;

        return true;
    }

    int Engine::Run()
    {
        WinWindow window;
        WinWindowDesc wd{};
        wd.title = L"NocturneHost";
        wd.width = 1280;
        wd.height = 720;
        wd.resizable = true;

        if (!window.Create(wd))
        {
            NOC_LOG_FATAL("Win32", "Failed to create window");
            return -1;
        }

        if (!AttachWindow(window))
        {
            NOC_LOG_FATAL("Runtime", "Failed to attach InputSystem to window");
            window.Destroy();
            return -1;
		}

        MainLoop loop;
        loop.Run(*this, window);

        window.Destroy();
        return 0;
    }

    void Engine::BeginFrame()
    {
        GetTime().BeginFrame();
        FrameArena().Reset();
		input_.BeginFrame();
    }

    void Engine::Tick()
    {
		input_.Update();
        resources_.Update();
    }

    void Engine::EndFrame()
    {
        GetTime().EndFrame();
    }

    void Engine::TickOnce()
    {
        GetTime().BeginFrame();
        FrameArena().Reset();

        void* a = FrameArena().Allocate(256, 16);
        void* b = FrameArena().Allocate(1024, 64);
        (void)a; (void)b;

        GetTime().EndFrame();

        NOC_LOG_INFO("Core", "TickOnce() dt=%.6f sec arenaUsed=%zu bytes",
            GetTime().DeltaSeconds(),
            FrameArena().Used());
    }

    void Engine::Shutdown()
    {
        // Resource manager must shutdown while jobs + memory + log still exist.
        resources_.Shutdown();
		input_.Shutdown();
        registry_.ShutdownAll(this);
    }

} // namespace noc
```

### Listing 80 — `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Runtime/MainLoop.h` (unchanged structure; shown for completeness)
```cpp
#pragma once

namespace noc
{
	class Engine;
	class WinWindow;

	class MainLoop
	{
	public:
		void Run(Engine& engine, WinWindow& window);
	};
}
```

### Listing 81 — `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Engine/Runtime/MainLoop.cpp` (unchanged; shown for completeness)
```cpp
#include "MainLoop.h"
#include "Runtime/Engine.h"
#include "Platform/Win32/WinWindow.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace noc
{
	static void PumpMessagesNonBlocking()
	{
		MSG msg{};
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	void MainLoop::Run(Engine& engine, WinWindow& window)
	{
		while (!window.ShouldQuit())
		{
			PumpMessagesNonBlocking();

			engine.BeginFrame();
			engine.Tick();
			engine.EndFrame();
		}
	}
}
```

### Listing 82 — `Phase 7 — Input System (Raw Input + Snapshots + Action Mapping).md` — C++ Implementations (Phase 7) > `Apps/NocturneHost/main.cpp`
```cpp
#include "Runtime/Engine.h"
#include "Core/Log.h"

#ifndef NOC_CONTENT_ROOT
#define NOC_CONTENT_ROOT "."
#endif

int main()
{
    noc::Engine engine;

    // ---- Engine configuration (pre-Init only) ----
    engine.SetContentRoot(NOC_CONTENT_ROOT);

    // ---- Engine startup ----
    if (!engine.Init())
    {
        NOC_LOG_FATAL("Host", "Engine initialization failed");
        return -1;
    }

    // ---- Run application (creates window + main loop) ----
    const int exitCode = engine.Run();

    // ---- Shutdown ----
    engine.Shutdown();

    return exitCode;
}
```

### Listing 83 — `Phase 8 — Rendering Bootstrap (DirectX 12).md` — Implementations (Phase 8) > `Engine/Render/RenderSystem.h`
```cpp
#pragma once
#include <cstdint>

namespace noc
{
	// Minimal render bootstrap system (DX12 implementation lives in .cpp).
	// Public header stays platform-agnostic: only uses void* for HWND.
	class RenderSystem
	{
	public:
		RenderSystem() = default;

		// Engine lifecycle.
		bool Init(bool enableDebugLayer);
		void Shutdown();

		// Must be called after window creation (needs HWND + client size).
		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		// Per-frame.
		void BeginFrame();
		void EndFramePresent(); // clears + presents

	private:
		struct Impl;
		Impl* impl_ = nullptr;
	};
}
````

---

## `Engine/Render/RenderSystem.cpp`

```cpp
#include "RenderSystem.h"

#include "Core/Log.h"
#include "Core/Assert.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

namespace noc
{
    using Microsoft::WRL::ComPtr;

    static inline void SafeCloseHandle(HANDLE& h)
    {
        if (h)
        {
            CloseHandle(h);
            h = nullptr;
        }
    }

    static inline bool HrOk(HRESULT hr, const char* what)
    {
        if (SUCCEEDED(hr))
            return true;

        NOC_LOG_ERROR("Render", "%s failed (hr=0x%08X)", what, (unsigned)hr);
        return false;
    }

    struct RenderSystem::Impl
    {
        static constexpr uint32_t kFrameCount = 2;

        bool debugLayer = false;

        ComPtr<IDXGIFactory6> factory;
        ComPtr<IDXGIAdapter1> adapter;

        ComPtr<ID3D12Device> device;
        ComPtr<ID3D12CommandQueue> queue;

        ComPtr<IDXGISwapChain3> swapChain;

        ComPtr<ID3D12DescriptorHeap> rtvHeap;
        uint32_t rtvDescriptorSize = 0;

        ComPtr<ID3D12Resource> backBuffers[kFrameCount];

        ComPtr<ID3D12CommandAllocator> cmdAlloc[kFrameCount];
        ComPtr<ID3D12GraphicsCommandList> cmdList;

        ComPtr<ID3D12Fence> fence;
        uint64_t fenceValues[kFrameCount] = {}; // indexed by back buffer
        HANDLE fenceEvent = nullptr;

        uint32_t frameIndex = 0; // current swapchain backbuffer index

        uint32_t clientW = 0;
        uint32_t clientH = 0;
        HWND hwnd = nullptr;

        bool frameOpen = false;

        // Track backbuffer state to avoid barrier mismatch
        D3D12_RESOURCE_STATES bbState[kFrameCount] = {
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_PRESENT
        };

        void LogDeviceRemoved_(const char* where) const
        {
            if (!device) return;
            HRESULT reason = device->GetDeviceRemovedReason();
            if (reason != S_OK)
                NOC_LOG_ERROR("Render", "Device removed reason at %s: hr=0x%08X", where, (unsigned)reason);
        }

        bool CreateFactory_()
        {
#if defined(_DEBUG)
            UINT flags = 0;
            if (debugLayer)
                flags |= DXGI_CREATE_FACTORY_DEBUG;
            return HrOk(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");
#else
            return HrOk(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");
#endif
        }

        bool PickAdapter_()
        {
            ComPtr<IDXGIAdapter1> best;

            for (UINT i = 0;; ++i)
            {
                ComPtr<IDXGIAdapter1> a;
                if (factory->EnumAdapters1(i, &a) == DXGI_ERROR_NOT_FOUND)
                    break;

                DXGI_ADAPTER_DESC1 desc{};
                a->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    continue;

                if (SUCCEEDED(D3D12CreateDevice(a.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
                    best = a;
                    break;
                }
            }

            if (!best)
            {
                NOC_LOG_ERROR("Render", "No suitable hardware DX12 adapter found");
                return false;
            }

            adapter = best;
            return true;
        }

        bool CreateDevice_()
        {
#if defined(_DEBUG)
            if (debugLayer)
            {
                ComPtr<ID3D12Debug> dbg;
                if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg))))
                {
                    dbg->EnableDebugLayer();
                    NOC_LOG_INFO("Render", "D3D12 debug layer enabled");
                }
                else
                {
                    NOC_LOG_WARN("Render", "D3D12 debug layer requested but not available");
                }
            }
#endif
            return HrOk(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "D3D12CreateDevice");
        }

        bool CreateQueue_()
        {
            D3D12_COMMAND_QUEUE_DESC q{};
            q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            q.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
            q.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            q.NodeMask = 0;

            return HrOk(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)), "CreateCommandQueue");
        }

        bool CreateSwapChain_(HWND h, uint32_t w, uint32_t hh)
        {
            hwnd = h;
            clientW = w;
            clientH = hh;

            DXGI_SWAP_CHAIN_DESC1 sc{};
            sc.BufferCount = kFrameCount;
            sc.Width = w;
            sc.Height = hh;
            sc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            sc.SampleDesc.Count = 1;

            ComPtr<IDXGISwapChain1> sc1;
            if (!HrOk(factory->CreateSwapChainForHwnd(
                queue.Get(),
                hwnd,
                &sc,
                nullptr,
                nullptr,
                &sc1), "CreateSwapChainForHwnd"))
            {
                return false;
            }

            factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

            if (!HrOk(sc1.As(&swapChain), "SwapChain1.As(SwapChain3)"))
                return false;

            frameIndex = swapChain->GetCurrentBackBufferIndex();
            return true;
        }

        bool CreateRtvHeapAndViews_()
        {
            D3D12_DESCRIPTOR_HEAP_DESC hd{};
            hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            hd.NumDescriptors = kFrameCount;
            hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            if (!HrOk(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvHeap)), "CreateDescriptorHeap(RTV)"))
                return false;

            rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            D3D12_CPU_DESCRIPTOR_HANDLE base = rtvHeap->GetCPUDescriptorHandleForHeapStart();

            for (uint32_t i = 0; i < kFrameCount; ++i)
            {
                if (!HrOk(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])), "SwapChain.GetBuffer"))
                    return false;

                D3D12_CPU_DESCRIPTOR_HANDLE h = base;
                h.ptr += (SIZE_T)i * (SIZE_T)rtvDescriptorSize;
                device->CreateRenderTargetView(backBuffers[i].Get(), nullptr, h);

                bbState[i] = D3D12_RESOURCE_STATE_PRESENT;
            }

            return true;
        }

        bool CreateCommands_()
        {
            for (uint32_t i = 0; i < kFrameCount; ++i)
            {
                if (!HrOk(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc[i])),
                    "CreateCommandAllocator"))
                {
                    return false;
                }
            }

            // Create list using allocator for current frameIndex.
            if (!HrOk(device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                cmdAlloc[frameIndex].Get(),
                nullptr,
                IID_PPV_ARGS(&cmdList)), "CreateCommandList"))
            {
                return false;
            }

            // Close it; BeginFrame will Reset it.
            if (!HrOk(cmdList->Close(), "cmdList->Close (initial)"))
                return false;

            return true;
        }

        bool CreateFence_()
        {
            // Create fence with initial value fenceValues[frameIndex] (0).
            if (!HrOk(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "CreateFence"))
                return false;

            for (uint32_t i = 0; i < kFrameCount; ++i)
                fenceValues[i] = 0;

            fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!fenceEvent)
            {
                NOC_LOG_ERROR("Render", "CreateEvent failed (err=%lu)", GetLastError());
                return false;
            }

            // Match the sample: bump the current frame's fence value once
            // so first MoveToNextFrame() has currentFenceValue >= 1.
            fenceValues[frameIndex] = 1;
            return true;
        }

        void MoveToNextFrame_()
        {
            // Signal for the frame we just submitted.
            const uint64_t currentFenceValue = fenceValues[frameIndex];
            HRESULT hr = queue->Signal(fence.Get(), currentFenceValue);
            if (FAILED(hr))
            {
                NOC_LOG_ERROR("Render", "MoveToNextFrame_: Signal failed (hr=0x%08X)", (unsigned)hr);
                LogDeviceRemoved_("MoveToNextFrame_:Signal");
                return;
            }

            // Update to the next back buffer.
            frameIndex = swapChain->GetCurrentBackBufferIndex();

            // Wait until that back buffer is ready.
            if (fence->GetCompletedValue() < fenceValues[frameIndex])
            {
                hr = fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent);
                if (FAILED(hr))
                {
                    NOC_LOG_ERROR("Render", "MoveToNextFrame_: SetEventOnCompletion failed (hr=0x%08X)", (unsigned)hr);
                    LogDeviceRemoved_("MoveToNextFrame_:SetEventOnCompletion");
                    return;
                }
                WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
            }

            // Fence value for the next time we render this buffer.
            fenceValues[frameIndex] = currentFenceValue + 1;
        }

        void WaitForGpu_()
        {
            // Flush: signal and wait using current frameIndex
            const uint64_t v = fenceValues[frameIndex];
            queue->Signal(fence.Get(), v);
            fence->SetEventOnCompletion(v, fenceEvent);
            WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
            fenceValues[frameIndex] = v + 1;
        }

        void Shutdown_()
        {
            if (device && queue && fence && fenceEvent)
            {
                WaitForGpu_();
            }

            SafeCloseHandle(fenceEvent);

            for (uint32_t i = 0; i < kFrameCount; ++i)
            {
                backBuffers[i].Reset();
                cmdAlloc[i].Reset();
            }

            cmdList.Reset();
            rtvHeap.Reset();
            swapChain.Reset();
            queue.Reset();
            fence.Reset();
            device.Reset();
            adapter.Reset();
            factory.Reset();

            hwnd = nullptr;
            clientW = clientH = 0;
        }
    };

    bool RenderSystem::Init(bool enableDebugLayer)
    {
        if (impl_)
            return true;

        impl_ = new Impl();
        impl_->debugLayer = enableDebugLayer;

        NOC_LOG_INFO("Render", "RenderSystem initialized (awaiting AttachToWindow)");
        return true;
    }

    void RenderSystem::Shutdown()
    {
        if (!impl_)
            return;

        impl_->Shutdown_();
        delete impl_;
        impl_ = nullptr;

        NOC_LOG_INFO("Render", "RenderSystem shutdown");
    }

    bool RenderSystem::AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
    {
        if (!impl_)
            return false;

        if (!nativeHwnd || clientWidth == 0 || clientHeight == 0)
        {
            NOC_LOG_ERROR("Render", "AttachToWindow invalid args");
            return false;
        }

        if (!impl_->CreateFactory_()) return false;
        if (!impl_->PickAdapter_()) return false;
        if (!impl_->CreateDevice_()) return false;
        if (!impl_->CreateQueue_()) return false;
        if (!impl_->CreateSwapChain_((HWND)nativeHwnd, clientWidth, clientHeight)) return false;
        if (!impl_->CreateRtvHeapAndViews_()) return false;
        if (!impl_->CreateCommands_()) return false;
        if (!impl_->CreateFence_()) return false;

        NOC_LOG_INFO("Render", "DX12 ready (%ux%u, buffers=%u)", clientWidth, clientHeight, Impl::kFrameCount);
        return true;
    }

    void RenderSystem::BeginFrame()
    {
        if (!impl_ || !impl_->device) return;
        if (impl_->device->GetDeviceRemovedReason() != S_OK) return;

        if (impl_->frameOpen)
        {
            NOC_LOG_ERROR("Render", "BeginFrame called while a frame is already open. Ignoring.");
            return;
        }
        impl_->frameOpen = true;

        // Reset allocator for current backbuffer index.
        HRESULT hr = impl_->cmdAlloc[impl_->frameIndex]->Reset();
        if (FAILED(hr))
        {
            NOC_LOG_ERROR("Render", "cmdAlloc->Reset failed (hr=0x%08X)", (unsigned)hr);
            impl_->LogDeviceRemoved_("cmdAlloc->Reset");
            impl_->frameOpen = false;
            return;
        }

        hr = impl_->cmdList->Reset(impl_->cmdAlloc[impl_->frameIndex].Get(), nullptr);
        if (FAILED(hr))
        {
            NOC_LOG_ERROR("Render", "cmdList->Reset failed (hr=0x%08X)", (unsigned)hr);
            impl_->LogDeviceRemoved_("cmdList->Reset");
            impl_->frameOpen = false;
            return;
        }
    }

    void RenderSystem::EndFramePresent()
    {
        if (!impl_ || !impl_->device) return;
        if (impl_->device->GetDeviceRemovedReason() != S_OK) return;

        if (!impl_->frameOpen)
        {
            NOC_LOG_ERROR("Render", "EndFramePresent called with no open frame. Ignoring.");
            return;
        }

        struct FrameCloseGuard { bool& open; ~FrameCloseGuard() { open = false; } } guard{ impl_->frameOpen };

        const uint32_t i = impl_->frameIndex;

        // RTV handle
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = impl_->rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += (SIZE_T)i * (SIZE_T)impl_->rtvDescriptorSize;

        auto Transition = [&](D3D12_RESOURCE_STATES to)
            {
                if (impl_->bbState[i] == to) return;

                D3D12_RESOURCE_BARRIER b{};
                b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b.Transition.pResource = impl_->backBuffers[i].Get();
                b.Transition.StateBefore = impl_->bbState[i];
                b.Transition.StateAfter = to;
                b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

                impl_->cmdList->ResourceBarrier(1, &b);
                impl_->bbState[i] = to;
            };

        Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);

        const float clear[4] = { 0.02f, 0.02f, 0.05f, 1.0f };
        impl_->cmdList->ClearRenderTargetView(rtv, clear, 0, nullptr);

        Transition(D3D12_RESOURCE_STATE_PRESENT);

        HRESULT hr = impl_->cmdList->Close();
        if (FAILED(hr))
        {
            NOC_LOG_ERROR("Render", "cmdList->Close failed (hr=0x%08X)", (unsigned)hr);
            impl_->LogDeviceRemoved_("cmdList->Close");
            return;
        }

        ID3D12CommandList* lists[] = { impl_->cmdList.Get() };
        impl_->queue->ExecuteCommandLists(1, lists);

        hr = impl_->swapChain->Present(1, 0);
        if (FAILED(hr))
        {
            NOC_LOG_ERROR("Render", "swapChain->Present failed (hr=0x%08X)", (unsigned)hr);
            impl_->LogDeviceRemoved_("Present");
            return;
        }

        impl_->MoveToNextFrame_();
    }

} // namespace noc
```

### Listing 84 — `Phase 8 — Rendering Bootstrap (DirectX 12).md` — Implementations (Phase 8) > `Engine/Platform/Win32/WinWindow.h` (MODIFIED: expose client size)
```cpp
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>

namespace noc
{
	namespace platform
	{
		struct RawMouseEvent
		{
			int dx = 0;
			int dy = 0;
			int wheel = 0;
			uint16_t buttonDownMask = 0; // bit0=L, bit1=R, bit2=M
			uint16_t buttonUpMask = 0;
		};

		struct RawKeyboardEvent
		{
			uint16_t vkey = 0;
			bool down = false;
		};

		struct IRawInputSink
		{
			virtual ~IRawInputSink() = default;
			virtual void OnRawMouse(const RawMouseEvent& e) = 0;
			virtual void OnRawKeyboard(const RawKeyboardEvent& e) = 0;
			virtual void OnFocusLost() = 0;
		};
	}

	struct WinWindowDesc
	{
		const wchar_t* title = L"Nocturne";
		int width = 1280;
		int height = 720;
		bool resizable = true;
	};

	class WinWindow
	{
	public:
		bool Create(const WinWindowDesc& desc);
		void Destroy();

		bool ShouldQuit() const { return shouldQuit_; }
		void RequestQuit() { shouldQuit_ = true; }

		void* Handle() const { return (void*)hwnd_; }

		// Client size (updated on WM_SIZE).
		uint32_t ClientWidth() const { return clientW_; }
		uint32_t ClientHeight() const { return clientH_; }

		// Phase 7: forward raw input to engine input system through an abstract sink.
		void SetRawInputSink(platform::IRawInputSink* sink) { rawSink_ = sink; }

	private:
		static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

		void RegisterClassOnce();
		void ApplyClientSize(int clientW, int clientH, bool resizable);

		static void DecodeRawInput_(HRAWINPUT hRawInput, WinWindow* win);

	private:
		HWND hwnd_ = nullptr;
		bool shouldQuit_ = false;

		uint32_t clientW_ = 0;
		uint32_t clientH_ = 0;

		platform::IRawInputSink* rawSink_ = nullptr;
	};
}
```

### Listing 85 — `Phase 8 — Rendering Bootstrap (DirectX 12).md` — Implementations (Phase 8) > `Engine/Platform/Win32/WinWindow.cpp` (MODIFIED: track WM_SIZE)
```cpp
#include <malloc.h>
#include "WinWindow.h"
#include "Core/Log.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace noc
{
	static const wchar_t* kWndClassName = L"NocturneWndClass";

	void WinWindow::RegisterClassOnce()
	{
		static bool registered = false;
		if (registered)
			return;

		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = &WinWindow::WndProc;
		wc.hInstance = GetModuleHandleW(nullptr);
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		wc.lpszClassName = kWndClassName;

		if (!RegisterClassExW(&wc))
		{
			NOC_LOG_FATAL("Win32", "RegisterClassExW failed (err=%lu)", GetLastError());
		}

		registered = true;
	}

	void WinWindow::ApplyClientSize(int clientW, int clientH, bool resizable)
	{
		DWORD style = WS_OVERLAPPEDWINDOW;
		if (!resizable)
		{
			style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
		}

		RECT r{ 0, 0, clientW, clientH };
		AdjustWindowRect(&r, style, FALSE);

		const int winW = r.right - r.left;
		const int winH = r.bottom - r.top;

		SetWindowLongPtrW(hwnd_, GWL_STYLE, (LONG_PTR)style);
		SetWindowPos(hwnd_, nullptr, 100, 100, winW, winH, SWP_NOZORDER | SWP_FRAMECHANGED);
	}

	bool WinWindow::Create(const WinWindowDesc& desc)
	{
		RegisterClassOnce();

		HINSTANCE hInst = GetModuleHandleW(nullptr);

		hwnd_ = CreateWindowExW(
			0,
			kWndClassName,
			desc.title,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			desc.width, desc.height,
			nullptr, nullptr,
			hInst,
			this
		);

		if (!hwnd_)
		{
			NOC_LOG_FATAL("Win32", "CreateWindowExW failed (err=%lu)", GetLastError());
			return false;
		}

		ApplyClientSize(desc.width, desc.height, desc.resizable);

		clientW_ = (uint32_t)desc.width;
		clientH_ = (uint32_t)desc.height;

		ShowWindow(hwnd_, SW_SHOW);
		UpdateWindow(hwnd_);

		return true;
	}

	void WinWindow::Destroy()
	{
		if (hwnd_)
		{
			DestroyWindow(hwnd_);
			hwnd_ = nullptr;
		}
	}

	void WinWindow::DecodeRawInput_(HRAWINPUT hRawInput, WinWindow* win)
	{
		if (!win) return;

		UINT size = 0;
		GetRawInputData(hRawInput, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
		if (!size) return;

		uint8_t stackBuf[512];
		uint8_t* buf = stackBuf;

		if (size > sizeof(stackBuf))
			buf = (uint8_t*)_alloca(size);

		if (GetRawInputData(hRawInput, RID_INPUT, buf, &size, sizeof(RAWINPUTHEADER)) != size)
			return;

		const RAWINPUT* ri = reinterpret_cast<const RAWINPUT*>(buf);

		if (ri->header.dwType == RIM_TYPEMOUSE)
		{
			const RAWMOUSE& m = ri->data.mouse;
			platform::RawMouseEvent e{};
			e.dx = m.lLastX;
			e.dy = m.lLastY;

			if (m.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)  e.buttonDownMask |= 0x1;
			if (m.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)    e.buttonUpMask |= 0x1;
			if (m.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) e.buttonDownMask |= 0x2;
			if (m.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)   e.buttonUpMask |= 0x2;
			if (m.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) e.buttonDownMask |= 0x4;
			if (m.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)   e.buttonUpMask |= 0x4;

			if (m.usButtonFlags & RI_MOUSE_WHEEL)
				e.wheel = (short)m.usButtonData;

			if (win->rawSink_) win->rawSink_->OnRawMouse(e);
		}
		else if (ri->header.dwType == RIM_TYPEKEYBOARD)
		{
			const RAWKEYBOARD& k = ri->data.keyboard;

			platform::RawKeyboardEvent e{};
			e.vkey = (uint16_t)k.VKey;
			e.down = (k.Flags & RI_KEY_BREAK) == 0;

			if (win->rawSink_) win->rawSink_->OnRawKeyboard(e);
		}
	}

	LRESULT CALLBACK WinWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == WM_NCCREATE)
		{
			auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
			auto* win = reinterpret_cast<WinWindow*>(cs->lpCreateParams);
			SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(win));
			return DefWindowProcW(hWnd, msg, wParam, lParam);
		}

		auto* win = reinterpret_cast<WinWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

		switch (msg)
		{
		case WM_CLOSE:
			if (win) win->RequestQuit();
			return 0;

		case WM_DESTROY:
			if (win) win->RequestQuit();
			PostQuitMessage(0);
			return 0;

		case WM_INPUT:
			if (win)
				DecodeRawInput_((HRAWINPUT)lParam, win);
			return 0;

		case WM_KILLFOCUS:
			if (win && win->rawSink_)
				win->rawSink_->OnFocusLost();
			return 0;

		case WM_SIZE:
			if (win)
			{
				const UINT w = LOWORD(lParam);
				const UINT h = HIWORD(lParam);
				win->clientW_ = (uint32_t)w;
				win->clientH_ = (uint32_t)h;
			}
			return 0;

		default:
			return DefWindowProcW(hWnd, msg, wParam, lParam);
		}
	}
}
```

### Listing 86 — `Phase 8 — Rendering Bootstrap (DirectX 12).md` — Implementations (Phase 8) > `Engine/Runtime/Engine.h` (MODIFIED: owns RenderSystem)
```cpp
#pragma once
#include "Core/BuildConfig.h"
#include "Core/Subsystems/SubsystemRegistry.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/LinearArena.h"

#if NOC_ENABLE_ASSERTS
#include "Core/Memory/DebugAlloc.h"
#endif

#include "Resources/VirtualFileSystem.h"
#include "Resources/ResourceManager.h"
#include "EngineConfig.h"
#include "Core/Jobs/JobSystem.h"
#include "Input/InputSystem.h"
#include "Render/RenderSystem.h"

namespace noc {

	class WinWindow;
	class MainLoop;

	class Engine
	{
	public:
		// Config access: only valid BEFORE Init().
		EngineConfig& ConfigMutable();
		const EngineConfig& Config() const { return cfg_; }

		bool SetContentRoot(const char* path);
		bool SetOverrideRoot(const char* path);
		bool SetArchivePath(const char* path);

		bool Init();
		void TickOnce();
		void Shutdown();

		int Run();
		void BeginFrame();
		void Tick();
		void EndFrame();

		IAllocator& Allocator();
		LinearArena& FrameArena();

		VirtualFileSystem& VFS() { return vfs_; }
		ResourceManager& Resources() { return resources_; }
		const ResourceManager& Resources() const { return resources_; }

		InputSystem& Input() { return input_; }
		const InputSystem& Input() const { return input_; }

		JobSystem& Jobs() { return jobs_; }
		const JobSystem& Jobs() const { return jobs_; }

		bool InitMemory();
		void KillMemory();

		bool AttachWindow(WinWindow& window);

	private:
		bool IsConfigMutable() const { return !initialized_; }

	private:
		SubsystemRegistry registry_;

		MallocAllocator baseAlloc_;

#if NOC_ENABLE_ASSERTS
		DebugAlloc debugAlloc_{ baseAlloc_ };
		IAllocator* alloc_ = &debugAlloc_;
#else
		IAllocator* alloc_ = &baseAlloc_;
#endif

		void* frameArenaMem_ = nullptr;
		LinearArena frameArena_;

		VirtualFileSystem vfs_;
		JobSystem jobs_;
		ResourceManager resources_;
		InputSystem input_;

		RenderSystem render_;

		EngineConfig cfg_{};
		bool initialized_ = false;
	};

} // namespace noc
```

### Listing 87 — `Phase 8 — Rendering Bootstrap (DirectX 12).md` — Implementations (Phase 8) > `Engine/Runtime/Engine.cpp` (MODIFIED: attach + frame integration)
```cpp
#include "Engine.h"

#include <algorithm>
#include <span>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Clock.h"

#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h"

namespace noc {

	IAllocator& Engine::Allocator() { return *alloc_; }
	LinearArena& Engine::FrameArena() { return frameArena_; }

	EngineConfig& Engine::ConfigMutable()
	{
		if (initialized_)
		{
			NOC_LOG_ERROR("Runtime", "EngineConfig is frozen after Init(). Modify config before calling Init().");
			return cfg_;
		}
		return cfg_;
	}

	bool Engine::SetContentRoot(const char* path)
	{
		if (!IsConfigMutable())
		{
			NOC_LOG_ERROR("Runtime", "SetContentRoot() called after Init(); ignored.");
			return false;
		}
		return cfg_.SetContentRoot(path);
	}

	bool Engine::SetOverrideRoot(const char* path)
	{
		if (!IsConfigMutable())
		{
			NOC_LOG_ERROR("Runtime", "SetOverrideRoot() called after Init(); ignored.");
			return false;
		}
		return cfg_.SetOverrideRoot(path);
	}

	bool Engine::SetArchivePath(const char* path)
	{
		if (!IsConfigMutable())
		{
			NOC_LOG_ERROR("Runtime", "SetArchivePath() called after Init(); ignored.");
			return false;
		}
		return cfg_.SetArchivePath(path);
	}

	bool Engine::Init()
	{
		// Phase 1 subsystem registration/startup already exists in your file.
		// Keep that part unchanged; below is only the new Phase 8 wiring.

		// --- existing init path (core + vfs + jobs + resources + input) ---
		if (!registry_.StartupAll(this))
			return false;

		// Phase 8: render system init (no HWND yet).
#if NOC_ENABLE_ASSERTS
		const bool enableDebugLayer = true;
#else
		const bool enableDebugLayer = false;
#endif
		if (!render_.Init(enableDebugLayer))
			return false;

		initialized_ = true;
		return true;
	}

	bool Engine::AttachWindow(WinWindow& window)
	{
		// Forward WM_INPUT + focus loss -> InputSystem.
		window.SetRawInputSink(input_.RawSink());

		// Register Raw Input devices (requires HWND).
		if (!input_.AttachToWindow(window.Handle()))
			return false;

		// Phase 8: DX12 attach (requires HWND + client size).
		if (!render_.AttachToWindow(window.Handle(), window.ClientWidth(), window.ClientHeight()))
			return false;

		return true;
	}

	int Engine::Run()
	{
		WinWindow window;
		WinWindowDesc wd{};
		wd.title = L"NocturneHost";
		wd.width = 1280;
		wd.height = 720;
		wd.resizable = true;

		if (!window.Create(wd))
		{
			NOC_LOG_FATAL("Win32", "Failed to create window");
			return -1;
		}

		if (!AttachWindow(window))
		{
			NOC_LOG_FATAL("Runtime", "Failed to attach systems to window");
			window.Destroy();
			return -1;
		}

		MainLoop loop;
		loop.Run(*this, window);

		window.Destroy();
		return 0;
	}

	void Engine::BeginFrame()
	{
		GetTime().BeginFrame();
		FrameArena().Reset();

		input_.BeginFrame();
		render_.BeginFrame();
	}

	void Engine::Tick()
	{
		input_.Update();
		resources_.Update();
	}

	void Engine::EndFrame()
	{
		// Phase 8: clear + present.
		render_.EndFramePresent();

		GetTime().EndFrame();
	}

	void Engine::TickOnce()
	{
		GetTime().BeginFrame();
		FrameArena().Reset();

		void* a = FrameArena().Allocate(256, 16);
		void* b = FrameArena().Allocate(1024, 64);
		(void)a; (void)b;

		GetTime().EndFrame();

		NOC_LOG_INFO("Core", "TickOnce() dt=%.6f sec arenaUsed=%zu bytes",
			GetTime().DeltaSeconds(),
			FrameArena().Used());
	}

	void Engine::Shutdown()
	{
		// Make sure GPU is idle before tearing down core allocators/logging.
		render_.Shutdown();

		// Resource manager must shutdown while jobs + memory + log still exist.
		resources_.Shutdown();
		input_.Shutdown();
		registry_.ShutdownAll(this);
	}

} // namespace noc
```

### Listing 88 — `Phase 8 — Rendering Bootstrap (DirectX 12).md` — Implementations (Phase 8) > Notes you will likely need in your build system (FYI)
```

### Listing 89 — `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/RenderSystem.h` (UNCHANGED)
```cpp
#pragma once
#include <cstdint>

namespace noc
{
	class RenderSystem
	{
	public:
		RenderSystem() = default;

		bool Init(bool enableDebugLayer);
		void Shutdown();

		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		void BeginFrame();
		void EndFramePresent();

	private:
		struct Impl;
		Impl* impl_ = nullptr;
	};
}
```

### Listing 90 — `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/RenderSystem.cpp` (REPLACED — now small)
```cpp
#include "RenderSystem.h"

#include "Core/Log.h"

#include "Render/DX12/Dx12Renderer.h"

namespace noc
{
	struct RenderSystem::Impl
	{
		Dx12Renderer renderer;
	};

	bool RenderSystem::Init(bool enableDebugLayer)
	{
		if (impl_)
			return true;

		impl_ = new Impl();
		if (!impl_->renderer.Init(enableDebugLayer))
		{
			NOC_LOG_ERROR("Render", "Dx12Renderer::Init failed");
			delete impl_;
			impl_ = nullptr;
			return false;
		}

		NOC_LOG_INFO("Render", "RenderSystem initialized");
		return true;
	}

	void RenderSystem::Shutdown()
	{
		if (!impl_)
			return;

		impl_->renderer.Shutdown();

		delete impl_;
		impl_ = nullptr;

		NOC_LOG_INFO("Render", "RenderSystem shutdown");
	}

	bool RenderSystem::AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!impl_)
			return false;

		return impl_->renderer.AttachToWindow(nativeHwnd, clientWidth, clientHeight);
	}

	void RenderSystem::BeginFrame()
	{
		if (!impl_)
			return;

		impl_->renderer.BeginFrame();
	}

	void RenderSystem::EndFramePresent()
	{
		if (!impl_)
			return;

		impl_->renderer.EndFramePresent();
	}
}
```

### Listing 91 — `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12Common.h` (NEW)
```cpp
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <dxgi1_6.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "Core/Log.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace noc::dx12
{
	using Microsoft::WRL::ComPtr;

	inline void SafeCloseHandle(HANDLE& h)
	{
		if (h)
		{
			CloseHandle(h);
			h = nullptr;
		}
	}

	inline bool HrOk(HRESULT hr, const char* what)
	{
		if (SUCCEEDED(hr))
			return true;

		NOC_LOG_ERROR("Render", "%s failed (hr=0x%08X)", what, (unsigned)hr);
		return false;
	}

	inline D3D12_RESOURCE_BARRIER TransitionBarrier(
		ID3D12Resource* res,
		D3D12_RESOURCE_STATES before,
		D3D12_RESOURCE_STATES after)
	{
		D3D12_RESOURCE_BARRIER b{};
		b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		b.Transition.pResource = res;
		b.Transition.StateBefore = before;
		b.Transition.StateAfter = after;
		b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		return b;
	}

	inline D3D12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE type)
	{
		D3D12_HEAP_PROPERTIES p{};
		p.Type = type;
		p.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		p.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		p.CreationNodeMask = 1;
		p.VisibleNodeMask = 1;
		return p;
	}

	inline D3D12_RESOURCE_DESC BufferDesc(UINT64 bytes)
	{
		D3D12_RESOURCE_DESC d{};
		d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		d.Alignment = 0;
		d.Width = bytes;
		d.Height = 1;
		d.DepthOrArraySize = 1;
		d.MipLevels = 1;
		d.Format = DXGI_FORMAT_UNKNOWN;
		d.SampleDesc.Count = 1;
		d.SampleDesc.Quality = 0;
		d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		d.Flags = D3D12_RESOURCE_FLAG_NONE;
		return d;
	}

	// Common formats for our baseline.
	static constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static constexpr uint32_t kFrameCount = 2;
}
```

### Listing 92 — `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12Device.h` (NEW)
```cpp
#pragma once
#include "Dx12Common.h"

namespace noc
{
	class Dx12Device
	{
	public:
		bool Init(bool enableDebugLayer);

		ID3D12Device* Device() const { return device_.Get(); }
		ID3D12CommandQueue* Queue() const { return queue_.Get(); }
		IDXGIFactory6* Factory() const { return factory_.Get(); }

		void Shutdown();

	private:
		bool CreateFactory_();
		bool PickAdapter_();
		bool CreateDevice_();
		bool CreateQueue_();

	private:
		bool debugLayer_ = false;

		dx12::ComPtr<IDXGIFactory6> factory_;
		dx12::ComPtr<IDXGIAdapter1> adapter_;
		dx12::ComPtr<ID3D12Device> device_;
		dx12::ComPtr<ID3D12CommandQueue> queue_;
	};
}
```

### Listing 93 — `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12Device.cpp` (NEW)
```cpp
#include "Dx12Device.h"

namespace noc
{
	bool Dx12Device::Init(bool enableDebugLayer)
	{
		debugLayer_ = enableDebugLayer;

		if (!CreateFactory_()) return false;
		if (!PickAdapter_()) return false;
		if (!CreateDevice_()) return false;
		if (!CreateQueue_()) return false;

		return true;
	}

	void Dx12Device::Shutdown()
	{
		queue_.Reset();
		device_.Reset();
		adapter_.Reset();
		factory_.Reset();
	}

	bool Dx12Device::CreateFactory_()
	{
		UINT flags = 0;

		if (debugLayer_)
		{
			dx12::ComPtr<ID3D12Debug> dbg;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg))))
			{
				dbg->EnableDebugLayer();
				flags |= DXGI_CREATE_FACTORY_DEBUG;
				NOC_LOG_INFO("Render", "D3D12 debug layer enabled");
			}
			else
			{
				NOC_LOG_WARN("Render", "Failed to enable D3D12 debug layer");
			}
		}

		return dx12::HrOk(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory_)), "CreateDXGIFactory2");
	}

	bool Dx12Device::PickAdapter_()
	{
		for (UINT i = 0; ; ++i)
		{
			dx12::ComPtr<IDXGIAdapter1> a;
			if (factory_->EnumAdapters1(i, &a) == DXGI_ERROR_NOT_FOUND)
				break;

			DXGI_ADAPTER_DESC1 desc{};
			a->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				continue;

			if (SUCCEEDED(D3D12CreateDevice(a.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
			{
				adapter_ = a;
				NOC_LOG_INFO("Render", "DX12 adapter selected");
				return true;
			}
		}

		NOC_LOG_ERROR("Render", "No suitable hardware adapter found");
		return false;
	}

	bool Dx12Device::CreateDevice_()
	{
		return dx12::HrOk(D3D12CreateDevice(adapter_.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)), "D3D12CreateDevice");
	}

	bool Dx12Device::CreateQueue_()
	{
		D3D12_COMMAND_QUEUE_DESC q{};
		q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		q.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		q.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		q.NodeMask = 0;

		return dx12::HrOk(device_->CreateCommandQueue(&q, IID_PPV_ARGS(&queue_)), "CreateCommandQueue");
	}
}
```

### Listing 94 — `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12FrameSync.h` (NEW)
```cpp
#pragma once
#include "Dx12Common.h"

namespace noc
{
	class Dx12FrameSync
	{
	public:
		bool Init(ID3D12Device* device);
		void Shutdown();

		// Wait for all queued GPU work.
		void WaitForGpu(ID3D12CommandQueue* queue, uint32_t frameIndex);

		// Called after Present; signals fence for the submitted work and waits for next back buffer slot if needed.
		void MoveToNextFrame(ID3D12CommandQueue* queue, IDXGISwapChain3* swapChain, uint32_t& inOutFrameIndex);

	private:
		dx12::ComPtr<ID3D12Fence> fence_;
		uint64_t fenceValues_[dx12::kFrameCount]{};
		HANDLE fenceEvent_ = nullptr;
	};
}
```

### Listing 95 — `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12FrameSync.cpp` (NEW)
```cpp
#include "Dx12FrameSync.h"

namespace noc
{
	bool Dx12FrameSync::Init(ID3D12Device* device)
	{
		if (!dx12::HrOk(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "CreateFence"))
			return false;

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			fenceValues_[i] = 0;

		fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!fenceEvent_)
		{
			NOC_LOG_ERROR("Render", "CreateEvent failed (err=%lu)", GetLastError());
			return false;
		}

		// Match Phase 8 behavior: bump current slot at first use.
		// (We’ll do it in MoveToNextFrame logic by ensuring values are incremented.)
		return true;
	}

	void Dx12FrameSync::Shutdown()
	{
		dx12::SafeCloseHandle(fenceEvent_);
		fence_.Reset();
	}

	void Dx12FrameSync::WaitForGpu(ID3D12CommandQueue* queue, uint32_t frameIndex)
	{
		const uint64_t v = fenceValues_[frameIndex] + 1;
		fenceValues_[frameIndex] = v;

		queue->Signal(fence_.Get(), v);
		fence_->SetEventOnCompletion(v, fenceEvent_);
		WaitForSingleObjectEx(fenceEvent_, INFINITE, FALSE);
	}

	void Dx12FrameSync::MoveToNextFrame(ID3D12CommandQueue* queue, IDXGISwapChain3* swapChain, uint32_t& inOutFrameIndex)
	{
		const uint32_t submittedIndex = inOutFrameIndex;

		// Signal fence for the work we just submitted on this slot.
		const uint64_t currentFenceValue = fenceValues_[submittedIndex] + 1;
		fenceValues_[submittedIndex] = currentFenceValue;

		HRESULT hr = queue->Signal(fence_.Get(), currentFenceValue);
		if (FAILED(hr))
		{
			NOC_LOG_ERROR("Render", "MoveToNextFrame: Signal failed (hr=0x%08X)", (unsigned)hr);
			return;
		}

		// Advance swapchain index.
		inOutFrameIndex = swapChain->GetCurrentBackBufferIndex();

		// Wait if the next frame slot isn't ready.
		if (fence_->GetCompletedValue() < fenceValues_[inOutFrameIndex])
		{
			fence_->SetEventOnCompletion(fenceValues_[inOutFrameIndex], fenceEvent_);
			WaitForSingleObjectEx(fenceEvent_, INFINITE, FALSE);
		}
	}
}
```

### Listing 96 — `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12SwapChain.h` (NEW)
```cpp
#pragma once
#include "Dx12Common.h"

namespace noc
{
	class Dx12SwapChain
	{
	public:
		bool Init(
			IDXGIFactory6* factory,
			ID3D12CommandQueue* queue,
			void* hwnd,
			uint32_t clientWidth,
			uint32_t clientHeight);

		bool CreateRtvHeapAndViews(ID3D12Device* device);

		void Shutdown();

		uint32_t FrameIndex() const { return frameIndex_; }
		void SetFrameIndex(uint32_t i) { frameIndex_ = i; }

		IDXGISwapChain3* SwapChain() const { return swapChain_.Get(); }
		ID3D12Resource* BackBuffer(uint32_t i) const { return backBuffers_[i].Get(); }

		D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv() const;

		D3D12_RESOURCE_STATES CurrentBackBufferState() const { return bbState_[frameIndex_]; }
		void TransitionCurrent(ID3D12GraphicsCommandList* cmd, D3D12_RESOURCE_STATES to);

		uint32_t Width() const { return w_; }
		uint32_t Height() const { return h_; }

	private:
		dx12::ComPtr<IDXGISwapChain3> swapChain_;

		dx12::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
		UINT rtvDescriptorSize_ = 0;

		dx12::ComPtr<ID3D12Resource> backBuffers_[dx12::kFrameCount];
		D3D12_RESOURCE_STATES bbState_[dx12::kFrameCount]{
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_PRESENT
		};

		uint32_t w_ = 0;
		uint32_t h_ = 0;
		uint32_t frameIndex_ = 0;
	};
}
```

### Listing 97 — `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12SwapChain.cpp` (NEW)
```cpp
#include "Dx12SwapChain.h"

namespace noc
{
	bool Dx12SwapChain::Init(
		IDXGIFactory6* factory,
		ID3D12CommandQueue* queue,
		void* hwnd,
		uint32_t clientWidth,
		uint32_t clientHeight)
	{
		if (!factory || !queue || !hwnd || clientWidth == 0 || clientHeight == 0)
			return false;

		w_ = clientWidth;
		h_ = clientHeight;

		DXGI_SWAP_CHAIN_DESC1 sd{};
		sd.Width = clientWidth;
		sd.Height = clientHeight;
		sd.Format = dx12::kBackBufferFormat;
		sd.Stereo = FALSE;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = dx12::kFrameCount;
		sd.Scaling = DXGI_SCALING_STRETCH;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		sd.Flags = 0;

		dx12::ComPtr<IDXGISwapChain1> sc1;
		if (!dx12::HrOk(factory->CreateSwapChainForHwnd(queue, (HWND)hwnd, &sd, nullptr, nullptr, &sc1), "CreateSwapChainForHwnd"))
			return false;

		if (!dx12::HrOk(sc1.As(&swapChain_), "As IDXGISwapChain3"))
			return false;

		frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
		return true;
	}

	bool Dx12SwapChain::CreateRtvHeapAndViews(ID3D12Device* device)
	{
		D3D12_DESCRIPTOR_HEAP_DESC hd{};
		hd.NumDescriptors = dx12::kFrameCount;
		hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hd.NodeMask = 0;

		if (!dx12::HrOk(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvHeap_)), "CreateDescriptorHeap(RTV)"))
			return false;

		rtvDescriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_CPU_DESCRIPTOR_HANDLE base = rtvHeap_->GetCPUDescriptorHandleForHeapStart();

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (!dx12::HrOk(swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i])), "SwapChain.GetBuffer"))
				return false;

			D3D12_CPU_DESCRIPTOR_HANDLE h = base;
			h.ptr += (SIZE_T)i * (SIZE_T)rtvDescriptorSize_;
			device->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, h);

			bbState_[i] = D3D12_RESOURCE_STATE_PRESENT;
		}

		return true;
	}

	void Dx12SwapChain::Shutdown()
	{
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			backBuffers_[i].Reset();

		rtvHeap_.Reset();
		swapChain_.Reset();

		rtvDescriptorSize_ = 0;
		w_ = h_ = 0;
		frameIndex_ = 0;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Dx12SwapChain::CurrentRtv() const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE h = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
		h.ptr += (SIZE_T)frameIndex_ * (SIZE_T)rtvDescriptorSize_;
		return h;
	}

	void Dx12SwapChain::TransitionCurrent(ID3D12GraphicsCommandList* cmd, D3D12_RESOURCE_STATES to)
	{
		const uint32_t i = frameIndex_;
		if (bbState_[i] == to)
			return;

		auto b = dx12::TransitionBarrier(backBuffers_[i].Get(), bbState_[i], to);
		cmd->ResourceBarrier(1, &b);
		bbState_[i] = to;
	}
}
```

### Listing 98 — `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/ShaderCompiler.h` (NEW)
```cpp
#pragma once
#include "Dx12Common.h"

namespace noc
{
	class ShaderCompiler
	{
	public:
		// Compile from in-memory string.
		static bool CompileFromString(
			const char* source,
			const char* entry,
			const char* target,
			dx12::ComPtr<ID3DBlob>& outBytecode);

		// Compile from file (recommended once we introduce shader assets properly).
		static bool CompileFromFile(
			const wchar_t* filepath,
			const char* entry,
			const char* target,
			dx12::ComPtr<ID3DBlob>& outBytecode);
	};
}
```

### Listing 99 — `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/ShaderCompiler.cpp` (NEW)
```cpp
#include "ShaderCompiler.h"
#include <cstring>

namespace noc
{
	static UINT ShaderFlags_()
	{
		UINT flags = 0;
#if defined(_DEBUG)
		flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
		return flags;
	}

	bool ShaderCompiler::CompileFromString(
		const char* source,
		const char* entry,
		const char* target,
		dx12::ComPtr<ID3DBlob>& outBytecode)
	{
		if (!source || !entry || !target)
			return false;

		dx12::ComPtr<ID3DBlob> errors;
		HRESULT hr = D3DCompile(
			source, std::strlen(source),
			nullptr,
			nullptr,
			nullptr,
			entry, target,
			ShaderFlags_(), 0,
			&outBytecode,
			&errors);

		if (FAILED(hr))
		{
			if (errors)
				NOC_LOG_ERROR("Render", "Shader compile failed (%s/%s): %s", entry, target, (const char*)errors->GetBufferPointer());
			else
				NOC_LOG_ERROR("Render", "Shader compile failed (%s/%s), hr=0x%08X", entry, target, (unsigned)hr);
			return false;
		}

		return true;
	}

	bool ShaderCompiler::CompileFromFile(
		const wchar_t* filepath,
		const char* entry,
		const char* target,
		dx12::ComPtr<ID3DBlob>& outBytecode)
	{
		if (!filepath || !entry || !target)
			return false;

		dx12::ComPtr<ID3DBlob> errors;
		HRESULT hr = D3DCompileFromFile(
			filepath,
			nullptr,
			nullptr,
			entry,
			target,
			ShaderFlags_(), 0,
			&outBytecode,
			&errors);

		if (FAILED(hr))
		{
			if (errors)
				NOC_LOG_ERROR("Render", "Shader compile failed (%s/%s): %s", entry, target, (const char*)errors->GetBufferPointer());
			else
				NOC_LOG_ERROR("Render", "Shader compile failed (%s/%s), hr=0x%08X", entry, target, (unsigned)hr);
			return false;
		}

		return true;
	}
}
```

### Listing 100 — `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/TrianglePass.h` (NEW)
```cpp
#pragma once
#include "Dx12Common.h"

namespace noc
{
	class TrianglePass
	{
	public:
		bool Init(ID3D12Device* device, uint32_t viewportW, uint32_t viewportH);
		void Shutdown();

		void OnResize(uint32_t viewportW, uint32_t viewportH);

		void Record(
			ID3D12GraphicsCommandList* cmd,
			D3D12_CPU_DESCRIPTOR_HANDLE rtv);

	private:
		struct Vertex
		{
			float px, py, pz;
			float r, g, b, a;
		};

		bool CreateRootSig_(ID3D12Device* device);
		bool CreatePso_(ID3D12Device* device);
		bool CreateVB_(ID3D12Device* device);

	private:
		dx12::ComPtr<ID3D12RootSignature> rootSig_;
		dx12::ComPtr<ID3D12PipelineState> pso_;
		dx12::ComPtr<ID3D12Resource> vbUpload_;
		D3D12_VERTEX_BUFFER_VIEW vbView_{};

		D3D12_VIEWPORT viewport_{};
		D3D12_RECT scissor_{};
	};
}
```

### Listing 101 — `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/TrianglePass.cpp` (NEW)
```cpp
#include "TrianglePass.h"
#include "ShaderCompiler.h"
#include <cstring>

namespace noc
{
	bool TrianglePass::Init(ID3D12Device* device, uint32_t viewportW, uint32_t viewportH)
	{
		if (!device || viewportW == 0 || viewportH == 0)
			return false;

		OnResize(viewportW, viewportH);

		if (!CreateRootSig_(device)) return false;
		if (!CreatePso_(device)) return false;
		if (!CreateVB_(device)) return false;

		NOC_LOG_INFO("Render", "TrianglePass initialized");
		return true;
	}

	void TrianglePass::Shutdown()
	{
		vbUpload_.Reset();
		pso_.Reset();
		rootSig_.Reset();
	}

	void TrianglePass::OnResize(uint32_t viewportW, uint32_t viewportH)
	{
		viewport_.TopLeftX = 0.0f;
		viewport_.TopLeftY = 0.0f;
		viewport_.Width = (float)viewportW;
		viewport_.Height = (float)viewportH;
		viewport_.MinDepth = 0.0f;
		viewport_.MaxDepth = 1.0f;

		scissor_.left = 0;
		scissor_.top = 0;
		scissor_.right = (LONG)viewportW;
		scissor_.bottom = (LONG)viewportH;
	}

	bool TrianglePass::CreateRootSig_(ID3D12Device* device)
	{
		// Empty root signature (no descriptors) but allow IA.
		D3D12_ROOT_SIGNATURE_DESC rs{};
		rs.NumParameters = 0;
		rs.pParameters = nullptr;
		rs.NumStaticSamplers = 0;
		rs.pStaticSamplers = nullptr;
		rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		dx12::ComPtr<ID3DBlob> blob;
		dx12::ComPtr<ID3DBlob> errors;
		HRESULT hr = D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors);
		if (FAILED(hr))
		{
			if (errors)
				NOC_LOG_ERROR("Render", "RootSignature serialize failed: %s", (const char*)errors->GetBufferPointer());
			else
				NOC_LOG_ERROR("Render", "RootSignature serialize failed hr=0x%08X", (unsigned)hr);
			return false;
		}

		return dx12::HrOk(device->CreateRootSignature(
			0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig_)),
			"CreateRootSignature");
	}

	bool TrianglePass::CreatePso_(ID3D12Device* device)
	{
		static const char* kHlsl = R"(
struct VSInput { float3 pos : POSITION; float4 color : COLOR; };
struct PSInput { float4 pos : SV_POSITION; float4 color : COLOR; };

PSInput VSMain(VSInput v)
{
	PSInput o;
	o.pos = float4(v.pos, 1.0);
	o.color = v.color;
	return o;
}

float4 PSMain(PSInput i) : SV_Target
{
	return i.color;
}
)";

		dx12::ComPtr<ID3DBlob> vs, ps;
		if (!ShaderCompiler::CompileFromString(kHlsl, "VSMain", "vs_5_0", vs)) return false;
		if (!ShaderCompiler::CompileFromString(kHlsl, "PSMain", "ps_5_0", ps)) return false;

		D3D12_INPUT_ELEMENT_DESC input[2]{};
		input[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		input[1] = { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

		// Build minimal PSO without relying on d3dx12 helpers.
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = rootSig_.Get();
		pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		pso.InputLayout = { input, 2 };
		pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		// Rasterizer default-ish
		D3D12_RASTERIZER_DESC rast{};
		rast.FillMode = D3D12_FILL_MODE_SOLID;
		rast.CullMode = D3D12_CULL_MODE_BACK;
		rast.FrontCounterClockwise = FALSE;
		rast.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rast.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rast.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rast.DepthClipEnable = TRUE;
		rast.MultisampleEnable = FALSE;
		rast.AntialiasedLineEnable = FALSE;
		rast.ForcedSampleCount = 0;
		rast.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		pso.RasterizerState = rast;

		// Blend default-ish
		D3D12_BLEND_DESC blend{};
		blend.AlphaToCoverageEnable = FALSE;
		blend.IndependentBlendEnable = FALSE;
		for (int i = 0; i < 8; ++i)
		{
			auto& rt = blend.RenderTarget[i];
			rt.BlendEnable = FALSE;
			rt.LogicOpEnable = FALSE;
			rt.SrcBlend = D3D12_BLEND_ONE;
			rt.DestBlend = D3D12_BLEND_ZERO;
			rt.BlendOp = D3D12_BLEND_OP_ADD;
			rt.SrcBlendAlpha = D3D12_BLEND_ONE;
			rt.DestBlendAlpha = D3D12_BLEND_ZERO;
			rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
			rt.LogicOp = D3D12_LOGIC_OP_NOOP;
			rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		}
		pso.BlendState = blend;

		// No depth/stencil for Phase 9.
		D3D12_DEPTH_STENCIL_DESC ds{};
		ds.DepthEnable = FALSE;
		ds.StencilEnable = FALSE;
		pso.DepthStencilState = ds;

		pso.SampleMask = UINT_MAX;
		pso.NumRenderTargets = 1;
		pso.RTVFormats[0] = dx12::kBackBufferFormat;
		pso.SampleDesc.Count = 1;

		return dx12::HrOk(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pso_)),
			"CreateGraphicsPipelineState");
	}

	bool TrianglePass::CreateVB_(ID3D12Device* device)
	{
		const Vertex verts[3] = {
			{  0.0f,  0.5f, 0.0f,  1.f, 0.f, 0.f, 1.f },
			{  0.5f, -0.5f, 0.0f,  0.f, 1.f, 0.f, 1.f },
			{ -0.5f, -0.5f, 0.0f,  0.f, 0.f, 1.f, 1.f },
		};

		const UINT vbBytes = (UINT)sizeof(verts);

		auto heap = dx12::HeapProps(D3D12_HEAP_TYPE_UPLOAD);
		auto desc = dx12::BufferDesc(vbBytes);

		if (!dx12::HrOk(device->CreateCommittedResource(
			&heap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&vbUpload_)),
			"CreateCommittedResource(VB upload)"))
		{
			return false;
		}

		void* mapped = nullptr;
		D3D12_RANGE range{ 0,0 };
		if (!dx12::HrOk(vbUpload_->Map(0, &range, &mapped), "VB.Map"))
			return false;

		std::memcpy(mapped, verts, vbBytes);
		vbUpload_->Unmap(0, nullptr);

		vbView_.BufferLocation = vbUpload_->GetGPUVirtualAddress();
		vbView_.SizeInBytes = vbBytes;
		vbView_.StrideInBytes = sizeof(Vertex);

		return true;
	}

	void TrianglePass::Record(ID3D12GraphicsCommandList* cmd, D3D12_CPU_DESCRIPTOR_HANDLE rtv)
	{
		// Viewport/scissor
		cmd->RSSetViewports(1, &viewport_);
		cmd->RSSetScissorRects(1, &scissor_);

		// Bind RTV + clear
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		const float clear[4] = { 0.02f, 0.02f, 0.05f, 1.0f };
		cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);

		// Pipeline
		cmd->SetGraphicsRootSignature(rootSig_.Get());
		cmd->SetPipelineState(pso_.Get());

		// IA
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 1, &vbView_);

		// Draw
		cmd->DrawInstanced(3, 1, 0, 0);
	}
}
```

### Listing 102 — `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12Renderer.h` (NEW)
```cpp
#pragma once
#include "Dx12Common.h"

#include "Dx12Device.h"
#include "Dx12SwapChain.h"
#include "Dx12FrameSync.h"
#include "TrianglePass.h"

namespace noc
{
	class Dx12Renderer
	{
	public:
		bool Init(bool enableDebugLayer);
		void Shutdown();

		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		void BeginFrame();
		void EndFramePresent();

	private:
		void LogDeviceRemoved_(const char* where);

	private:
		bool inited_ = false;
		bool attached_ = false;

		Dx12Device device_;
		Dx12SwapChain swap_;
		Dx12FrameSync sync_;

		dx12::ComPtr<ID3D12CommandAllocator> cmdAlloc_[dx12::kFrameCount];
		dx12::ComPtr<ID3D12GraphicsCommandList> cmdList_;

		uint32_t frameIndex_ = 0;
		bool frameOpen_ = false;

		TrianglePass triangle_;
	};
}
```

### Listing 103 — `Phase 9 — Rendering Engine Foundation.md` — Implementations (Clean Phase 9) > `Engine/Render/DX12/Dx12Renderer.cpp` (NEW)
```cpp
#include "Dx12Renderer.h"

namespace noc
{
	bool Dx12Renderer::Init(bool enableDebugLayer)
	{
		if (inited_)
			return true;

		if (!device_.Init(enableDebugLayer))
			return false;

		if (!sync_.Init(device_.Device()))
			return false;

		inited_ = true;
		NOC_LOG_INFO("Render", "Dx12Renderer initialized (awaiting AttachToWindow)");
		return true;
	}

	void Dx12Renderer::Shutdown()
	{
		if (!inited_)
			return;

		// Ensure GPU is idle before releasing.
		if (attached_)
			sync_.WaitForGpu(device_.Queue());

		triangle_.Shutdown();

		cmdList_.Reset();
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			cmdAlloc_[i].Reset();

		swap_.Shutdown();
		sync_.Shutdown();
		device_.Shutdown();

		inited_ = false;
		attached_ = false;

		NOC_LOG_INFO("Render", "Dx12Renderer shutdown");
	}

	bool Dx12Renderer::AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!inited_)
			return false;

		if (!swap_.Init(device_.Factory(), device_.Queue(), nativeHwnd, clientWidth, clientHeight))
			return false;

		if (!swap_.CreateRtvHeapAndViews(device_.Device()))
			return false;

		frameIndex_ = swap_.FrameIndex();

		// Commands: allocators + one list.
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (!dx12::HrOk(device_.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc_[i])),
				"CreateCommandAllocator"))
			{
				return false;
			}
		}

		if (!dx12::HrOk(device_.Device()->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			cmdAlloc_[frameIndex_].Get(),
			nullptr,
			IID_PPV_ARGS(&cmdList_)),
			"CreateCommandList"))
		{
			return false;
		}

		// Close it; BeginFrame will Reset it (Phase 8 pattern). 
		if (!dx12::HrOk(cmdList_->Close(), "cmdList->Close (initial)"))
			return false;

		// Phase 9: triangle pass.
		if (!triangle_.Init(device_.Device(), swap_.Width(), swap_.Height()))
			return false;

		attached_ = true;
		NOC_LOG_INFO("Render", "DX12 ready (%ux%u, buffers=%u)", clientWidth, clientHeight, dx12::kFrameCount);
		return true;
	}

	void Dx12Renderer::LogDeviceRemoved_(const char* where)
	{
		if (!device_.Device())
			return;

		HRESULT dr = device_.Device()->GetDeviceRemovedReason();
		if (dr != S_OK)
			NOC_LOG_ERROR("Render", "Device removed at %s (hr=0x%08X)", where, (unsigned)dr);
	}

	void Dx12Renderer::BeginFrame()
	{
		if (!attached_ || !device_.Device())
			return;

		if (device_.Device()->GetDeviceRemovedReason() != S_OK)
			return;

		if (frameOpen_)
		{
			NOC_LOG_ERROR("Render", "BeginFrame called while frame is already open");
			return;
		}
		frameOpen_ = true;

		// Reset allocator for current frame.
		if (FAILED(cmdAlloc_[frameIndex_]->Reset()))
		{
			NOC_LOG_ERROR("Render", "cmdAlloc->Reset failed");
			LogDeviceRemoved_("cmdAlloc->Reset");
			frameOpen_ = false;
			return;
		}

		if (FAILED(cmdList_->Reset(cmdAlloc_[frameIndex_].Get(), nullptr)))
		{
			NOC_LOG_ERROR("Render", "cmdList->Reset failed");
			LogDeviceRemoved_("cmdList->Reset");
			frameOpen_ = false;
			return;
		}
	}

	void Dx12Renderer::EndFramePresent()
	{
		if (!attached_ || !device_.Device())
			return;

		if (device_.Device()->GetDeviceRemovedReason() != S_OK)
			return;

		if (!frameOpen_)
		{
			NOC_LOG_ERROR("Render", "EndFramePresent called with no open frame");
			return;
		}

		struct Guard { bool& b; ~Guard() { b = false; } } guard{ frameOpen_ };

		// Back buffer to RT.
		swap_.SetFrameIndex(frameIndex_);
		swap_.TransitionCurrent(cmdList_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);

		// Record triangle pass (clear + draw).
		triangle_.Record(cmdList_.Get(), swap_.CurrentRtv());

		// RT back to present.
		swap_.TransitionCurrent(cmdList_.Get(), D3D12_RESOURCE_STATE_PRESENT);

		if (FAILED(cmdList_->Close()))
		{
			NOC_LOG_ERROR("Render", "cmdList->Close failed");
			LogDeviceRemoved_("cmdList->Close");
			return;
		}

		ID3D12CommandList* lists[] = { cmdList_.Get() };
		device_.Queue()->ExecuteCommandLists(1, lists);

		HRESULT hr = swap_.SwapChain()->Present(1, 0);
		if (FAILED(hr))
		{
			NOC_LOG_ERROR("Render", "Present failed (hr=0x%08X)", (unsigned)hr);
			LogDeviceRemoved_("Present");
			return;
		}

		// Advance frame index + sync (keeps Phase 8 model intact). 
		sync_.MoveToNextFrame(device_.Queue(), swap_.SwapChain(), frameIndex_);
	}
}
```

### Listing 104 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/RenderSystem.h` (MODIFIED)
```cpp
#pragma once
#include <cstdint>

namespace noc
{
	class ResourceManager;

	class RenderSystem
	{
	public:
		RenderSystem() = default;

		bool Init(bool enableDebugLayer);
		void Shutdown();

		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		// Phase 9.5: allow renderer to consume engine assets via ResourceManager.
		// RenderSystem remains thin; this is just a pointer handoff.
		void SetResourceManager(ResourceManager* rm);

		void BeginFrame();
		void EndFramePresent();

	private:
		struct Impl;
		Impl* impl_ = nullptr;
	};
}
```

### Listing 105 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/RenderSystem.cpp` (MODIFIED)
```cpp
#include "RenderSystem.h"

#include "Core/Log.h"
#include "Resources/ResourceManager.h"
#include "Render/DX12/Dx12Renderer.h"

namespace noc
{
	struct RenderSystem::Impl
	{
		Dx12Renderer renderer;
		ResourceManager* rm = nullptr;
	};

	bool RenderSystem::Init(bool enableDebugLayer)
	{
		if (impl_)
			return true;

		impl_ = new Impl();
		if (!impl_->renderer.Init(enableDebugLayer))
		{
			NOC_LOG_ERROR("Render", "Dx12Renderer::Init failed");
			delete impl_;
			impl_ = nullptr;
			return false;
		}

		NOC_LOG_INFO("Render", "RenderSystem initialized");
		return true;
	}

	void RenderSystem::Shutdown()
	{
		if (!impl_)
			return;

		impl_->renderer.Shutdown();

		delete impl_;
		impl_ = nullptr;

		NOC_LOG_INFO("Render", "RenderSystem shutdown");
	}

	bool RenderSystem::AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!impl_)
			return false;

		if (!impl_->renderer.AttachToWindow(nativeHwnd, clientWidth, clientHeight))
			return false;

		// If RM was already provided, forward it now that renderer is attached.
		impl_->renderer.SetResourceManager(impl_->rm);
		return true;
	}

	void RenderSystem::SetResourceManager(ResourceManager* rm)
	{
		if (!impl_)
			return;

		impl_->rm = rm;
		impl_->renderer.SetResourceManager(rm);
	}

	void RenderSystem::BeginFrame()
	{
		if (!impl_)
			return;

		impl_->renderer.BeginFrame();
	}

	void RenderSystem::EndFramePresent()
	{
		if (!impl_)
			return;

		impl_->renderer.EndFramePresent();
	}
}
```

### Listing 106 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12FrameSync.h` (MODIFIED)
```cpp
#pragma once
#include "Dx12Common.h"

namespace noc
{
	class Dx12FrameSync
	{
	public:
		bool Init(ID3D12Device* device);
		void Shutdown();

		// Wait for the GPU to finish all work up to the last signaled fence.
		void WaitForGpu(ID3D12CommandQueue* queue);

		// Called at end of frame: signal fence for current frame, advance swapchain index, wait if needed.
		void MoveToNextFrame(ID3D12CommandQueue* queue, IDXGISwapChain3* swapChain, uint32_t& inOutFrameIndex);

		// Phase 9.5: accessors for deferred release + upload tracking.
		uint64_t FenceValueForFrame(uint32_t frameIndex) const { return fenceValues_[frameIndex]; }
		uint64_t CompletedValue() const { return fence_ ? fence_->GetCompletedValue() : 0; }

	private:
		void WaitForFenceValue_(uint64_t v);

	private:
		dx12::ComPtr<ID3D12Fence> fence_;
		HANDLE fenceEvent_ = nullptr;

		uint64_t fenceValues_[dx12::kFrameCount]{};
	};
}
```

### Listing 107 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12FrameSync.cpp` (MODIFIED)
```cpp
#include "Dx12FrameSync.h"

namespace noc
{
	bool Dx12FrameSync::Init(ID3D12Device* device)
	{
		if (!device)
			return false;

		if (!dx12::HrOk(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "CreateFence"))
			return false;

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			fenceValues_[i] = 0;

		fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!fenceEvent_)
		{
			NOC_LOG_ERROR("Render", "CreateEvent failed (err=%lu)", GetLastError());
			return false;
		}

		// Match Phase 8/9 behavior: bump once so first MoveToNextFrame is sane.
		fenceValues_[0] = 1;
		fenceValues_[1] = 1;
		return true;
	}

	void Dx12FrameSync::Shutdown()
	{
		dx12::SafeCloseHandle(fenceEvent_);
		fence_.Reset();
	}

	void Dx12FrameSync::WaitForFenceValue_(uint64_t v)
	{
		if (!fence_ || !fenceEvent_)
			return;

		if (fence_->GetCompletedValue() >= v)
			return;

		fence_->SetEventOnCompletion(v, fenceEvent_);
		WaitForSingleObjectEx(fenceEvent_, INFINITE, FALSE);
	}

	void Dx12FrameSync::WaitForGpu(ID3D12CommandQueue* queue)
	{
		if (!queue || !fence_)
			return;

		// Use frame 0 fence slot as a “flush” lane.
		const uint64_t v = fenceValues_[0];
		queue->Signal(fence_.Get(), v);
		WaitForFenceValue_(v);
		fenceValues_[0] = v + 1;
	}

	void Dx12FrameSync::MoveToNextFrame(ID3D12CommandQueue* queue, IDXGISwapChain3* swapChain, uint32_t& inOutFrameIndex)
	{
		if (!queue || !swapChain || !fence_)
			return;

		const uint32_t frameIndex = inOutFrameIndex;

		// Signal for the frame we just submitted.
		const uint64_t currentFenceValue = fenceValues_[frameIndex];
		HRESULT hr = queue->Signal(fence_.Get(), currentFenceValue);
		if (FAILED(hr))
		{
			NOC_LOG_ERROR("Render", "MoveToNextFrame: Signal failed (hr=0x%08X)", (unsigned)hr);
			return;
		}

		// Update to the next back buffer.
		inOutFrameIndex = swapChain->GetCurrentBackBufferIndex();

		// Wait until that back buffer is ready (avoid allocator reuse hazards).
		const uint64_t nextFenceValue = fenceValues_[inOutFrameIndex];
		WaitForFenceValue_(nextFenceValue);

		// Set fence value for next time we signal on this frame.
		fenceValues_[inOutFrameIndex] = currentFenceValue + 1;
	}
}
```

### Listing 108 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12DescriptorAllocator.h` (NEW)
```cpp
#pragma once
#include "Dx12Common.h"
#include <cstdint>

namespace noc
{
	struct Dx12DescriptorHandle
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
		D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
		uint32_t index = 0;
		bool shaderVisible = false;
	};

	class Dx12DescriptorAllocator
	{
	public:
		bool Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool shaderVisible);
		void Shutdown();

		Dx12DescriptorHandle Allocate(); // linear for Phase 9.5 (stable, no frees)
		ID3D12DescriptorHeap* Heap() const { return heap_.Get(); }
		uint32_t DescriptorSize() const { return descriptorSize_; }
		bool ShaderVisible() const { return shaderVisible_; }

	private:
		dx12::ComPtr<ID3D12DescriptorHeap> heap_;
		D3D12_DESCRIPTOR_HEAP_TYPE type_ = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		uint32_t descriptorSize_ = 0;
		uint32_t capacity_ = 0;
		uint32_t cursor_ = 0;
		bool shaderVisible_ = false;
	};
}
```

### Listing 109 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12DescriptorAllocator.cpp` (NEW)
```cpp
#include "Dx12DescriptorAllocator.h"

namespace noc
{
	bool Dx12DescriptorAllocator::Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool shaderVisible)
	{
		if (!device || capacity == 0)
			return false;

		type_ = type;
		capacity_ = capacity;
		cursor_ = 0;
		shaderVisible_ = shaderVisible;

		D3D12_DESCRIPTOR_HEAP_DESC hd{};
		hd.Type = type;
		hd.NumDescriptors = capacity;
		hd.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		if (!dx12::HrOk(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap_)), "CreateDescriptorHeap"))
			return false;

		descriptorSize_ = device->GetDescriptorHandleIncrementSize(type);
		return true;
	}

	void Dx12DescriptorAllocator::Shutdown()
	{
		heap_.Reset();
		capacity_ = 0;
		cursor_ = 0;
		descriptorSize_ = 0;
		shaderVisible_ = false;
	}

	Dx12DescriptorHandle Dx12DescriptorAllocator::Allocate()
	{
		Dx12DescriptorHandle h{};
		if (!heap_ || cursor_ >= capacity_)
		{
			NOC_LOG_ERROR("Render", "DescriptorAllocator out of space (type=%u cap=%u)", (unsigned)type_, capacity_);
			return h;
		}

		const uint32_t idx = cursor_++;
		D3D12_CPU_DESCRIPTOR_HANDLE cpu = heap_->GetCPUDescriptorHandleForHeapStart();
		cpu.ptr += (SIZE_T)idx * (SIZE_T)descriptorSize_;

		h.cpu = cpu;
		h.index = idx;
		h.shaderVisible = shaderVisible_;

		if (shaderVisible_)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE gpu = heap_->GetGPUDescriptorHandleForHeapStart();
			gpu.ptr += (UINT64)idx * (UINT64)descriptorSize_;
			h.gpu = gpu;
		}

		return h;
	}
}
```

### Listing 110 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12DeferredReleaseQueue.h` (NEW)
```cpp
#pragma once
#include "Dx12Common.h"
#include <cstdint>
#include <vector>

namespace noc
{
	class Dx12DeferredReleaseQueue
	{
	public:
		void Enqueue(uint64_t fenceValue, dx12::ComPtr<IUnknown>&& obj);
		void Collect(uint64_t completedFenceValue);
		void Clear(); // drop everything immediately (only call after GPU idle)

	private:
		struct Item
		{
			uint64_t fenceValue = 0;
			dx12::ComPtr<IUnknown> obj;
		};

		std::vector<Item> items_;
	};
}
```

### Listing 111 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12DeferredReleaseQueue.cpp` (NEW)
```cpp
#include "Dx12DeferredReleaseQueue.h"

namespace noc
{
	void Dx12DeferredReleaseQueue::Enqueue(uint64_t fenceValue, dx12::ComPtr<IUnknown>&& obj)
	{
		if (!obj)
			return;

		Item it{};
		it.fenceValue = fenceValue;
		it.obj = std::move(obj);
		items_.push_back(std::move(it));
	}

	void Dx12DeferredReleaseQueue::Collect(uint64_t completedFenceValue)
	{
		// Compact in-place.
		size_t out = 0;
		for (size_t i = 0; i < items_.size(); ++i)
		{
			if (items_[i].fenceValue <= completedFenceValue)
			{
				// eligible: let ComPtr drop
				continue;
			}
			if (out != i)
				items_[out] = std::move(items_[i]);
			++out;
		}
		items_.resize(out);
	}

	void Dx12DeferredReleaseQueue::Clear()
	{
		items_.clear();
	}
}
```

### Listing 112 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/GpuBuffer.h` (NEW)
```cpp
#pragma once
#include "Dx12Common.h"
#include <cstdint>
#include <cstddef>

namespace noc
{
	class Dx12DeferredReleaseQueue;
	class Dx12FrameSync;

	class GpuBuffer
	{
	public:
		enum class Kind : uint8_t { Vertex, Index };

		bool CreateStatic(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmd,
			Dx12DeferredReleaseQueue& deferred,
			const Dx12FrameSync& sync,
			uint32_t frameIndex,
			Kind kind,
			const void* srcData,
			size_t numBytes,
			uint32_t strideBytes);

		void ShutdownNow();

		ID3D12Resource* Resource() const { return resource_.Get(); }
		size_t SizeBytes() const { return sizeBytes_; }

		D3D12_VERTEX_BUFFER_VIEW VertexView() const;
		D3D12_INDEX_BUFFER_VIEW IndexView(DXGI_FORMAT fmt) const;

	private:
		dx12::ComPtr<ID3D12Resource> resource_;
		size_t sizeBytes_ = 0;
		uint32_t strideBytes_ = 0;
		Kind kind_ = Kind::Vertex;
	};
}
```

### Listing 113 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/GpuBuffer.cpp` (NEW)
```cpp
#include "GpuBuffer.h"
#include "Dx12DeferredReleaseQueue.h"
#include "Dx12FrameSync.h"

namespace noc
{
	static D3D12_RESOURCE_DESC BufferDesc_(UINT64 bytes)
	{
		D3D12_RESOURCE_DESC d{};
		d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		d.Alignment = 0;
		d.Width = bytes;
		d.Height = 1;
		d.DepthOrArraySize = 1;
		d.MipLevels = 1;
		d.Format = DXGI_FORMAT_UNKNOWN;
		d.SampleDesc.Count = 1;
		d.SampleDesc.Quality = 0;
		d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		d.Flags = D3D12_RESOURCE_FLAG_NONE;
		return d;
	}

	bool GpuBuffer::CreateStatic(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12DeferredReleaseQueue& deferred,
		const Dx12FrameSync& sync,
		uint32_t frameIndex,
		Kind kind,
		const void* srcData,
		size_t numBytes,
		uint32_t strideBytes)
	{
		if (!device || !cmd || !srcData || numBytes == 0)
			return false;

		ShutdownNow();

		kind_ = kind;
		sizeBytes_ = numBytes;
		strideBytes_ = strideBytes;

		// Default heap (GPU-only)
		D3D12_HEAP_PROPERTIES hpDefault{};
		hpDefault.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC desc = BufferDesc_((UINT64)numBytes);

		if (!dx12::HrOk(device->CreateCommittedResource(
			&hpDefault,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&resource_)), "CreateCommittedResource(DefaultBuffer)"))
		{
			return false;
		}

		// Upload heap (CPU-visible staging)
		D3D12_HEAP_PROPERTIES hpUpload{};
		hpUpload.Type = D3D12_HEAP_TYPE_UPLOAD;

		dx12::ComPtr<ID3D12Resource> upload;
		if (!dx12::HrOk(device->CreateCommittedResource(
			&hpUpload,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&upload)), "CreateCommittedResource(UploadBuffer)"))
		{
			resource_.Reset();
			return false;
		}

		// Copy bytes into upload.
		void* mapped = nullptr;
		D3D12_RANGE r{ 0, 0 };
		if (!dx12::HrOk(upload->Map(0, &r, &mapped), "UploadBuffer.Map"))
		{
			resource_.Reset();
			upload.Reset();
			return false;
		}
		memcpy(mapped, srcData, numBytes);
		upload->Unmap(0, nullptr);

		// Record copy into default buffer.
		cmd->CopyBufferRegion(resource_.Get(), 0, upload.Get(), 0, (UINT64)numBytes);

		// Transition to final state for binding.
		D3D12_RESOURCE_STATES finalState =
			(kind == Kind::Vertex) ? D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
			                       : D3D12_RESOURCE_STATE_INDEX_BUFFER;

		D3D12_RESOURCE_BARRIER b = dx12::TransitionBarrier(resource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);
		cmd->ResourceBarrier(1, &b);

		// Keep upload alive until the fence value for this frame has completed.
		// We tag with the value that *will be signaled* for this frame index.
		{
			dx12::ComPtr<IUnknown> asUnknown;
			upload.As(&asUnknown);
			deferred.Enqueue(sync.FenceValueForFrame(frameIndex), std::move(asUnknown));
		}

		return true;
	}

	void GpuBuffer::ShutdownNow()
	{
		resource_.Reset();
		sizeBytes_ = 0;
		strideBytes_ = 0;
		kind_ = Kind::Vertex;
	}

	D3D12_VERTEX_BUFFER_VIEW GpuBuffer::VertexView() const
	{
		D3D12_VERTEX_BUFFER_VIEW v{};
		if (!resource_)
			return v;

		v.BufferLocation = resource_->GetGPUVirtualAddress();
		v.SizeInBytes = (UINT)sizeBytes_;
		v.StrideInBytes = strideBytes_;
		return v;
	}

	D3D12_INDEX_BUFFER_VIEW GpuBuffer::IndexView(DXGI_FORMAT fmt) const
	{
		D3D12_INDEX_BUFFER_VIEW v{};
		if (!resource_)
			return v;

		v.BufferLocation = resource_->GetGPUVirtualAddress();
		v.SizeInBytes = (UINT)sizeBytes_;
		v.Format = fmt;
		return v;
	}
}
```

### Listing 114 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/GpuRingConstantBuffer.h` (NEW)
```cpp
#pragma once
#include "Dx12Common.h"
#include <cstdint>
#include <cstddef>

namespace noc
{
	// Per-frame upload-heap constant buffer (one resource per frame).
	// Constants are allocated linearly; reset once per frame.
	class GpuRingConstantBuffer
	{
	public:
		bool Init(ID3D12Device* device, size_t bytesPerFrame);
		void Shutdown();

		void BeginFrame(uint32_t frameIndex);

		// Allocates aligned (256B) constant data in the *current* frame buffer.
		// Returns GPU virtual address and CPU pointer to write into.
		bool Allocate(size_t bytes, D3D12_GPU_VIRTUAL_ADDRESS& outGpu, void*& outCpu);

		ID3D12Resource* Resource(uint32_t frameIndex) const { return frames_[frameIndex].resource.Get(); }
		uint8_t* Mapped(uint32_t frameIndex) const { return frames_[frameIndex].mapped; }
		size_t CapacityBytes() const { return capacity_; }

	private:
		static size_t Align256_(size_t x) { return (x + 255u) & ~255u; }

		struct Frame
		{
			dx12::ComPtr<ID3D12Resource> resource;
			uint8_t* mapped = nullptr;
		};

		Frame frames_[dx12::kFrameCount]{};
		size_t capacity_ = 0;
		size_t cursor_ = 0;
		uint32_t curFrame_ = 0;
	};
}
```

### Listing 115 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/GpuRingConstantBuffer.cpp` (NEW)
```cpp
#include "GpuRingConstantBuffer.h"

namespace noc
{
	bool GpuRingConstantBuffer::Init(ID3D12Device* device, size_t bytesPerFrame)
	{
		if (!device || bytesPerFrame == 0)
			return false;

		capacity_ = Align256_(bytesPerFrame);

		D3D12_HEAP_PROPERTIES hp{};
		hp.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC d{};
		d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		d.Width = (UINT64)capacity_;
		d.Height = 1;
		d.DepthOrArraySize = 1;
		d.MipLevels = 1;
		d.SampleDesc.Count = 1;
		d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (!dx12::HrOk(device->CreateCommittedResource(
				&hp,
				D3D12_HEAP_FLAG_NONE,
				&d,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&frames_[i].resource)), "CreateCommittedResource(FrameCB)"))
			{
				return false;
			}

			void* mapped = nullptr;
			D3D12_RANGE r{ 0, 0 };
			if (!dx12::HrOk(frames_[i].resource->Map(0, &r, &mapped), "FrameCB.Map"))
				return false;

			frames_[i].mapped = (uint8_t*)mapped;
		}

		return true;
	}

	void GpuRingConstantBuffer::Shutdown()
	{
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (frames_[i].resource && frames_[i].mapped)
				frames_[i].resource->Unmap(0, nullptr);

			frames_[i].mapped = nullptr;
			frames_[i].resource.Reset();
		}
		capacity_ = 0;
		cursor_ = 0;
		curFrame_ = 0;
	}

	void GpuRingConstantBuffer::BeginFrame(uint32_t frameIndex)
	{
		curFrame_ = frameIndex;
		cursor_ = 0;
	}

	bool GpuRingConstantBuffer::Allocate(size_t bytes, D3D12_GPU_VIRTUAL_ADDRESS& outGpu, void*& outCpu)
	{
		const size_t aligned = Align256_(bytes);
		if (cursor_ + aligned > capacity_)
			return false;

		auto* res = frames_[curFrame_].resource.Get();
		if (!res || !frames_[curFrame_].mapped)
			return false;

		outGpu = res->GetGPUVirtualAddress() + (UINT64)cursor_;
		outCpu = frames_[curFrame_].mapped + cursor_;
		cursor_ += aligned;
		return true;
	}
}
```

### Listing 116 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12PsoCache.h` (NEW)
```cpp
#pragma once
#include "Dx12Common.h"
#include <cstdint>
#include <unordered_map>

namespace noc
{
	struct Dx12PsoKey
	{
		const void* vs = nullptr;
		const void* ps = nullptr;
		ID3D12RootSignature* rootSig = nullptr;
		DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		uint64_t inputLayoutHash = 0;

		bool operator==(const Dx12PsoKey& o) const
		{
			return vs == o.vs && ps == o.ps && rootSig == o.rootSig && rtvFormat == o.rtvFormat && inputLayoutHash == o.inputLayoutHash;
		}
	};

	struct Dx12PsoKeyHash
	{
		size_t operator()(const Dx12PsoKey& k) const noexcept
		{
			size_t h = 1469598103934665603ull;
			auto mix = [&](size_t v) { h ^= v; h *= 1099511628211ull; };
			mix((size_t)k.vs);
			mix((size_t)k.ps);
			mix((size_t)k.rootSig);
			mix((size_t)k.rtvFormat);
			mix((size_t)k.inputLayoutHash);
			return h;
		}
	};

	class Dx12PsoCache
	{
	public:
		ID3D12PipelineState* Find(const Dx12PsoKey& key) const;
		void Insert(const Dx12PsoKey& key, dx12::ComPtr<ID3D12PipelineState>&& pso);
		void Clear();

	private:
		std::unordered_map<Dx12PsoKey, dx12::ComPtr<ID3D12PipelineState>, Dx12PsoKeyHash> map_;
	};
}
```

### Listing 117 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12PsoCache.cpp` (NEW)
```cpp
#include "Dx12PsoCache.h"

namespace noc
{
	ID3D12PipelineState* Dx12PsoCache::Find(const Dx12PsoKey& key) const
	{
		auto it = map_.find(key);
		return (it == map_.end()) ? nullptr : it->second.Get();
	}

	void Dx12PsoCache::Insert(const Dx12PsoKey& key, dx12::ComPtr<ID3D12PipelineState>&& pso)
	{
		map_[key] = std::move(pso);
	}

	void Dx12PsoCache::Clear()
	{
		map_.clear();
	}
}
```

### Listing 118 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/MeshFormat.h` (NEW)
```cpp
#pragma once
#include <cstdint>
#include <vector>

namespace noc
{
	// Minimal binary mesh format for Phase 9.5:
	// Header:
	//   char     magic[4] = "NMSH"
	//   uint32   version  = 1
	//   uint32   vertexCount
	//   uint32   indexCount
	// Vertex:
	//   float3 position
	//   float4 color
	// Indices:
	//   uint16 indexCount entries
	struct MeshVertexPC
	{
		float px, py, pz;
		float r, g, b, a;
	};

	struct CpuMeshPC
	{
		std::vector<MeshVertexPC> vertices;
		std::vector<uint16_t> indices;
	};

	// Returns false on parse error.
	bool ParseNocMeshPC(const uint8_t* bytes, size_t size, CpuMeshPC& out, const char*& outErr);
}
```

### Listing 119 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/MeshFormat.cpp` (NEW)
```cpp
#include "MeshFormat.h"
#include <cstring>

namespace noc
{
	static bool ReadU32_(const uint8_t*& p, const uint8_t* end, uint32_t& out)
	{
		if (p + 4 > end) return false;
		memcpy(&out, p, 4);
		p += 4;
		return true;
	}

	bool ParseNocMeshPC(const uint8_t* bytes, size_t size, CpuMeshPC& out, const char*& outErr)
	{
		outErr = nullptr;
		out.vertices.clear();
		out.indices.clear();

		if (!bytes || size < 16)
		{
			outErr = "mesh: too small";
			return false;
		}

		const uint8_t* p = bytes;
		const uint8_t* end = bytes + size;

		char magic[4]{};
		memcpy(magic, p, 4);
		p += 4;

		if (memcmp(magic, "NMSH", 4) != 0)
		{
			outErr = "mesh: bad magic";
			return false;
		}

		uint32_t ver = 0, vc = 0, ic = 0;
		if (!ReadU32_(p, end, ver) || !ReadU32_(p, end, vc) || !ReadU32_(p, end, ic))
		{
			outErr = "mesh: header truncated";
			return false;
		}

		if (ver != 1)
		{
			outErr = "mesh: unsupported version";
			return false;
		}

		const size_t vBytes = (size_t)vc * sizeof(MeshVertexPC);
		const size_t iBytes = (size_t)ic * sizeof(uint16_t);

		if ((size_t)(end - p) < vBytes + iBytes)
		{
			outErr = "mesh: payload truncated";
			return false;
		}

		out.vertices.resize(vc);
		memcpy(out.vertices.data(), p, vBytes);
		p += vBytes;

		out.indices.resize(ic);
		memcpy(out.indices.data(), p, iBytes);
		p += iBytes;

		return true;
	}
}
```

### Listing 120 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/ShaderCompiler.h` (MODIFIED)
```cpp
#pragma once
#include "Dx12Common.h"
#include <string>

namespace noc
{
	class ShaderCompiler
	{
	public:
		// Compile from an in-memory HLSL string (VFS-backed source).
		static bool CompileFromMemory(
			const char* debugName, // used for error messages
			const char* sourceUtf8,
			size_t sourceBytes,
			const char* entry,
			const char* target,
			dx12::ComPtr<ID3DBlob>& outBytecode);

		// Kept from Phase 9 (if you already had it).
		static bool CompileFromFile(
			const wchar_t* filePath,
			const char* entry,
			const char* target,
			dx12::ComPtr<ID3DBlob>& outBytecode);
	};
}
```

### Listing 121 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/ShaderCompiler.cpp` (MODIFIED)
```cpp
#include "ShaderCompiler.h"

namespace noc
{
	bool ShaderCompiler::CompileFromMemory(
		const char* debugName,
		const char* sourceUtf8,
		size_t sourceBytes,
		const char* entry,
		const char* target,
		dx12::ComPtr<ID3DBlob>& outBytecode)
	{
		outBytecode.Reset();

		if (!sourceUtf8 || sourceBytes == 0 || !entry || !target)
			return false;

		UINT flags = 0;
#if defined(_DEBUG)
		flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

		dx12::ComPtr<ID3DBlob> errors;
		HRESULT hr = D3DCompile(
			sourceUtf8,
			sourceBytes,
			debugName ? debugName : "noc_shader",
			nullptr,
			nullptr,
			entry,
			target,
			flags,
			0,
			&outBytecode,
			&errors);

		if (FAILED(hr))
		{
			const char* e = errors ? (const char*)errors->GetBufferPointer() : "unknown";
			NOC_LOG_ERROR("Render", "D3DCompile failed (%s:%s/%s): %s", debugName ? debugName : "mem", entry, target, e);
			return false;
		}

		return true;
	}

	bool ShaderCompiler::CompileFromFile(
		const wchar_t* filePath,
		const char* entry,
		const char* target,
		dx12::ComPtr<ID3DBlob>& outBytecode)
	{
		outBytecode.Reset();
		if (!filePath || !entry || !target)
			return false;

		UINT flags = 0;
#if defined(_DEBUG)
		flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

		dx12::ComPtr<ID3DBlob> errors;
		HRESULT hr = D3DCompileFromFile(
			filePath,
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entry,
			target,
			flags,
			0,
			&outBytecode,
			&errors);

		if (FAILED(hr))
		{
			const char* e = errors ? (const char*)errors->GetBufferPointer() : "unknown";
			NOC_LOG_ERROR("Render", "D3DCompileFromFile failed: %s", e);
			return false;
		}
		return true;
	}
}
```

### Listing 122 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/MeshPass.h` (NEW — replaces TrianglePass)
```cpp
#pragma once
#include "Dx12Common.h"

#include "Dx12DescriptorAllocator.h"
#include "Dx12DeferredReleaseQueue.h"
#include "Dx12PsoCache.h"
#include "GpuBuffer.h"
#include "GpuRingConstantBuffer.h"
#include "MeshFormat.h"

#include "Resources/ResourceHandle.h"
#include "Resources/Typed/ResourceHandleT.h"
#include "Resources/Typed/TextResource.h"

namespace noc
{
	class ResourceManager;
	class Dx12SwapChain;
	class Dx12FrameSync;

	// Phase 9.5: a minimal “real” draw pass:
	// - shader source from VFS (TextResource)
	// - mesh bytes from VFS (Binary)
	// - default-heap VB/IB
	// - descriptor table for per-frame CBV
	class MeshPass
	{
	public:
		bool Init(
			ID3D12Device* device,
			Dx12DescriptorAllocator& cbvSrvUav,
			Dx12DescriptorAllocator& samplers,
			Dx12PsoCache& psoCache);

		void Shutdown(Dx12DeferredReleaseQueue& deferred, uint64_t safeFenceValue);

		// Called every frame after cmd list is reset and RT is in RT state.
		void Record(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmd,
			Dx12SwapChain& swap,
			const Dx12FrameSync& sync,
			uint32_t frameIndex,
			Dx12DeferredReleaseQueue& deferred,
			ResourceManager* rm);

	private:
		bool EnsureRootSigAndPso_(
			ID3D12Device* device,
			Dx12PsoCache& cache,
			ResourceManager* rm);

		bool EnsureMeshUploaded_(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmd,
			Dx12DeferredReleaseQueue& deferred,
			const Dx12FrameSync& sync,
			uint32_t frameIndex,
			ResourceManager* rm);

		void EnsurePerFrameCbv_(ID3D12Device* device);

	private:
		// --- Assets (CPU) ---
		ResourceHandle meshBin_{};                 // RequestBinary("Meshes/triangle.nmsh")
		ResourceHandleT<TextResource> shaderHlsl_; // RequestText("Shaders/Basic.hlsl")

		// --- GPU objects ---
		dx12::ComPtr<ID3D12RootSignature> rootSig_;
		dx12::ComPtr<ID3D12PipelineState> pso_;

		GpuBuffer vb_;
		GpuBuffer ib_;
		uint32_t indexCount_ = 0;

		GpuRingConstantBuffer perFrameCB_;
		Dx12DescriptorAllocator* cbvSrvUav_ = nullptr;
		Dx12DescriptorHandle perFrameCbv_[dx12::kFrameCount]{};

		Dx12PsoCache* psoCache_ = nullptr;

		// state flags
		bool rootReady_ = false;
		bool psoReady_ = false;
		bool meshReady_ = false;
		bool cbReady_ = false;
	};
}
```

### Listing 123 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/MeshPass.cpp` (NEW)
```cpp
#include "MeshPass.h"

#include "Dx12SwapChain.h"
#include "Dx12FrameSync.h"
#include "ShaderCompiler.h"

#include "Resources/ResourceManager.h"

namespace noc
{
	struct PerFrameConstants
	{
		float time;
		float pad[3];
	};

	static uint64_t HashInputLayoutPC_()
	{
		// Stable constant for Phase 9.5: position+color layout.
		// (Design choice) Replace with real hashing later.
		return 0xA0C0CA11u;
	}

	bool MeshPass::Init(
		ID3D12Device* device,
		Dx12DescriptorAllocator& cbvSrvUav,
		Dx12DescriptorAllocator& samplers,
		Dx12PsoCache& psoCache)
	{
		(void)samplers;

		if (!device)
			return false;

		cbvSrvUav_ = &cbvSrvUav;
		psoCache_ = &psoCache;

		// Request assets (vpaths are relative to your mounted content root).
		// These return immediately; readiness is polled in Record().
		// NOTE: shader file should live at Data/Shaders/Basic.hlsl, and mesh at Data/Meshes/triangle.nmsh.
		// VFS mount in Phase 3/6 already maps Data/ as root.
		// If your VFS uses different conventions, adjust vpaths accordingly.
		rootReady_ = false;
		psoReady_ = false;
		meshReady_ = false;
		cbReady_ = false;

		// Per-frame constant buffers (upload heap, one per swap buffer).
		if (!perFrameCB_.Init(device, 64 * 1024))
			return false;

		return true;
	}

	void MeshPass::Shutdown(Dx12DeferredReleaseQueue& deferred, uint64_t safeFenceValue)
	{
		// Queue GPU objects for safe release (or release now after GPU idle).
		if (pso_)
		{
			dx12::ComPtr<IUnknown> u;
			pso_.As(&u);
			deferred.Enqueue(safeFenceValue, std::move(u));
			pso_.Reset();
		}
		if (rootSig_)
		{
			dx12::ComPtr<IUnknown> u;
			rootSig_.As(&u);
			deferred.Enqueue(safeFenceValue, std::move(u));
			rootSig_.Reset();
		}

		vb_.ShutdownNow();
		ib_.ShutdownNow();

		perFrameCB_.Shutdown();
	}

	void MeshPass::EnsurePerFrameCbv_(ID3D12Device* device)
	{
		if (cbReady_ || !device || !cbvSrvUav_)
			return;

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			perFrameCbv_[i] = cbvSrvUav_->Allocate();

			D3D12_CONSTANT_BUFFER_VIEW_DESC d{};
			d.BufferLocation = perFrameCB_.Resource(i)->GetGPUVirtualAddress();
			// CBV size must be 256-byte aligned.
			d.SizeInBytes = (UINT)((sizeof(PerFrameConstants) + 255u) & ~255u);

			device->CreateConstantBufferView(&d, perFrameCbv_[i].cpu);
		}

		cbReady_ = true;
	}

	bool MeshPass::EnsureRootSigAndPso_(ID3D12Device* device, Dx12PsoCache& cache, ResourceManager* rm)
	{
		if (!device || !rm)
			return false;

		// Request handles once.
		if (!shaderHlsl_.IsValid())
			shaderHlsl_ = rm->RequestText("Shaders/Basic.hlsl");

		// Root signature: build once (no dependency on asset readiness).
		if (!rootReady_)
		{
			// Root parameters:
			// 0: CBV table (b0) per-frame
			// 1: SRV table (t0..t7) per-draw/material (future)
			// 2: Sampler table (s0..s7) (future)
			D3D12_DESCRIPTOR_RANGE ranges[3]{};

			ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			ranges[0].NumDescriptors = 1;
			ranges[0].BaseShaderRegister = 0;
			ranges[0].RegisterSpace = 0;
			ranges[0].OffsetInDescriptorsFromTableStart = 0;

			ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			ranges[1].NumDescriptors = 8;
			ranges[1].BaseShaderRegister = 0;
			ranges[1].RegisterSpace = 0;
			ranges[1].OffsetInDescriptorsFromTableStart = 0;

			ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			ranges[2].NumDescriptors = 8;
			ranges[2].BaseShaderRegister = 0;
			ranges[2].RegisterSpace = 0;
			ranges[2].OffsetInDescriptorsFromTableStart = 0;

			D3D12_ROOT_PARAMETER params[3]{};

			params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[0].DescriptorTable.NumDescriptorRanges = 1;
			params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
			params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[1].DescriptorTable.NumDescriptorRanges = 1;
			params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
			params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[2].DescriptorTable.NumDescriptorRanges = 1;
			params[2].DescriptorTable.pDescriptorRanges = &ranges[2];
			params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_ROOT_SIGNATURE_DESC rs{};
			rs.NumParameters = 3;
			rs.pParameters = params;
			rs.NumStaticSamplers = 0;
			rs.pStaticSamplers = nullptr;
			rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			dx12::ComPtr<ID3DBlob> blob;
			dx12::ComPtr<ID3DBlob> err;
			if (!dx12::HrOk(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err), "SerializeRootSignature"))
			{
				const char* e = err ? (const char*)err->GetBufferPointer() : "unknown";
				NOC_LOG_ERROR("Render", "RootSig serialize error: %s", e);
				return false;
			}

			if (!dx12::HrOk(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig_)), "CreateRootSignature"))
				return false;

			rootReady_ = true;
		}

		// Shader compilation requires the TextResource to be ready.
		const TextResource* src = rm->GetText(shaderHlsl_);
		if (!src)
			return false; // not ready yet

		dx12::ComPtr<ID3DBlob> vs;
		dx12::ComPtr<ID3DBlob> ps;

		if (!ShaderCompiler::CompileFromMemory("Shaders/Basic.hlsl", src->Str().c_str(), src->Str().size(), "VSMain", "vs_5_1", vs))
			return false;
		if (!ShaderCompiler::CompileFromMemory("Shaders/Basic.hlsl", src->Str().c_str(), src->Str().size(), "PSMain", "ps_5_1", ps))
			return false;

		// PSO cache lookup.
		Dx12PsoKey key{};
		key.vs = vs.Get();
		key.ps = ps.Get();
		key.rootSig = rootSig_.Get();
		key.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		key.inputLayoutHash = HashInputLayoutPC_();

		if (auto* cached = cache.Find(key))
		{
			pso_ = cached;
			psoReady_ = true;
			return true;
		}

		// Create PSO
		D3D12_INPUT_ELEMENT_DESC layout[2]{};
		layout[0].SemanticName = "POSITION";
		layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		layout[0].InputSlot = 0;
		layout[0].AlignedByteOffset = 0;
		layout[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

		layout[1].SemanticName = "COLOR";
		layout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		layout[1].InputSlot = 0;
		layout[1].AlignedByteOffset = 12;
		layout[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = rootSig_.Get();
		pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pso.DepthStencilState.DepthEnable = FALSE;
		pso.DepthStencilState.StencilEnable = FALSE;
		pso.SampleMask = UINT_MAX;
		pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso.NumRenderTargets = 1;
		pso.RTVFormats[0] = key.rtvFormat;
		pso.SampleDesc.Count = 1;
		pso.InputLayout = { layout, 2 };

		dx12::ComPtr<ID3D12PipelineState> created;
		if (!dx12::HrOk(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&created)), "CreateGraphicsPipelineState"))
			return false;

		pso_ = created;
		cache.Insert(key, std::move(created));
		psoReady_ = true;
		return true;
	}

	bool MeshPass::EnsureMeshUploaded_(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12DeferredReleaseQueue& deferred,
		const Dx12FrameSync& sync,
		uint32_t frameIndex,
		ResourceManager* rm)
	{
		if (meshReady_)
			return true;

		if (!meshBin_.IsValid())
			meshBin_ = rm->RequestBinary("Meshes/triangle.nmsh");

		if (!rm->IsReady(meshBin_))
			return false;

		const uint8_t* bytes = rm->GetBytes(meshBin_);
		const size_t size = rm->GetSize(meshBin_);
		if (!bytes || size == 0)
			return false;

		CpuMeshPC cpu{};
		const char* err = nullptr;
		if (!ParseNocMeshPC(bytes, size, cpu, err))
		{
			NOC_LOG_ERROR("Render", "Mesh parse failed: %s", err ? err : "unknown");
			return false;
		}

		indexCount_ = (uint32_t)cpu.indices.size();

		if (!vb_.CreateStatic(device, cmd, deferred, sync, frameIndex, GpuBuffer::Kind::Vertex,
			cpu.vertices.data(), cpu.vertices.size() * sizeof(MeshVertexPC), sizeof(MeshVertexPC)))
			return false;

		if (!ib_.CreateStatic(device, cmd, deferred, sync, frameIndex, GpuBuffer::Kind::Index,
			cpu.indices.data(), cpu.indices.size() * sizeof(uint16_t), 0))
			return false;

		meshReady_ = true;
		return true;
	}

	void MeshPass::Record(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12SwapChain& swap,
		const Dx12FrameSync& sync,
		uint32_t frameIndex,
		Dx12DeferredReleaseQueue& deferred,
		ResourceManager* rm)
	{
		if (!device || !cmd)
			return;

		// Always ensure per-frame CBV descriptors exist.
		EnsurePerFrameCbv_(device);

		// Clear RT (swap already provides RTV).
		auto rtv = swap.CurrentRtv(frameIndex);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		const float clearColor[4] = { 0.05f, 0.05f, 0.08f, 1.0f };
		cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

		if (!rm)
			return;

		// Build root/PSO when shader becomes ready.
		if (!EnsureRootSigAndPso_(device, *psoCache_, rm))
			return;

		// Upload mesh when bytes become ready.
		if (!EnsureMeshUploaded_(device, cmd, deferred, sync, frameIndex, rm))
			return;

		// Write per-frame constants.
		perFrameCB_.BeginFrame(frameIndex);
		D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
		void* cpu = nullptr;
		if (perFrameCB_.Allocate(sizeof(PerFrameConstants), gpu, cpu))
		{
			auto* c = (PerFrameConstants*)cpu;
			c->time = 0.0f; // (Design choice) wire real time later
		}

		// Bind descriptor heaps (shader-visible).
		ID3D12DescriptorHeap* heaps[] = { cbvSrvUav_->Heap() };
		cmd->SetDescriptorHeaps(1, heaps);

		cmd->SetGraphicsRootSignature(rootSig_.Get());
		cmd->SetPipelineState(pso_.Get());

		// Root slot 0: per-frame CBV table (b0)
		cmd->SetGraphicsRootDescriptorTable(0, perFrameCbv_[frameIndex].gpu);

		// Viewport/scissor from swapchain size.
		D3D12_VIEWPORT vp{};
		vp.Width = (float)swap.Width();
		vp.Height = (float)swap.Height();
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;

		D3D12_RECT sc{};
		sc.left = 0;
		sc.top = 0;
		sc.right = (LONG)swap.Width();
		sc.bottom = (LONG)swap.Height();

		cmd->RSSetViewports(1, &vp);
		cmd->RSSetScissorRects(1, &sc);

		// IA bind
		auto vbv = vb_.VertexView();
		auto ibv = ib_.IndexView(DXGI_FORMAT_R16_UINT);

		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 1, &vbv);
		cmd->IASetIndexBuffer(&ibv);

		cmd->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
	}
}
```

### Listing 124 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12Renderer.h` (MODIFIED)
```cpp
#pragma once
#include "Dx12Common.h"

#include "Dx12Device.h"
#include "Dx12SwapChain.h"
#include "Dx12FrameSync.h"

#include "Dx12DescriptorAllocator.h"
#include "Dx12DeferredReleaseQueue.h"
#include "Dx12PsoCache.h"
#include "MeshPass.h"

namespace noc
{
	class ResourceManager;

	class Dx12Renderer
	{
	public:
		bool Init(bool enableDebugLayer);
		void Shutdown();

		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		void SetResourceManager(ResourceManager* rm) { rm_ = rm; }

		void BeginFrame();
		void EndFramePresent();

	private:
		void LogDeviceRemoved_(const char* where);

	private:
		bool inited_ = false;
		bool attached_ = false;

		ResourceManager* rm_ = nullptr;

		Dx12Device device_;
		Dx12SwapChain swap_;
		Dx12FrameSync sync_;

		dx12::ComPtr<ID3D12CommandAllocator> cmdAlloc_[dx12::kFrameCount];
		dx12::ComPtr<ID3D12GraphicsCommandList> cmdList_;

		uint32_t frameIndex_ = 0;
		bool frameOpen_ = false;

		// Phase 9.5: resource foundation
		Dx12DescriptorAllocator cbvSrvUavHeap_;
		Dx12DescriptorAllocator samplerHeap_; // not used yet, but reserved for materials
		Dx12DeferredReleaseQueue deferred_;
		Dx12PsoCache psoCache_;

		MeshPass meshPass_;
	};
}
```

### Listing 125 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12Renderer.cpp` (MODIFIED)
```cpp
#include "Dx12Renderer.h"

#include "Resources/ResourceManager.h"

namespace noc
{
	bool Dx12Renderer::Init(bool enableDebugLayer)
	{
		if (inited_)
			return true;

		if (!device_.Init(enableDebugLayer))
			return false;

		if (!sync_.Init(device_.Device()))
			return false;

		inited_ = true;
		NOC_LOG_INFO("Render", "Dx12Renderer initialized (awaiting AttachToWindow)");
		return true;
	}

	void Dx12Renderer::Shutdown()
	{
		if (!inited_)
			return;

		// Ensure GPU is idle before releasing.
		if (attached_)
			sync_.WaitForGpu(device_.Queue());

		// After GPU idle, it is safe to clear deferred queue immediately.
		deferred_.Clear();

		meshPass_.Shutdown(deferred_, /*safeFenceValue*/sync_.CompletedValue());

		cmdList_.Reset();
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			cmdAlloc_[i].Reset();

		psoCache_.Clear();

		cbvSrvUavHeap_.Shutdown();
		samplerHeap_.Shutdown();

		swap_.Shutdown();
		sync_.Shutdown();
		device_.Shutdown();

		inited_ = false;
		attached_ = false;

		NOC_LOG_INFO("Render", "Dx12Renderer shutdown");
	}

	bool Dx12Renderer::AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!inited_)
			return false;

		if (!swap_.Init(device_.Factory(), device_.Queue(), nativeHwnd, clientWidth, clientHeight))
			return false;

		if (!swap_.CreateRtvHeapAndViews(device_.Device()))
			return false;

		frameIndex_ = swap_.FrameIndex();

		// Commands: allocators + one list.
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (!dx12::HrOk(device_.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc_[i])),
				"CreateCommandAllocator"))
			{
				return false;
			}
		}

		if (!dx12::HrOk(device_.Device()->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc_[frameIndex_].Get(), nullptr, IID_PPV_ARGS(&cmdList_)),
			"CreateCommandList"))
		{
			return false;
		}
		dx12::HrOk(cmdList_->Close(), "cmdList->Close (initial)");

		// Phase 9.5: descriptor heaps (persistent; no per-frame recreation).
		if (!cbvSrvUavHeap_.Init(device_.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true))
			return false;
		if (!samplerHeap_.Init(device_.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 64, true))
			return false;

		// Phase 9.5: mesh pass
		if (!meshPass_.Init(device_.Device(), cbvSrvUavHeap_, samplerHeap_, psoCache_))
			return false;

		attached_ = true;
		NOC_LOG_INFO("Render", "Dx12Renderer attached (%ux%u)", clientWidth, clientHeight);
		return true;
	}

	void Dx12Renderer::BeginFrame()
	{
		if (!attached_)
			return;

		if (device_.Device()->GetDeviceRemovedReason() != S_OK)
			return;

		if (frameOpen_)
		{
			NOC_LOG_ERROR("Render", "BeginFrame called while frame is already open");
			return;
		}
		frameOpen_ = true;

		// Collect deferred releases for everything the GPU has finished.
		deferred_.Collect(sync_.CompletedValue());

		// Reset allocator/list for current back buffer.
		if (FAILED(cmdAlloc_[frameIndex_]->Reset()))
		{
			LogDeviceRemoved_("cmdAlloc->Reset");
			frameOpen_ = false;
			return;
		}

		if (FAILED(cmdList_->Reset(cmdAlloc_[frameIndex_].Get(), nullptr)))
		{
			LogDeviceRemoved_("cmdList->Reset");
			frameOpen_ = false;
			return;
		}

		// Transition Present->RT for current back buffer.
		swap_.TransitionTo(cmdList_.Get(), frameIndex_, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	void Dx12Renderer::EndFramePresent()
	{
		if (!attached_)
			return;

		if (device_.Device()->GetDeviceRemovedReason() != S_OK)
			return;

		if (!frameOpen_)
		{
			NOC_LOG_ERROR("Render", "EndFramePresent called without BeginFrame");
			return;
		}
		struct Guard { bool& b; ~Guard() { b = false; } } g{ frameOpen_ };

		// Record pass into cmd list (clear + draw if assets ready).
		meshPass_.Record(device_.Device(), cmdList_.Get(), swap_, sync_, frameIndex_, deferred_, rm_);

		// Transition RT->Present.
		swap_.TransitionTo(cmdList_.Get(), frameIndex_, D3D12_RESOURCE_STATE_PRESENT);

		if (FAILED(cmdList_->Close()))
		{
			LogDeviceRemoved_("cmdList->Close");
			return;
		}

		ID3D12CommandList* lists[] = { cmdList_.Get() };
		device_.Queue()->ExecuteCommandLists(1, lists);

		// Present
		swap_.Present();

		// Signal + advance/wait using Phase 8/9 model.
		sync_.MoveToNextFrame(device_.Queue(), swap_.SwapChain(), frameIndex_);
		swap_.UpdateFrameIndex(frameIndex_);
	}

	void Dx12Renderer::LogDeviceRemoved_(const char* where)
	{
		HRESULT hr = device_.Device()->GetDeviceRemovedReason();
		NOC_LOG_ERROR("Render", "Device removed at %s (hr=0x%08X)", where, (unsigned)hr);
	}
}
```

### Listing 126 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12SwapChain.h` (MODIFIED — small additions)
```cpp
#pragma once
#include "Dx12Common.h"
#include <cstdint>

namespace noc
{
	class Dx12SwapChain
	{
	public:
		bool Init(IDXGIFactory6* factory, ID3D12CommandQueue* queue, void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);
		void Shutdown();

		bool CreateRtvHeapAndViews(ID3D12Device* device);

		void Present();
		uint32_t FrameIndex() const { return frameIndex_; }
		void UpdateFrameIndex(uint32_t i) { frameIndex_ = i; }

		IDXGISwapChain3* SwapChain() const { return swapChain_.Get(); }

		uint32_t Width() const { return width_; }
		uint32_t Height() const { return height_; }

		D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv(uint32_t frameIndex) const;

		void TransitionTo(ID3D12GraphicsCommandList* cmd, uint32_t frameIndex, D3D12_RESOURCE_STATES to);

	private:
		uint32_t width_ = 0, height_ = 0;
		uint32_t frameIndex_ = 0;

		dx12::ComPtr<IDXGISwapChain3> swapChain_;

		dx12::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
		uint32_t rtvDescriptorSize_ = 0;

		dx12::ComPtr<ID3D12Resource> backBuffers_[dx12::kFrameCount];
		D3D12_RESOURCE_STATES bbState_[dx12::kFrameCount]{};
	};
}
```

### Listing 127 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Render/DX12/Dx12SwapChain.cpp` (MODIFIED — provides helpers used by MeshPass)
```cpp
#include "Dx12SwapChain.h"

namespace noc
{
	bool Dx12SwapChain::Init(IDXGIFactory6* factory, ID3D12CommandQueue* queue, void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!factory || !queue || !nativeHwnd || clientWidth == 0 || clientHeight == 0)
			return false;

		width_ = clientWidth;
		height_ = clientHeight;

		DXGI_SWAP_CHAIN_DESC1 sc{};
		sc.BufferCount = dx12::kFrameCount;
		sc.Width = clientWidth;
		sc.Height = clientHeight;
		sc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sc.SampleDesc.Count = 1;

		dx12::ComPtr<IDXGISwapChain1> sc1;
		if (!dx12::HrOk(factory->CreateSwapChainForHwnd(queue, (HWND)nativeHwnd, &sc, nullptr, nullptr, &sc1), "CreateSwapChainForHwnd"))
			return false;

		factory->MakeWindowAssociation((HWND)nativeHwnd, DXGI_MWA_NO_ALT_ENTER);

		if (!dx12::HrOk(sc1.As(&swapChain_), "SwapChain1.As(SwapChain3)"))
			return false;

		frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
		return true;
	}

	void Dx12SwapChain::Shutdown()
	{
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			backBuffers_[i].Reset();

		rtvHeap_.Reset();
		swapChain_.Reset();
		width_ = height_ = 0;
		frameIndex_ = 0;
		rtvDescriptorSize_ = 0;
	}

	bool Dx12SwapChain::CreateRtvHeapAndViews(ID3D12Device* device)
	{
		if (!device || !swapChain_)
			return false;

		D3D12_DESCRIPTOR_HEAP_DESC hd{};
		hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		hd.NumDescriptors = dx12::kFrameCount;
		hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		if (!dx12::HrOk(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvHeap_)), "CreateDescriptorHeap(RTV)"))
			return false;

		rtvDescriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_CPU_DESCRIPTOR_HANDLE base = rtvHeap_->GetCPUDescriptorHandleForHeapStart();

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (!dx12::HrOk(swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i])), "SwapChain.GetBuffer"))
				return false;

			D3D12_CPU_DESCRIPTOR_HANDLE h = base;
			h.ptr += (SIZE_T)i * (SIZE_T)rtvDescriptorSize_;
			device->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, h);

			bbState_[i] = D3D12_RESOURCE_STATE_PRESENT;
		}

		return true;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Dx12SwapChain::CurrentRtv(uint32_t frameIndex) const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE h{};
		if (!rtvHeap_)
			return h;

		h = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
		h.ptr += (SIZE_T)frameIndex * (SIZE_T)rtvDescriptorSize_;
		return h;
	}

	void Dx12SwapChain::TransitionTo(ID3D12GraphicsCommandList* cmd, uint32_t frameIndex, D3D12_RESOURCE_STATES to)
	{
		if (!cmd)
			return;

		if (bbState_[frameIndex] == to)
			return;

		D3D12_RESOURCE_BARRIER b = dx12::TransitionBarrier(backBuffers_[frameIndex].Get(), bbState_[frameIndex], to);
		cmd->ResourceBarrier(1, &b);
		bbState_[frameIndex] = to;
	}

	void Dx12SwapChain::Present()
	{
		if (!swapChain_)
			return;

		swapChain_->Present(1, 0);
	}
}
```

### Listing 128 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 5) Full C++ implementations (Phase 9.5) > `Engine/Runtime/Engine.cpp` (MODIFIED — wire ResourceManager into RenderSystem)
```cpp
#include "Engine.h"

#include <algorithm>
#include <span>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Clock.h"

#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h"

namespace noc {

	IAllocator& Engine::Allocator() { return *alloc_; }
	LinearArena& Engine::FrameArena() { return frameArena_; }

	EngineConfig& Engine::ConfigMutable()
	{
		if (initialized_)
		{
			NOC_LOG_ERROR("Runtime", "EngineConfig is frozen after Init(). Modify config before calling Init().");
			return cfg_;
		}
		return cfg_;
	}

	bool Engine::SetContentRoot(const char* path)
	{
		if (!IsConfigMutable())
		{
			NOC_LOG_ERROR("Runtime", "SetContentRoot() called after Init(); ignored.");
			return false;
		}
		return cfg_.SetContentRoot(path);
	}

	bool Engine::SetOverrideRoot(const char* path)
	{
		if (!IsConfigMutable())
		{
			NOC_LOG_ERROR("Runtime", "SetOverrideRoot() called after Init(); ignored.");
			return false;
		}
		return cfg_.SetOverrideRoot(path);
	}

	bool Engine::SetArchivePath(const char* path)
	{
		if (!IsConfigMutable())
		{
			NOC_LOG_ERROR("Runtime", "SetArchivePath() called after Init(); ignored.");
			return false;
		}
		return cfg_.SetArchivePath(path);
	}

	bool Engine::Init()
	{
		// --- existing init path (core + vfs + jobs + resources + input) ---
		if (!registry_.StartupAll(this))
			return false;

#if NOC_ENABLE_ASSERTS
		const bool enableDebugLayer = true;
#else
		const bool enableDebugLayer = false;
#endif
		if (!render_.Init(enableDebugLayer))
			return false;

		// Phase 9.5: allow renderer to request assets through the ResourceManager.
		render_.SetResourceManager(&resources_);

		initialized_ = true;
		return true;
	}

	// ... rest of file unchanged (AttachWindow/Run/BeginFrame/Tick/EndFrame/Shutdown) ...
}
```

### Listing 129 — `Phase 9.5 — GPU Resource Foundation (DX12 Memory, Descriptors, Asset-Backed GPU Resources).md` — 8) Next chat handoff (ONLY what you should say/bring next) > Appendix: `Data/Shaders/Basic.hlsl` (you create this file)
```hlsl
struct VSIn
{
    float3 pos   : POSITION;
    float4 color : COLOR;
};

struct VSOut
{
    float4 pos   : SV_Position;
    float4 color : COLOR;
};

cbuffer PerFrame : register(b0)
{
    float gTime;
    float3 _pad;
};

VSOut VSMain(VSIn v)
{
    VSOut o;
    o.pos = float4(v.pos, 1.0);
    o.color = v.color;
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    return i.color;
}
```

### Listing 130 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Core/Math/MathTypes.h` (NEW)
```cpp
#pragma once
#include <cmath>
#include <cstdint>

namespace noc
{
    // ============================================================
    // Vec3
    // ============================================================

    struct Vec3
    {
        float x{}, y{}, z{};

        constexpr Vec3() = default;
        constexpr Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}

        static constexpr Vec3 Zero() { return { 0,0,0 }; }
        static constexpr Vec3 One() { return { 1,1,1 }; }

        friend constexpr Vec3 operator+(const Vec3& a, const Vec3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
        friend constexpr Vec3 operator-(const Vec3& a, const Vec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
        friend constexpr Vec3 operator*(const Vec3& v, float s) { return { v.x * s, v.y * s, v.z * s }; }
        friend constexpr Vec3 operator*(float s, const Vec3& v) { return v * s; }
    };

    inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

    inline Vec3 Cross(const Vec3& a, const Vec3& b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    inline float LengthSq(const Vec3& v) { return Dot(v, v); }
    inline float Length(const Vec3& v) { return std::sqrt(LengthSq(v)); }

    inline Vec3 Normalize(const Vec3& v)
    {
        const float len = Length(v);
        if (len <= 1e-6f) return Vec3::Zero();
        return v * (1.0f / len);
    }

    // ============================================================
    // Quat
    // ============================================================

    struct Quat
    {
        float x{}, y{}, z{}, w{ 1.0f };

        constexpr Quat() = default;
        constexpr Quat(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}

        static constexpr Quat Identity() { return { 0,0,0,1 }; }
    };

    // Rotate vector by unit quaternion (no matrices)
    inline Vec3 Rotate(const Quat& q, const Vec3& v)
    {
        Vec3 qv{ q.x, q.y, q.z };
        Vec3 t = Cross(qv, v) * 2.0f;
        return v + t * q.w + Cross(qv, t);
    }

    // ============================================================
    // Mat4 (COLUMN-MAJOR, m[col*4 + row])
    // ============================================================

    struct Mat4
    {
        float m[16]{};

        static Mat4 Identity()
        {
            Mat4 r{};
            r.m[0] = 1.0f;
            r.m[5] = 1.0f;
            r.m[10] = 1.0f;
            r.m[15] = 1.0f;
            return r;
        }
    };

    // Access helper: element at (row, col)
    inline float& M(Mat4& m, int row, int col) { return m.m[col * 4 + row]; }
    inline float  M(const Mat4& m, int row, int col) { return m.m[col * 4 + row]; }

    // Matrix multiply (column-major, column vectors): r = a * b
    inline Mat4 Mul(const Mat4& a, const Mat4& b)
    {
        Mat4 r{};
        for (int c = 0; c < 4; ++c)
        {
            for (int rrow = 0; rrow < 4; ++rrow)
            {
                M(r, rrow, c) =
                    M(a, rrow, 0) * M(b, 0, c) +
                    M(a, rrow, 1) * M(b, 1, c) +
                    M(a, rrow, 2) * M(b, 2, c) +
                    M(a, rrow, 3) * M(b, 3, c);
            }
        }
        return r;
    }

    // Transform point (column vector): p' = M * [p,1]
    inline Vec3 TransformPoint(const Mat4& m, const Vec3& p)
    {
        const float x = M(m, 0, 0) * p.x + M(m, 0, 1) * p.y + M(m, 0, 2) * p.z + M(m, 0, 3) * 1.0f;
        const float y = M(m, 1, 0) * p.x + M(m, 1, 1) * p.y + M(m, 1, 2) * p.z + M(m, 1, 3) * 1.0f;
        const float z = M(m, 2, 0) * p.x + M(m, 2, 1) * p.y + M(m, 2, 2) * p.z + M(m, 2, 3) * 1.0f;
        return { x,y,z };
    }

    inline Mat4 Translation(const Vec3& t)
    {
        Mat4 r = Mat4::Identity();
        r.m[12] = t.x;
        r.m[13] = t.y;
        r.m[14] = t.z;
        return r;
    }

    inline Mat4 Scale(const Vec3& s)
    {
        Mat4 r{};
        r.m[0] = s.x;
        r.m[5] = s.y;
        r.m[10] = s.z;
        r.m[15] = 1.0f;
        return r;
    }

    inline Mat4 RotationFromQuat(const Quat& q)
    {
        const float x = q.x, y = q.y, z = q.z, w = q.w;
        const float xx = x * x, yy = y * y, zz = z * z;
        const float xy = x * y, xz = x * z, yz = y * z;
        const float wx = w * x, wy = w * y, wz = w * z;

        Mat4 r = Mat4::Identity();

        // column-major rotation matrix
        r.m[0] = 1.0f - 2.0f * (yy + zz);
        r.m[1] = 2.0f * (xy + wz);
        r.m[2] = 2.0f * (xz - wy);

        r.m[4] = 2.0f * (xy - wz);
        r.m[5] = 1.0f - 2.0f * (xx + zz);
        r.m[6] = 2.0f * (yz + wx);

        r.m[8] = 2.0f * (xz + wy);
        r.m[9] = 2.0f * (yz - wx);
        r.m[10] = 1.0f - 2.0f * (xx + yy);

        return r;
    }

    inline Mat4 TRS(const Vec3& t, const Quat& r, const Vec3& s)
    {
        // Column-vector convention: M = T * R * S
        return Mul(Translation(t), Mul(RotationFromQuat(r), Scale(s)));
    }

    // ============================================================
    // Camera matrices
    // ============================================================

    inline Mat4 LookToLH(const Vec3& eye, const Vec3& dir, const Vec3& up)
    {
        const Vec3 zaxis = Normalize(dir);
        const Vec3 xaxis = Normalize(Cross(up, zaxis));
        const Vec3 yaxis = Cross(zaxis, xaxis);

        Mat4 r = Mat4::Identity();

        // basis vectors into columns
        r.m[0] = xaxis.x; r.m[1] = xaxis.y; r.m[2] = xaxis.z;
        r.m[4] = yaxis.x; r.m[5] = yaxis.y; r.m[6] = yaxis.z;
        r.m[8] = zaxis.x; r.m[9] = zaxis.y; r.m[10] = zaxis.z;

        // translation
        r.m[12] = -Dot(xaxis, eye);
        r.m[13] = -Dot(yaxis, eye);
        r.m[14] = -Dot(zaxis, eye);

        return r;
    }

    // D3D-style LH perspective, depth 0..1
    inline Mat4 PerspectiveFovLH(float fovY, float aspect, float zn, float zf)
    {
        Mat4 r{};
        const float yScale = 1.0f / std::tan(fovY * 0.5f);
        const float xScale = yScale / aspect;

        r.m[0] = xScale;
        r.m[5] = yScale;
        r.m[10] = zf / (zf - zn);
        r.m[11] = 1.0f;
        r.m[14] = (-zn * zf) / (zf - zn);
        return r;
    }
}
```

### Listing 131 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/Bounds.h` (NEW)
```cpp
#pragma once
#include "Core/Math/MathTypes.h"

namespace noc
{
	struct AABB
	{
		Vec3 min;
		Vec3 max;
	};

	inline AABB AabbInvalid()
	{
		return AABB{ Vec3(1e30f, 1e30f, 1e30f), Vec3(-1e30f,-1e30f,-1e30f) };
	}

	inline void AabbExpand(AABB& a, const Vec3& p)
	{
		if (p.x < a.min.x) a.min.x = p.x;
		if (p.y < a.min.y) a.min.y = p.y;
		if (p.z < a.min.z) a.min.z = p.z;
		if (p.x > a.max.x) a.max.x = p.x;
		if (p.y > a.max.y) a.max.y = p.y;
		if (p.z > a.max.z) a.max.z = p.z;
	}

	inline AABB TransformAabb(const AABB& local, const Mat4& world)
	{
		// Conservative: transform all 8 corners.
		const Vec3 c[8] = {
			{local.min.x, local.min.y, local.min.z},
			{local.max.x, local.min.y, local.min.z},
			{local.min.x, local.max.y, local.min.z},
			{local.max.x, local.max.y, local.min.z},
			{local.min.x, local.min.y, local.max.z},
			{local.max.x, local.min.y, local.max.z},
			{local.min.x, local.max.y, local.max.z},
			{local.max.x, local.max.y, local.max.z},
		};

		AABB out = AabbInvalid();
		for (int i = 0; i < 8; ++i)
			AabbExpand(out, TransformPoint(world, c[i]));
		return out;
	}
}
```

### Listing 132 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/Frustum.h` (NEW)
```cpp
#pragma once
#include "Core/Math/MathTypes.h"
#include "Runtime/Bounds.h"
#include <cmath>

namespace noc
{
	struct Plane
	{
		// ax + by + cz + d >= 0 is inside
		float a = 0, b = 0, c = 0, d = 0;
	};

	struct Frustum
	{
		// 0..5: left,right,bottom,top,near,far
		Plane p[6]{};
	};

	inline void NormalizePlane(Plane& pl)
	{
		const float len = std::sqrtf(pl.a * pl.a + pl.b * pl.b + pl.c * pl.c);
		if (len > 1e-6f)
		{
			const float inv = 1.0f / len;
			pl.a *= inv; pl.b *= inv; pl.c *= inv; pl.d *= inv;
		}
	}

	inline Frustum FrustumFromViewProj(const Mat4& m)
	{
		// Mat4 is stored column-major in m.m[col*4 + row]
		auto at = [&](int row, int col) -> float { return m.m[col * 4 + row]; };

		Frustum f{};

		// Left   = row3 + row0
		f.p[0] = Plane{ at(3,0) + at(0,0), at(3,1) + at(0,1), at(3,2) + at(0,2), at(3,3) + at(0,3) };
		// Right  = row3 - row0
		f.p[1] = Plane{ at(3,0) - at(0,0), at(3,1) - at(0,1), at(3,2) - at(0,2), at(3,3) - at(0,3) };
		// Bottom = row3 + row1
		f.p[2] = Plane{ at(3,0) + at(1,0), at(3,1) + at(1,1), at(3,2) + at(1,2), at(3,3) + at(1,3) };
		// Top    = row3 - row1
		f.p[3] = Plane{ at(3,0) - at(1,0), at(3,1) - at(1,1), at(3,2) - at(1,2), at(3,3) - at(1,3) };

		// D3D depth 0..1:
		// Near = row2
		f.p[4] = Plane{ at(2,0), at(2,1), at(2,2), at(2,3) };
		// Far  = row3 - row2
		f.p[5] = Plane{ at(3,0) - at(2,0), at(3,1) - at(2,1), at(3,2) - at(2,2), at(3,3) - at(2,3) };

		for (int i = 0; i < 6; ++i) NormalizePlane(f.p[i]);
		return f;
	}




	inline bool AabbInsidePlane(const AABB& a, const Plane& p)
	{
		// Positive vertex test
		Vec3 v;
		v.x = (p.a >= 0) ? a.max.x : a.min.x;
		v.y = (p.b >= 0) ? a.max.y : a.min.y;
		v.z = (p.c >= 0) ? a.max.z : a.min.z;

		const float dist = p.a * v.x + p.b * v.y + p.c * v.z + p.d;
		return dist >= 0.0f;
	}

	inline bool AabbIntersectsFrustum(const AABB& a, const Frustum& f)
	{
		for (int i = 0; i < 6; ++i)
		{
			if (!AabbInsidePlane(a, f.p[i]))
				return false;
		}
		return true;
	}
}
```

### Listing 133 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/SceneObject.h` (NEW)
```cpp
#pragma once
#include <cstdint>

namespace noc
{
	struct SceneObjectHandle
	{
		uint32_t index = 0xFFFFFFFFu;
		uint32_t generation = 0;

		bool IsValid() const { return index != 0xFFFFFFFFu; }
	};

	inline bool operator==(const SceneObjectHandle& a, const SceneObjectHandle& b)
	{
		return a.index == b.index && a.generation == b.generation;
	}
}
```

### Listing 134 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/Camera.h` (NEW)
```cpp
#pragma once
#include "Core/Math/MathTypes.h"

namespace noc
{
	struct Camera
	{
		float fovYRadians = 1.04719755f; // ~60 deg
		float aspect = 16.0f / 9.0f;
		float nearZ = 0.1f;
		float farZ = 500.0f;

		Mat4 view = Mat4::Identity();
		Mat4 proj = Mat4::Identity();
		Mat4 viewProj = Mat4::Identity();

		void Rebuild(const Vec3& eye, const Vec3& forward, const Vec3& up)
		{
			view = LookToLH(eye, forward, up);
			proj = PerspectiveFovLH(fovYRadians, aspect, nearZ, farZ);
			viewProj = Mul(proj, view); // column-major, column vectors

		}
	};
}

```

### Listing 135 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Render/RenderQueue.h` (NEW)
```cpp
#pragma once
#include <cstdint>

#include "Core/Math/MathTypes.h"
#include "Resources/ResourceHandle.h"

namespace noc
{
	struct RenderView
	{
		Mat4 viewProj;
		uint32_t viewportWidth = 0;
		uint32_t viewportHeight = 0;
	};

	struct RenderInstance
	{
		ResourceHandle mesh; // ResourceManager handle (NOT raw pointer)
		Mat4 world;
	};

	// POD render submission for a single frame.
	// Memory for instances is owned by the caller (FrameArena).
	struct RenderQueue
	{
		RenderView view{};
		const RenderInstance* instances = nullptr;
		uint32_t instanceCount = 0;
		uint32_t totalRenderables = 0;
	};
}
```

### Listing 136 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/World.h` (NEW)
```cpp
#pragma once
#include <cstdint>

#include "Runtime/SceneObject.h"
#include "Runtime/Bounds.h"
#include "Runtime/Camera.h"
#include "Runtime/Frustum.h"

#include "Core/Math/MathTypes.h"

#include "Resources/ResourceHandle.h"

namespace noc
{
	class IAllocator;
	class LinearArena;

	struct RenderQueue;

	struct WorldStats
	{
		uint32_t visible = 0;
		uint32_t total = 0;
	};

	class World
	{
	public:
		World() = default;

		bool Init(IAllocator& persistentAlloc);
		void Shutdown();

		// Frame update (transform propagation + bounds)
		void Update();

		// --- Object model ---
		SceneObjectHandle CreateObject();
		void DestroyObject(SceneObjectHandle h);

		// Deterministic iteration order for debugging: indices are stable in creation order
		uint32_t AliveCount() const;

		// --- Transform ---
		void SetLocalTRS(SceneObjectHandle h, const Vec3& t, const Quat& r, const Vec3& s);
		void SetParent(SceneObjectHandle child, SceneObjectHandle parent);
		Mat4 GetWorldMatrix(SceneObjectHandle h);

		// --- Renderable binding ---
		// localBounds is in object-local space.
		void SetRenderable(SceneObjectHandle h, ResourceHandle mesh, const AABB& localBounds);

		// --- Camera ---
		// Minimal: one active camera stored in the World (can be extended later).
		void SetCameraParams(float fovYRadians, float aspect, float nearZ, float farZ);
		void SetCameraFromObject(SceneObjectHandle h); // camera follows this object's transform
		void SetCullingEnabled(bool enabled) { cullingEnabled_ = enabled; }
		bool IsCullingEnabled() const { return cullingEnabled_; }
		void SetCullingMaxDistance(float meters); // 0 = disabled

		// --- Runtime → Render handoff (allocates from FrameArena) ---
		// Returns a RenderQueue whose instance array is allocated from frameArena.
		RenderQueue BuildRenderQueue(LinearArena& frameArena, uint32_t viewportW, uint32_t viewportH);



		const WorldStats& GetLastStats() const;
		void DebugRequestCullDump();

	private:
		struct Impl;
		Impl* impl_ = nullptr;
		float cullingMaxDistance_ = 0.0f; // 0 = disabled (Design choice)
		WorldStats lastStats_;

		bool cullingEnabled_ = true;
		bool debugCullDump_ = false;
	};
}
```

### Listing 137 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/World.cpp` (NEW)
```cpp
#include "World.h"

#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/LinearArena.h"

#include "Render/RenderQueue.h"
#include "Core/Math/MathTypes.h"
#include <cstring>

namespace noc
{
	static constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;


	struct TransformData
	{
		Vec3 localT = Vec3::Zero();
		Quat localR = Quat::Identity();
		Vec3 localS = Vec3::One();

		uint32_t parent = kInvalidIndex;
		uint32_t firstChild = kInvalidIndex;
		uint32_t nextSibling = kInvalidIndex;

		Mat4 world = Mat4::Identity();
		uint8_t dirty = 1;
	};

	struct RenderableData
	{
		uint8_t has = 0;
		ResourceHandle mesh{};
		AABB localBounds{};
		AABB worldBounds{};
	};

	struct World::Impl
	{
		IAllocator* alloc = nullptr;

		// Handle table
		uint32_t capacity = 0;
		uint32_t countAlive = 0;

		uint32_t* generations = nullptr;
		uint8_t* alive = nullptr;

		// deterministic creation order (indices)
		uint32_t* order = nullptr;
		uint32_t orderCount = 0;
		uint32_t orderCap = 0;

		// free list (stack)
		uint32_t* freeList = nullptr;
		uint32_t freeCount = 0;
		uint32_t freeCap = 0;

		TransformData* xform = nullptr;
		RenderableData* rend = nullptr;

		// camera follows an object (optional)
		uint32_t cameraFollowIndex = kInvalidIndex;
		Camera camera{};
		Frustum frustum{};

		bool EnsureCapacity(uint32_t newCap)
		{
			if (newCap <= capacity) return true;

			// grow to power-of-two-ish
			uint32_t target = (capacity == 0) ? 64u : capacity;
			while (target < newCap) target *= 2;

			auto reallocArr = [&](void*& ptr, size_t elemSize, size_t oldCount, size_t newCount) -> bool
				{
					void* n = alloc->Allocate(elemSize * newCount, 64);
					if (!n) return false;
					if (ptr && oldCount)
						memcpy(n, ptr, elemSize * oldCount);
					if (ptr)
						alloc->Deallocate(ptr);
					ptr = n;
					return true;
				};

			const uint32_t oldCap = capacity;
			if (!reallocArr((void*&)generations, sizeof(uint32_t), oldCap, target)) return false;
			if (!reallocArr((void*&)alive, sizeof(uint8_t), oldCap, target)) return false;
			if (!reallocArr((void*&)xform, sizeof(TransformData), oldCap, target)) return false;
			if (!reallocArr((void*&)rend, sizeof(RenderableData), oldCap, target)) return false;

			// init new slots
			for (uint32_t i = oldCap; i < target; ++i)
			{
				generations[i] = 1;
				alive[i] = 0;
				xform[i] = TransformData{};
				rend[i] = RenderableData{};
			}

			capacity = target;
			return true;
		}

		void PushOrder(uint32_t idx)
		{
			if (orderCount == orderCap)
			{
				const uint32_t newCap = (orderCap == 0) ? 64u : orderCap * 2;
				void* n = alloc->Allocate(sizeof(uint32_t) * newCap, 64);
				if (order && orderCount)
					memcpy(n, order, sizeof(uint32_t) * orderCount);
				if (order) alloc->Deallocate(order);
				order = (uint32_t*)n;
				orderCap = newCap;
			}
			order[orderCount++] = idx;
		}

		void PushFree(uint32_t idx)
		{
			if (freeCount == freeCap)
			{
				const uint32_t newCap = (freeCap == 0) ? 64u : freeCap * 2;
				void* n = alloc->Allocate(sizeof(uint32_t) * newCap, 64);
				if (freeList && freeCount)
					memcpy(n, freeList, sizeof(uint32_t) * freeCount);
				if (freeList) alloc->Deallocate(freeList);
				freeList = (uint32_t*)n;
				freeCap = newCap;
			}
			freeList[freeCount++] = idx;
		}

		bool IsAliveHandle(SceneObjectHandle h) const
		{
			if (!h.IsValid() || h.index >= capacity) return false;
			return alive[h.index] != 0 && generations[h.index] == h.generation;
		}

		void MarkDirtySubtree(uint32_t idx)
		{
			// No heap allocations: DFS using sibling pointers.
			xform[idx].dirty = 1;
			for (uint32_t c = xform[idx].firstChild; c != kInvalidIndex; c = xform[c].nextSibling)
				MarkDirtySubtree(c);
		}

		void DetachFromParent(uint32_t child)
		{
			const uint32_t p = xform[child].parent;
			if (p == kInvalidIndex) return;

			uint32_t* link = &xform[p].firstChild;
			while (*link != kInvalidIndex)
			{
				if (*link == child)
				{
					*link = xform[child].nextSibling;
					break;
				}
				link = &xform[*link].nextSibling;
			}

			xform[child].parent = kInvalidIndex;
			xform[child].nextSibling = kInvalidIndex;
		}

		void AttachToParent(uint32_t child, uint32_t parent)
		{
			xform[child].parent = parent;
			xform[child].nextSibling = xform[parent].firstChild;
			xform[parent].firstChild = child;
		}

		void UpdateWorldRecursive(uint32_t idx)
		{
			TransformData& t = xform[idx];

			Mat4 local = TRS(t.localT, t.localR, t.localS);
			if (t.parent != kInvalidIndex)
				t.world = Mul(xform[t.parent].world, local);
			else
				t.world = local;

			// bounds update (if renderable)
			if (rend[idx].has)
				rend[idx].worldBounds = TransformAabb(rend[idx].localBounds, t.world);

			t.dirty = 0;

			for (uint32_t c = t.firstChild; c != kInvalidIndex; c = xform[c].nextSibling)
			{
				if (xform[c].dirty)
					UpdateWorldRecursive(c);
				else
				{
					// parent changed implies child should have been marked; keep strict:
					// we still recompute if parent just recomputed, to avoid stale data.
					UpdateWorldRecursive(c);
				}
			}
		}

		void UpdateAllDirty()
		{
			// For determinism: traverse creation order.
			for (uint32_t i = 0; i < orderCount; ++i)
			{
				const uint32_t idx = order[i];
				if (idx >= capacity || alive[idx] == 0) continue;
				if (xform[idx].dirty)
					UpdateWorldRecursive(idx);
			}
		}

		void RebuildCamera(uint32_t viewportW, uint32_t viewportH)
		{
			if (viewportW == 0 || viewportH == 0)
				return;

			camera.aspect = (float)viewportW / (float)viewportH;

			Vec3 eye = Vec3(0, 0, -5);
			Quat rot = Quat::Identity();

			if (cameraFollowIndex != kInvalidIndex && cameraFollowIndex < capacity && alive[cameraFollowIndex])
			{
				const TransformData& tf = xform[cameraFollowIndex];
				eye = tf.localT; // camera object local position is used; world already handled by tf.world if parented
				// Prefer world position if parented:
				eye = Vec3(tf.world.m[12], tf.world.m[13], tf.world.m[14]);
				rot = tf.localR;
			}

			const Vec3 forward = Rotate(rot, Vec3(0, 0, 1)); // LH forward
			const Vec3 up = Rotate(rot, Vec3(0, 1, 0));

			camera.Rebuild(eye, forward, up);
			frustum = FrustumFromViewProj(camera.viewProj);
		}
	};


	bool World::Init(IAllocator& persistentAlloc)
	{
		if (impl_) return true;

		impl_ = (Impl*)persistentAlloc.Allocate(sizeof(Impl), 64);
		if (!impl_) return false;
		memset(impl_, 0, sizeof(Impl));
		impl_->alloc = &persistentAlloc;

		impl_->camera.proj = PerspectiveFovLH(impl_->camera.fovYRadians, impl_->camera.aspect, impl_->camera.nearZ, impl_->camera.farZ);

		NOC_LOG_INFO("World", "World initialized");
		return true;
	}

	void World::Shutdown()
	{
		if (!impl_) return;

		IAllocator* a = impl_->alloc;

		if (impl_->generations) a->Deallocate(impl_->generations);
		if (impl_->alive) a->Deallocate(impl_->alive);
		if (impl_->xform) a->Deallocate(impl_->xform);
		if (impl_->rend) a->Deallocate(impl_->rend);
		if (impl_->order) a->Deallocate(impl_->order);
		if (impl_->freeList) a->Deallocate(impl_->freeList);

		a->Deallocate(impl_);
		impl_ = nullptr;

		NOC_LOG_INFO("World", "World shutdown");
	}

	void World::Update()
	{
		if (!impl_) return;
		impl_->UpdateAllDirty();
	}

	SceneObjectHandle World::CreateObject()
	{
		if (!impl_) return {};

		uint32_t idx = kInvalidIndex;

		if (impl_->freeCount > 0)
		{
			idx = impl_->freeList[--impl_->freeCount];
		}
		else
		{
			idx = impl_->capacity;
			if (!impl_->EnsureCapacity(idx + 1))
				return {};
		}

		impl_->alive[idx] = 1;
		impl_->countAlive++;

		impl_->xform[idx] = TransformData{};
		impl_->rend[idx] = RenderableData{};

		impl_->PushOrder(idx);

		SceneObjectHandle h;
		h.index = idx;
		h.generation = impl_->generations[idx];
		return h;
	}

	void World::DestroyObject(SceneObjectHandle h)
	{
		if (!impl_ || !impl_->IsAliveHandle(h)) return;

		const uint32_t idx = h.index;

		// detach children (promote to roots)
		uint32_t child = impl_->xform[idx].firstChild;
		while (child != kInvalidIndex)
		{
			uint32_t next = impl_->xform[child].nextSibling;
			impl_->xform[child].parent = kInvalidIndex;
			impl_->xform[child].nextSibling = kInvalidIndex;
			child = next;
		}
		impl_->xform[idx].firstChild = kInvalidIndex;

		// detach from parent
		impl_->DetachFromParent(idx);

		// invalidate camera follow if needed
		if (impl_->cameraFollowIndex == idx)
			impl_->cameraFollowIndex = kInvalidIndex;

		impl_->alive[idx] = 0;
		impl_->countAlive--;

		impl_->generations[idx]++; // bump generation
		impl_->PushFree(idx);
	}

	uint32_t World::AliveCount() const
	{
		return impl_ ? impl_->countAlive : 0;
	}

	void World::SetLocalTRS(SceneObjectHandle h, const Vec3& t, const Quat& r, const Vec3& s)
	{
		if (!impl_ || !impl_->IsAliveHandle(h)) return;

		TransformData& tf = impl_->xform[h.index];
		tf.localT = t;
		tf.localR = r;
		tf.localS = s;

		impl_->MarkDirtySubtree(h.index);
	}

	void World::SetParent(SceneObjectHandle child, SceneObjectHandle parent)
	{
		if (!impl_) return;
		if (!impl_->IsAliveHandle(child)) return;

		const uint32_t c = child.index;
		const uint32_t p = (impl_->IsAliveHandle(parent)) ? parent.index : kInvalidIndex;

		if (impl_->xform[c].parent == p)
			return;

		impl_->DetachFromParent(c);
		if (p != kInvalidIndex)
			impl_->AttachToParent(c, p);

		impl_->MarkDirtySubtree(c);
	}

	Mat4 World::GetWorldMatrix(SceneObjectHandle h)
	{
		if (!impl_ || !impl_->IsAliveHandle(h)) return Mat4::Identity();

		if (impl_->xform[h.index].dirty)
			impl_->UpdateWorldRecursive(h.index);

		return impl_->xform[h.index].world;
	}

	void World::SetRenderable(SceneObjectHandle h, ResourceHandle mesh, const AABB& localBounds)
	{
		if (!impl_ || !impl_->IsAliveHandle(h)) return;

		RenderableData& rd = impl_->rend[h.index];
		rd.has = 1;
		rd.mesh = mesh;
		rd.localBounds = localBounds;

		// force bounds update
		impl_->MarkDirtySubtree(h.index);
	}

	void World::SetCameraParams(float fovYRadians, float aspect, float nearZ, float farZ)
	{
		if (!impl_) return;
		impl_->camera.fovYRadians = fovYRadians;
		impl_->camera.aspect = aspect;
		impl_->camera.nearZ = nearZ;
		impl_->camera.farZ = farZ;
	}

	void World::SetCameraFromObject(SceneObjectHandle h)
	{
		if (!impl_ || !impl_->IsAliveHandle(h)) return;
		impl_->cameraFollowIndex = h.index;
	}

	void World::SetCullingMaxDistance(float meters)
	{
		cullingMaxDistance_ = (meters < 0.0f) ? 0.0f : meters;
	}


	RenderQueue World::BuildRenderQueue(LinearArena& frameArena, uint32_t viewportW, uint32_t viewportH)
	{
		RenderQueue q{};

		if (!impl_)
			return q;

		// Ensure transforms/bounds are up to date.
		impl_->UpdateAllDirty();
		impl_->RebuildCamera(viewportW, viewportH);

		q.view.viewProj = impl_->camera.viewProj;
		q.view.viewportWidth = viewportW;
		q.view.viewportHeight = viewportH;

		// Count visible instances first (deterministic order).
		uint32_t visible = 0;
		uint32_t total = 0;
		for (uint32_t i = 0; i < impl_->orderCount; ++i)
		{
			const uint32_t idx = impl_->order[i];
			if (idx >= impl_->capacity || impl_->alive[idx] == 0) continue;
			if (!impl_->rend[idx].has) continue;
			total++;
			if (!cullingEnabled_)
			{
				visible++;
				continue;
			}
			if (debugCullDump_)
			{
				const auto& wb = impl_->rend[idx].worldBounds;

				const bool hit = AabbIntersectsFrustum(wb, impl_->frustum);

				NOC_LOG_INFO("World",
					"CullDump idx=%u hit=%s bounds min(%.2f %.2f %.2f) max(%.2f %.2f %.2f)",
					idx,
					hit ? "YES" : "NO",
					wb.min.x, wb.min.y, wb.min.z,
					wb.max.x, wb.max.y, wb.max.z);
			}



			if (AabbIntersectsFrustum(impl_->rend[idx].worldBounds, impl_->frustum))
				visible++;
		}

		if (visible == 0)
		{
			lastStats_.visible = 0;
			lastStats_.total = total;

			if (debugCullDump_)
			{
				NOC_LOG_INFO("World",
					"CullDump summary: visible=%u total=%u (culling=%s)",
					lastStats_.visible, lastStats_.total, cullingEnabled_ ? "ON" : "OFF");

				debugCullDump_ = false; // one-shot
			}


			return q;
		}

		void* mem = frameArena.Allocate(sizeof(RenderInstance) * visible, 16);



		if (!mem)
			return q;

		RenderInstance* out = (RenderInstance*)mem;

		uint32_t w = 0;
		for (uint32_t i = 0; i < impl_->orderCount; ++i)
		{
			const uint32_t idx = impl_->order[i];
			if (idx >= impl_->capacity || impl_->alive[idx] == 0) continue;
			if (!impl_->rend[idx].has) continue;

			bool keep = true;
			if (cullingEnabled_)
				keep = AabbIntersectsFrustum(impl_->rend[idx].worldBounds, impl_->frustum);

			if (!keep) continue;

			out[w].mesh = impl_->rend[idx].mesh;
			out[w].world = impl_->xform[idx].world;
			w++;
		}

		lastStats_.visible = w;
		lastStats_.total = total;

		if (debugCullDump_)
		{
			NOC_LOG_INFO("World",
				"CullDump summary: visible=%u total=%u (culling=%s)",
				lastStats_.visible, lastStats_.total, cullingEnabled_ ? "ON" : "OFF");

			debugCullDump_ = false; // one-shot
		}


		q.instances = out;
		q.instanceCount = w;
		return q;
	}

	const WorldStats& World::GetLastStats() const
	{
		return lastStats_;
	}

	void World::DebugRequestCullDump()
	{
		debugCullDump_ = true;
	}
}
```

### Listing 138 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Render/RenderSystem.h` (MODIFIED)
```cpp
#pragma once
#include <cstdint>

namespace noc
{
	class ResourceManager;
	struct RenderQueue;

	class RenderSystem
	{
	public:
		RenderSystem() = default;

		bool Init(bool enableDebugLayer);
		void Shutdown();

		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		void SetResourceManager(ResourceManager* rm);

		// Frame lifecycle (unchanged semantics)
		void BeginFrame();
		void EndFramePresent();

		// Runtime → Render handoff for this frame (no ownership taken).
		void SetFrameRenderQueue(const RenderQueue* q);

	private:
		struct Impl;
		Impl* impl_ = nullptr;
	};
}
```

### Listing 139 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Render/RenderSystem.cpp` (MODIFIED)
```cpp
#include "RenderSystem.h"

#include "Core/Log.h"
#include "Render/DX12/Dx12Renderer.h"

namespace noc
{
	struct RenderSystem::Impl
	{
		Dx12Renderer renderer;
	};

	bool RenderSystem::Init(bool enableDebugLayer)
	{
		if (impl_)
			return true;

		impl_ = new Impl();
		if (!impl_->renderer.Init(enableDebugLayer))
		{
			NOC_LOG_ERROR("Render", "Dx12Renderer::Init failed");
			delete impl_;
			impl_ = nullptr;
			return false;
		}

		NOC_LOG_INFO("Render", "RenderSystem initialized");
		return true;
	}

	void RenderSystem::Shutdown()
	{
		if (!impl_)
			return;

		impl_->renderer.Shutdown();

		delete impl_;
		impl_ = nullptr;

		NOC_LOG_INFO("Render", "RenderSystem shutdown");
	}

	bool RenderSystem::AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!impl_)
			return false;

		return impl_->renderer.AttachToWindow(nativeHwnd, clientWidth, clientHeight);
	}

	void RenderSystem::SetResourceManager(ResourceManager* rm)
	{
		if (!impl_) return;
		impl_->renderer.SetResourceManager(rm);
	}

	void RenderSystem::SetFrameRenderQueue(const RenderQueue* q)
	{
		if (!impl_) return;
		impl_->renderer.SetFrameRenderQueue(q);
	}

	void RenderSystem::BeginFrame()
	{
		if (!impl_)
			return;

		impl_->renderer.BeginFrame();
	}

	void RenderSystem::EndFramePresent()
	{
		if (!impl_)
			return;

		impl_->renderer.EndFramePresent();
	}
}
```

### Listing 140 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Render/DX12/Dx12Renderer.h` (MODIFIED)
```cpp
#pragma once
#include "Dx12Common.h"

#include "Dx12Device.h"
#include "Dx12SwapChain.h"
#include "Dx12FrameSync.h"

#include "Dx12DescriptorAllocator.h"
#include "Dx12DeferredReleaseQueue.h"
#include "Dx12PsoCache.h"
#include "MeshPass.h"

namespace noc
{
	class ResourceManager;
	struct RenderQueue;

	class Dx12Renderer
	{
	public:
		bool Init(bool enableDebugLayer);
		void Shutdown();

		bool AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight);

		void SetResourceManager(ResourceManager* rm) { rm_ = rm; }
		void SetFrameRenderQueue(const RenderQueue* q) { frameQueue_ = q; }

		void BeginFrame();
		void EndFramePresent();

	private:
		void LogDeviceRemoved_(const char* where);

	private:
		bool inited_ = false;
		bool attached_ = false;

		ResourceManager* rm_ = nullptr;
		const RenderQueue* frameQueue_ = nullptr; // points to FrameArena memory

		Dx12Device device_;
		Dx12SwapChain swap_;
		Dx12FrameSync sync_;

		dx12::ComPtr<ID3D12CommandAllocator> cmdAlloc_[dx12::kFrameCount];
		dx12::ComPtr<ID3D12GraphicsCommandList> cmdList_;

		uint32_t frameIndex_ = 0;
		bool frameOpen_ = false;

		Dx12DescriptorAllocator cbvSrvUavHeap_;
		Dx12DescriptorAllocator samplerHeap_;
		Dx12DeferredReleaseQueue deferred_;
		Dx12PsoCache psoCache_;

		MeshPass meshPass_;
	};
}
```

### Listing 141 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Render/DX12/Dx12Renderer.cpp` (MODIFIED)
```cpp
#include "Dx12Renderer.h"

#include "Resources/ResourceManager.h"
#include "Render/RenderQueue.h"

namespace noc
{
	bool Dx12Renderer::Init(bool enableDebugLayer)
	{
		if (inited_)
			return true;

		if (!device_.Init(enableDebugLayer))
			return false;

		if (!sync_.Init(device_.Device()))
			return false;

		inited_ = true;
		NOC_LOG_INFO("Render", "Dx12Renderer initialized (awaiting AttachToWindow)");
		return true;
	}

	void Dx12Renderer::Shutdown()
	{
		if (!inited_)
			return;

		if (attached_)
			sync_.WaitForGpu(device_.Queue());

		deferred_.Clear();

		meshPass_.Shutdown(deferred_, sync_.CompletedValue());

		cmdList_.Reset();
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
			cmdAlloc_[i].Reset();

		psoCache_.Clear();

		cbvSrvUavHeap_.Shutdown();
		samplerHeap_.Shutdown();

		swap_.Shutdown();
		sync_.Shutdown();
		device_.Shutdown();

		inited_ = false;
		attached_ = false;
		frameQueue_ = nullptr;

		NOC_LOG_INFO("Render", "Dx12Renderer shutdown");
	}

	bool Dx12Renderer::AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!inited_)
			return false;

		if (!swap_.Init(device_.Factory(), device_.Queue(), nativeHwnd, clientWidth, clientHeight))
			return false;

		if (!swap_.CreateRtvHeapAndViews(device_.Device()))
			return false;

		frameIndex_ = swap_.FrameIndex();

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (!dx12::HrOk(device_.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc_[i])),
				"CreateCommandAllocator"))
			{
				return false;
			}
		}

		if (!dx12::HrOk(device_.Device()->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc_[frameIndex_].Get(), nullptr, IID_PPV_ARGS(&cmdList_)),
			"CreateCommandList"))
		{
			return false;
		}
		dx12::HrOk(cmdList_->Close(), "cmdList->Close (initial)");

		if (!cbvSrvUavHeap_.Init(device_.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true))
			return false;
		if (!samplerHeap_.Init(device_.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 64, true))
			return false;

		if (!meshPass_.Init(device_.Device(), cbvSrvUavHeap_, samplerHeap_, psoCache_))
			return false;

		attached_ = true;
		NOC_LOG_INFO("Render", "Dx12Renderer attached (%ux%u)", clientWidth, clientHeight);
		return true;
	}

	void Dx12Renderer::BeginFrame()
	{
		if (!attached_)
			return;

		if (device_.Device()->GetDeviceRemovedReason() != S_OK)
			return;

		if (frameOpen_)
		{
			NOC_LOG_ERROR("Render", "BeginFrame called while frame is already open");
			return;
		}
		frameOpen_ = true;

		deferred_.Collect(sync_.CompletedValue());

		if (FAILED(cmdAlloc_[frameIndex_]->Reset()))
		{
			LogDeviceRemoved_("cmdAlloc->Reset");
			frameOpen_ = false;
			return;
		}

		if (FAILED(cmdList_->Reset(cmdAlloc_[frameIndex_].Get(), nullptr)))
		{
			LogDeviceRemoved_("cmdList->Reset");
			frameOpen_ = false;
			return;
		}

		swap_.TransitionTo(cmdList_.Get(), frameIndex_, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	void Dx12Renderer::EndFramePresent()
	{
		if (!attached_)
			return;

		if (device_.Device()->GetDeviceRemovedReason() != S_OK)
			return;

		if (!frameOpen_)
		{
			NOC_LOG_ERROR("Render", "EndFramePresent called without BeginFrame");
			return;
		}
		struct Guard { bool& b; ~Guard() { b = false; } } g{ frameOpen_ };

		// Record pass: clear + draw instances from frameQueue_ (if any are ready).
		meshPass_.Record(device_.Device(), cmdList_.Get(), swap_, sync_, frameIndex_, deferred_, rm_, frameQueue_);

		swap_.TransitionTo(cmdList_.Get(), frameIndex_, D3D12_RESOURCE_STATE_PRESENT);

		if (FAILED(cmdList_->Close()))
		{
			LogDeviceRemoved_("cmdList->Close");
			return;
		}

		ID3D12CommandList* lists[] = { cmdList_.Get() };
		device_.Queue()->ExecuteCommandLists(1, lists);

		swap_.Present();

		sync_.MoveToNextFrame(device_.Queue(), swap_.SwapChain(), frameIndex_);
		swap_.UpdateFrameIndex(frameIndex_);
	}

	void Dx12Renderer::LogDeviceRemoved_(const char* where)
	{
		HRESULT hr = device_.Device()->GetDeviceRemovedReason();
		NOC_LOG_ERROR("Render", "Device removed at %s (hr=0x%08X)", where, (unsigned)hr);
	}
}
```

### Listing 142 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Render/DX12/MeshPass.h` (MODIFIED)
```cpp
#pragma once
#include "Dx12Common.h"

#include "Dx12DescriptorAllocator.h"
#include "Dx12DeferredReleaseQueue.h"
#include "Dx12PsoCache.h"
#include "GpuBuffer.h"
#include "GpuRingConstantBuffer.h"
#include "MeshFormat.h"

#include "Resources/ResourceHandle.h"
#include "Resources/Typed/ResourceHandleT.h"
#include "Resources/Typed/TextResource.h"

namespace noc
{
	class ResourceManager;
	class Dx12SwapChain;
	class Dx12FrameSync;
	struct RenderQueue;

	class MeshPass
	{
	public:
		bool Init(
			ID3D12Device* device,
			Dx12DescriptorAllocator& cbvSrvUav,
			Dx12DescriptorAllocator& samplers,
			Dx12PsoCache& psoCache);

		void Shutdown(Dx12DeferredReleaseQueue& deferred, uint64_t safeFenceValue);

		void Record(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmd,
			Dx12SwapChain& swap,
			const Dx12FrameSync& sync,
			uint32_t frameIndex,
			Dx12DeferredReleaseQueue& deferred,
			ResourceManager* rm,
			const RenderQueue* queue);

	private:
		bool EnsureRootSigAndPso_(ID3D12Device* device, Dx12PsoCache& cache, ResourceManager* rm);
		bool EnsureMeshUploaded_(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, Dx12DeferredReleaseQueue& deferred,
			const Dx12FrameSync& sync, uint32_t frameIndex, ResourceManager* rm);

		void EnsurePerFrameCbv_(ID3D12Device* device);
		void EnsurePerFrameInstanceSrv_(ID3D12Device* device);

	private:
		ResourceHandleT<TextResource> shaderHlsl_;

		// GPU objects
		dx12::ComPtr<ID3D12RootSignature> rootSig_;
		dx12::ComPtr<ID3D12PipelineState> pso_;

		// For Phase 10 demo: one mesh upload path (triangle.nmsh), but drawn N times.
		ResourceHandle meshBin_{};
		GpuBuffer vb_;
		GpuBuffer ib_;
		uint32_t indexCount_ = 0;

		// Per-frame constants
		GpuRingConstantBuffer perFrameCB_;
		Dx12DescriptorAllocator* cbvSrvUav_ = nullptr;
		Dx12DescriptorHandle perFrameCbv_[dx12::kFrameCount]{};

		// Per-frame instance matrices in an upload buffer (mapped once), exposed as SRV t0.
		dx12::ComPtr<ID3D12Resource> instanceBuf_[dx12::kFrameCount];
		uint8_t* instanceMapped_[dx12::kFrameCount]{};
		uint32_t instanceCapacity_ = 0;
		Dx12DescriptorHandle instanceSrv_[dx12::kFrameCount]{};

		Dx12PsoCache* psoCache_ = nullptr;

		bool rootReady_ = false;
		bool psoReady_ = false;
		bool meshReady_ = false;
		bool cbReady_ = false;
		bool instReady_ = false;
	};
}
```

### Listing 143 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Render/DX12/MeshPass.cpp` (MODIFIED)
```cpp
#include "MeshPass.h"

#include "Dx12SwapChain.h"
#include "Dx12FrameSync.h"
#include "ShaderCompiler.h"

#include "Resources/ResourceManager.h"
#include "Render/RenderQueue.h"

namespace noc
{
	// Matches Basic.hlsl
	struct PerFrameConstants
	{
		Mat4 viewProj;
	};

	static uint64_t HashInputLayoutPC_()
	{
		return 0xA0C0CA11u; // stable constant for Phase 9.5/10 demo
	}

	bool MeshPass::Init(
		ID3D12Device* device,
		Dx12DescriptorAllocator& cbvSrvUav,
		Dx12DescriptorAllocator& samplers,
		Dx12PsoCache& psoCache)
	{
		(void)samplers;

		if (!device)
			return false;

		cbvSrvUav_ = &cbvSrvUav;
		psoCache_ = &psoCache;

		rootReady_ = false;
		psoReady_ = false;
		meshReady_ = false;
		cbReady_ = false;
		instReady_ = false;

		// Per-frame constant buffers
		if (!perFrameCB_.Init(device, 64 * 1024))
			return false;

		// Instance buffer capacity (Design choice): enough for a small test scene.
		instanceCapacity_ = 1024;

		return true;
	}

	void MeshPass::Shutdown(Dx12DeferredReleaseQueue& deferred, uint64_t safeFenceValue)
	{
		if (pso_)
		{
			dx12::ComPtr<IUnknown> u;
			pso_.As(&u);
			deferred.Enqueue(safeFenceValue, std::move(u));
			pso_.Reset();
		}
		if (rootSig_)
		{
			dx12::ComPtr<IUnknown> u;
			rootSig_.As(&u);
			deferred.Enqueue(safeFenceValue, std::move(u));
			rootSig_.Reset();
		}

		vb_.ShutdownNow();
		ib_.ShutdownNow();

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			if (instanceBuf_[i] && instanceMapped_[i])
				instanceBuf_[i]->Unmap(0, nullptr);
			instanceMapped_[i] = nullptr;
			instanceBuf_[i].Reset();
		}

		perFrameCB_.Shutdown();
	}

	void MeshPass::EnsurePerFrameCbv_(ID3D12Device* device)
	{
		if (cbReady_ || !device || !cbvSrvUav_)
			return;

		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			perFrameCbv_[i] = cbvSrvUav_->Allocate();

			D3D12_CONSTANT_BUFFER_VIEW_DESC d{};
			d.BufferLocation = perFrameCB_.Resource(i)->GetGPUVirtualAddress();
			d.SizeInBytes = (UINT)((sizeof(PerFrameConstants) + 255u) & ~255u);

			device->CreateConstantBufferView(&d, perFrameCbv_[i].cpu);
		}

		cbReady_ = true;
	}

	void MeshPass::EnsurePerFrameInstanceSrv_(ID3D12Device* device)
	{
		if (instReady_ || !device || !cbvSrvUav_)
			return;

		// Upload heap, persistently mapped.
		for (uint32_t i = 0; i < dx12::kFrameCount; ++i)
		{
			instanceSrv_[i] = cbvSrvUav_->Allocate();

			const UINT64 bytes = (UINT64)instanceCapacity_ * sizeof(Mat4);

			D3D12_HEAP_PROPERTIES hp{};
			hp.Type = D3D12_HEAP_TYPE_UPLOAD;

			D3D12_RESOURCE_DESC d = dx12::BufferDesc(bytes);

			if (!dx12::HrOk(device->CreateCommittedResource(
				&hp,
				D3D12_HEAP_FLAG_NONE,
				&d,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&instanceBuf_[i])), "CreateCommittedResource(InstanceUpload)"))
			{
				return;
			}

			void* mapped = nullptr;
			D3D12_RANGE r{ 0, 0 };
			if (!dx12::HrOk(instanceBuf_[i]->Map(0, &r, &mapped), "InstanceUpload.Map"))
				return;

			instanceMapped_[i] = (uint8_t*)mapped;

			D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
			sd.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			sd.Buffer.FirstElement = 0;
			sd.Buffer.NumElements = instanceCapacity_;
			sd.Buffer.StructureByteStride = sizeof(Mat4);
			sd.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			sd.Format = DXGI_FORMAT_UNKNOWN;

			device->CreateShaderResourceView(instanceBuf_[i].Get(), &sd, instanceSrv_[i].cpu);
		}

		instReady_ = true;
	}

	bool MeshPass::EnsureRootSigAndPso_(ID3D12Device* device, Dx12PsoCache& cache, ResourceManager* rm)
	{
		if (!device || !rm)
			return false;

		if (!shaderHlsl_.IsValid())
			shaderHlsl_ = rm->RequestText("Shaders/Basic.hlsl");

		if (!rootReady_)
		{
			// Root parameters:
			// 0: CBV table (b0) per-frame
			// 1: SRV table (t0..t7) (t0 used for instance matrices)
			// 2: Sampler table (s0..s7) (reserved)
			D3D12_DESCRIPTOR_RANGE ranges[3]{};

			ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			ranges[0].NumDescriptors = 1;
			ranges[0].BaseShaderRegister = 0;
			ranges[0].RegisterSpace = 0;
			ranges[0].OffsetInDescriptorsFromTableStart = 0;

			ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			ranges[1].NumDescriptors = 8;
			ranges[1].BaseShaderRegister = 0;
			ranges[1].RegisterSpace = 0;
			ranges[1].OffsetInDescriptorsFromTableStart = 0;

			ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			ranges[2].NumDescriptors = 8;
			ranges[2].BaseShaderRegister = 0;
			ranges[2].RegisterSpace = 0;
			ranges[2].OffsetInDescriptorsFromTableStart = 0;

			D3D12_ROOT_PARAMETER params[3]{};

			params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[0].DescriptorTable.NumDescriptorRanges = 1;
			params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
			params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[1].DescriptorTable.NumDescriptorRanges = 1;
			params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
			params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[2].DescriptorTable.NumDescriptorRanges = 1;
			params[2].DescriptorTable.pDescriptorRanges = &ranges[2];
			params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_ROOT_SIGNATURE_DESC rs{};
			rs.NumParameters = 3;
			rs.pParameters = params;
			rs.NumStaticSamplers = 0;
			rs.pStaticSamplers = nullptr;
			rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			dx12::ComPtr<ID3DBlob> blob;
			dx12::ComPtr<ID3DBlob> err;
			if (!dx12::HrOk(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err), "SerializeRootSignature"))
			{
				const char* e = err ? (const char*)err->GetBufferPointer() : "unknown";
				NOC_LOG_ERROR("Render", "RootSig serialize error: %s", e);
				return false;
			}

			if (!dx12::HrOk(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig_)), "CreateRootSignature"))
				return false;

			rootReady_ = true;
		}

		const TextResource* src = rm->GetText(shaderHlsl_);
		if (!src)
			return false;

		dx12::ComPtr<ID3DBlob> vs;
		dx12::ComPtr<ID3DBlob> ps;

		if (!ShaderCompiler::CompileFromMemory("Shaders/Basic.hlsl", src->Str().c_str(), src->Str().size(), "VSMain", "vs_5_1", vs))
			return false;
		if (!ShaderCompiler::CompileFromMemory("Shaders/Basic.hlsl", src->Str().c_str(), src->Str().size(), "PSMain", "ps_5_1", ps))
			return false;

		Dx12PsoKey key{};
		key.vs = vs.Get();
		key.ps = ps.Get();
		key.rootSig = rootSig_.Get();
		key.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		key.inputLayoutHash = HashInputLayoutPC_();

		if (auto* cached = cache.Find(key))
		{
			pso_ = cached;
			psoReady_ = true;
			return true;
		}

		D3D12_INPUT_ELEMENT_DESC layout[2]{};
		layout[0].SemanticName = "POSITION";
		layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		layout[0].InputSlot = 0;
		layout[0].AlignedByteOffset = 0;
		layout[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

		layout[1].SemanticName = "COLOR";
		layout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		layout[1].InputSlot = 0;
		layout[1].AlignedByteOffset = 12;
		layout[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = rootSig_.Get();
		pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pso.DepthStencilState.DepthEnable = FALSE;
		pso.DepthStencilState.StencilEnable = FALSE;
		pso.SampleMask = UINT_MAX;
		pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso.NumRenderTargets = 1;
		pso.RTVFormats[0] = key.rtvFormat;
		pso.SampleDesc.Count = 1;
		pso.InputLayout = { layout, 2 };

		dx12::ComPtr<ID3D12PipelineState> created;
		if (!dx12::HrOk(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&created)), "CreateGraphicsPipelineState"))
			return false;

		pso_ = created;
		cache.Insert(key, std::move(created));
		psoReady_ = true;
		return true;
	}

	bool MeshPass::EnsureMeshUploaded_(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12DeferredReleaseQueue& deferred,
		const Dx12FrameSync& sync,
		uint32_t frameIndex,
		ResourceManager* rm)
	{
		if (meshReady_)
			return true;

		if (!meshBin_.IsValid())
			meshBin_ = rm->RequestBinary("Meshes/triangle.nmsh");

		if (!rm->IsReady(meshBin_))
			return false;

		const uint8_t* bytes = rm->GetBytes(meshBin_);
		const size_t size = rm->GetSize(meshBin_);
		if (!bytes || size == 0)
			return false;

		CpuMeshPC cpu{};
		const char* err = nullptr;
		if (!ParseNocMeshPC(bytes, size, cpu, err))
		{
			NOC_LOG_ERROR("Render", "Mesh parse failed: %s", err ? err : "unknown");
			return false;
		}

		indexCount_ = (uint32_t)cpu.indices.size();

		if (!vb_.CreateStatic(device, cmd, deferred, sync, frameIndex, GpuBuffer::Kind::Vertex,
			cpu.vertices.data(), cpu.vertices.size() * sizeof(MeshVertexPC), sizeof(MeshVertexPC)))
			return false;

		if (!ib_.CreateStatic(device, cmd, deferred, sync, frameIndex, GpuBuffer::Kind::Index,
			cpu.indices.data(), cpu.indices.size() * sizeof(uint16_t), 0))
			return false;

		meshReady_ = true;
		return true;
	}

	void MeshPass::Record(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		Dx12SwapChain& swap,
		const Dx12FrameSync& sync,
		uint32_t frameIndex,
		Dx12DeferredReleaseQueue& deferred,
		ResourceManager* rm,
		const RenderQueue* queue)
	{
		if (!device || !cmd)
			return;

		EnsurePerFrameCbv_(device);
		EnsurePerFrameInstanceSrv_(device);

		auto rtv = swap.CurrentRtv(frameIndex);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		const float clearColor[4] = { 0.05f, 0.05f, 0.08f, 1.0f };
		cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

		if (!rm || !queue || queue->instanceCount == 0)
			return;

		if (!EnsureRootSigAndPso_(device, *psoCache_, rm))
			return;

		if (!EnsureMeshUploaded_(device, cmd, deferred, sync, frameIndex, rm))
			return;

		// Per-frame constants: viewProj
		perFrameCB_.BeginFrame(frameIndex);
		D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
		void* cpu = nullptr;
		if (perFrameCB_.Allocate(sizeof(PerFrameConstants), gpu, cpu))
		{
			auto* c = (PerFrameConstants*)cpu;
			c->viewProj = queue->view.viewProj;
		}

		// Upload instance matrices for this frame (clamp to capacity).
		const uint32_t count = (queue->instanceCount > instanceCapacity_) ? instanceCapacity_ : queue->instanceCount;

		// Correct: write matrices one-by-one because RenderInstance contains a mesh handle before the matrix.
		Mat4* dst = reinterpret_cast<Mat4*>(instanceMapped_[frameIndex]);
		for (uint32_t i = 0; i < count; ++i)
		{
			dst[i] = queue->instances[i].world;
		}


		// Bind descriptor heaps (shader-visible).
		ID3D12DescriptorHeap* heaps[] = { cbvSrvUav_->Heap() };
		cmd->SetDescriptorHeaps(1, heaps);

		cmd->SetGraphicsRootSignature(rootSig_.Get());
		cmd->SetPipelineState(pso_.Get());

		// Root slot 0: per-frame CBV table (b0)
		cmd->SetGraphicsRootDescriptorTable(0, perFrameCbv_[frameIndex].gpu);

		// Root slot 1: SRV table (t0..), we use t0 = instance matrices
		cmd->SetGraphicsRootDescriptorTable(1, instanceSrv_[frameIndex].gpu);

		D3D12_VIEWPORT vp{};
		vp.Width = (float)swap.Width();
		vp.Height = (float)swap.Height();
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;

		D3D12_RECT sc{};
		sc.left = 0;
		sc.top = 0;
		sc.right = (LONG)swap.Width();
		sc.bottom = (LONG)swap.Height();

		cmd->RSSetViewports(1, &vp);
		cmd->RSSetScissorRects(1, &sc);

		auto vbv = vb_.VertexView();
		auto ibv = ib_.IndexView(DXGI_FORMAT_R16_UINT);

		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 1, &vbv);
		cmd->IASetIndexBuffer(&ibv);

		cmd->DrawIndexedInstanced(indexCount_, count, 0, 0, 0);
	}
}
```

### Listing 144 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/Engine.h` (MODIFIED: owns World)
```cpp
#pragma once
#include "Core/BuildConfig.h"
#include "Core/Subsystems/SubsystemRegistry.h"
#include "Core/Memory/Allocator.h"
#include "Core/Memory/LinearArena.h"

#if NOC_ENABLE_ASSERTS
#include "Core/Memory/DebugAlloc.h"
#endif

#include "Resources/VirtualFileSystem.h"
#include "Resources/ResourceManager.h"
#include "EngineConfig.h"
#include "Core/Jobs/JobSystem.h"
#include "Input/InputSystem.h"
#include "Render/RenderSystem.h"

#include "Runtime/World.h"
#include <Platform/Win32/WinWindow.h>

namespace noc {

	class WinWindow;
	class MainLoop;

	class Engine
	{
	public:
		EngineConfig& ConfigMutable();
		const EngineConfig& Config() const { return cfg_; }

		bool SetContentRoot(const char* path);
		bool SetOverrideRoot(const char* path);
		bool SetArchivePath(const char* path);

		bool Init();
		void TickOnce();
		void Shutdown();

		int Run();
		void BeginFrame();
		void Tick();
		void EndFrame();

		IAllocator& Allocator();
		LinearArena& FrameArena();

		VirtualFileSystem& VFS() { return vfs_; }
		ResourceManager& Resources() { return resources_; }
		const ResourceManager& Resources() const { return resources_; }

		InputSystem& Input() { return input_; }
		const InputSystem& Input() const { return input_; }

		JobSystem& Jobs() { return jobs_; }
		const JobSystem& Jobs() const { return jobs_; }

		World& GetWorld() { return world_; }
		const World& GetWorld() const { return world_; }

		bool InitMemory();
		void KillMemory();

		bool AttachWindow(WinWindow& window);
		bool CreateAndAttachMainWindow(WinWindowDesc desc, WinWindow& outWindow);


	private:
		bool IsConfigMutable() const { return !initialized_; }

	private:
		SubsystemRegistry registry_;

		MallocAllocator baseAlloc_;

#if NOC_ENABLE_ASSERTS
		DebugAlloc debugAlloc_{ baseAlloc_ };
		IAllocator* alloc_ = &debugAlloc_;
#else
		IAllocator* alloc_ = &baseAlloc_;
#endif

		void* frameArenaMem_ = nullptr;
		LinearArena frameArena_;

		VirtualFileSystem vfs_;
		JobSystem jobs_;
		ResourceManager resources_;
		InputSystem input_;

		RenderSystem render_;
		World world_;

		EngineConfig cfg_{};
		bool initialized_ = false;
	};

} // namespace noc
```

### Listing 145 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Engine/Runtime/Engine.cpp` (MODIFIED: tick world + render queue)
```cpp
#include "Engine.h"

#include <algorithm>
#include <span>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Clock.h"

#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h"

#include "Render/RenderQueue.h"

namespace noc {

    IAllocator& Engine::Allocator() { return *alloc_; }
    LinearArena& Engine::FrameArena() { return frameArena_; }

    EngineConfig& Engine::ConfigMutable()
    {
        if (initialized_)
        {
            NOC_LOG_ERROR("Runtime", "EngineConfig is frozen after Init(). Modify config before calling Init().");
            return cfg_;
        }
        return cfg_;
    }

    bool Engine::SetContentRoot(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_ERROR("Runtime", "SetContentRoot() called after Init(); ignored.");
            return false;
        }
        cfg_.contentRoot = path;
        return true;
    }

    bool Engine::SetOverrideRoot(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_ERROR("Runtime", "SetOverrideRoot() called after Init(); ignored.");
            return false;
        }
        cfg_.overrideRoot = path;
        return true;
    }

    bool Engine::SetArchivePath(const char* path)
    {
        if (!IsConfigMutable())
        {
            NOC_LOG_ERROR("Runtime", "SetArchivePath() called after Init(); ignored.");
            return false;
        }
        cfg_.archivePath = path;
        return true;
    }

    bool Engine::InitMemory()
    {
        constexpr std::size_t kFrameArenaBytes = 8 * 1024 * 1024; // Design choice
        frameArenaMem_ = Allocator().Allocate(kFrameArenaBytes, 64);
        frameArena_.Init(frameArenaMem_, kFrameArenaBytes);

        NOC_LOG_INFO("Core", "Memory system initialized (frame arena=%zu bytes)", kFrameArenaBytes);
        return true;
    }

    void Engine::KillMemory()
    {
        if (frameArenaMem_)
        {
            Allocator().Deallocate(frameArenaMem_);
            frameArenaMem_ = nullptr;
        }

#if NOC_ENABLE_ASSERTS
        NOC_LOG_INFO("Core", "Memory stats: total=%zu outstanding=%zu allocs=%zu",
            debugAlloc_.TotalAllocatedBytes(),
            debugAlloc_.OutstandingBytes(),
            debugAlloc_.AllocationCount());
#endif
    }

    static bool StartupLog(void*)
    {
        noc::GetLogger().Init();
        NOC_LOG_INFO("Core", "Logger initialized");
        return true;
    }

    static void ShutdownLog(void*)
    {
        NOC_LOG_INFO("Core", "Logger shutting down");
        noc::GetLogger().Shutdown();
    }

    static bool StartupTime(void*)
    {
        noc::GetTime().Init();
        NOC_LOG_INFO("Core", "Time system initialized");
        return true;
    }

    static void ShutdownTime(void*)
    {
        NOC_LOG_INFO("Core", "Time system shutting down");
    }

    static bool StartupAssert(void*)
    {
        NOC_LOG_INFO("Core", "Assert system initialized");
        return true;
    }

    static void ShutdownAssert(void*)
    {
        NOC_LOG_INFO("Core", "Assert system shutting down");
    }

    static bool StartupMemory(void* ctx)
    {
        auto* e = static_cast<noc::Engine*>(ctx);
        return e->InitMemory();
    }

    static void ShutdownMemory(void* ctx)
    {
        NOC_LOG_INFO("Core", "Memory system shutting down");
        auto* e = static_cast<noc::Engine*>(ctx);
        e->KillMemory();
    }

    static bool StartupWindow(void* ctx)
    {
        auto* e = static_cast<Engine*>(ctx);
        (void)e;
        NOC_LOG_INFO("Win32", "Window subsystem ready");
        return true;
    }

    static void ShutdownWindow(void*)
    {
        NOC_LOG_INFO("Win32", "Window subsystem shutdown");
    }

    static bool StartupJobs(void* ctx)
    {
        auto* e = static_cast<noc::Engine*>(ctx);
        // Design choice: worker count = HW threads - 1 (leave room for main thread), clamped.
        const uint32_t hw = (std::max)(1u, std::thread::hardware_concurrency());
        const uint32_t workers = (hw > 1) ? (hw - 1) : 1;
        return e->Jobs().Init(workers);
    }

    static void ShutdownJobs(void* ctx)
    {
        auto* e = static_cast<noc::Engine*>(ctx);
        e->Jobs().Shutdown();
    }

    bool Engine::Init()
    {
        std::span<const char* const> depsLog{};

        static const char* kDepsNeedLog[] = { "Log" };
        std::span<const char* const> depsNeedLog{ kDepsNeedLog, 1 };

        static const char* kDepsJobs[] = { "Log", "Memory" };
        std::span<const char* const> depsJobs{ kDepsJobs, 2 };

        registry_.Register(SubsystemDesc{ "Log",    depsLog,     &StartupLog,    &ShutdownLog });
        registry_.Register(SubsystemDesc{ "Time",   depsNeedLog, &StartupTime,   &ShutdownTime });
        registry_.Register(SubsystemDesc{ "Memory", depsNeedLog, &StartupMemory, &ShutdownMemory });
        registry_.Register(SubsystemDesc{ "Assert", depsNeedLog, &StartupAssert, &ShutdownAssert });
        registry_.Register(SubsystemDesc{ "Window", depsNeedLog, &StartupWindow, &ShutdownWindow });
        registry_.Register(SubsystemDesc{ "Jobs",   depsJobs,    &StartupJobs,   &ShutdownJobs });

        if (!registry_.StartupAll(this))
            return false;

        // Phase 3 policy: later mounts override earlier mounts.
        if (cfg_.archivePath && cfg_.archivePath[0] != 0)
        {
            if (!vfs_.MountArchive(cfg_.archivePath))
                NOC_LOG_WARN("VFS", "Failed to mount archivePath: %s", cfg_.archivePath);
        }

        if (cfg_.contentRoot && cfg_.contentRoot[0] != 0)
        {
            if (!vfs_.MountLooseDirectory(cfg_.contentRoot))
                NOC_LOG_WARN("VFS", "Failed to mount contentRoot: %s", cfg_.contentRoot);
        }

        if (cfg_.overrideRoot && cfg_.overrideRoot[0] != 0)
        {
            if (!vfs_.MountLooseDirectory(cfg_.overrideRoot))
                NOC_LOG_WARN("VFS", "Failed to mount overrideRoot: %s", cfg_.overrideRoot);
        }

        // Phase 6: ResourceManager uses JobSystem (must be after jobs + VFS mounts).
        if (!resources_.Init(*this, vfs_))
            return false;

		// Phase 7: InputSystem init (HWND comes later in AttachWindow).
		if (!input_.Init(*this))
			return false;

        // Phase 8: RenderSystem init (no HWND yet)
#if NOC_ENABLE_ASSERTS
		const bool enableDebugLayer = true;
#else
		const bool enableDebugLayer = false;
#endif

		if (!render_.Init(enableDebugLayer))
			return false;

        render_.SetResourceManager(&resources_);

        // World is runtime-owned
        if (!world_.Init(Allocator()))
            return false;

        initialized_ = true;
        return true;
    }

    bool Engine::AttachWindow(WinWindow& window)
    {
        // Forward WM_INPUT + focus loss -> InputSystem.
        window.SetRawInputSink(input_.RawSink());

        // Register Raw Input devices (requires HWND).
        if (!input_.AttachToWindow(window.Handle()))
            return false;

        // Phase 8: DX12 attach (requires HWND + client size).
        if (!render_.AttachToWindow(window.Handle(), window.ClientWidth(), window.ClientHeight()))
            return false;

        return true;
    }

    bool Engine::CreateAndAttachMainWindow(WinWindowDesc desc, WinWindow& outWindow)
    {
        if (!outWindow.Create(desc))
            return false;

        if (!AttachWindow(outWindow))
            return false;

        return true;
    }


    int Engine::Run()
    {
        WinWindow window;
        WinWindowDesc wd{};
        wd.title = L"NocturneHost";
        wd.width = 1280;
        wd.height = 720;
        wd.resizable = true;

        if (!window.Create(wd))
        {
            NOC_LOG_FATAL("Win32", "Failed to create window");
            return -1;
        }

        if (!AttachWindow(window))
        {
            NOC_LOG_FATAL("Runtime", "Failed to attach InputSystem to window");
            window.Destroy();
            return -1;
		}

        MainLoop loop;
        loop.Run(*this, window);

        window.Destroy();
        return 0;
    }

    void Engine::BeginFrame()
    {
        GetTime().BeginFrame();
        FrameArena().Reset();
		input_.BeginFrame();
		render_.BeginFrame();
    }

    void Engine::Tick()
    {
		input_.Update();
        resources_.Update();
        world_.Update();
    }

    void Engine::EndFrame()
    {
        // Phase 10: runtime builds render submission in FrameArena and hands it to Render.
        // This keeps Render from touching Runtime state.
        // Viewport size is owned by the swapchain; in this phase we mirror host window size.
        // (Design choice) If you already expose swapchain size, wire it here instead.
        const uint32_t viewportW = 1280;
        const uint32_t viewportH = 720;

        RenderQueue rq = world_.BuildRenderQueue(FrameArena(), viewportW, viewportH);
        render_.SetFrameRenderQueue(&rq);
        render_.EndFramePresent();
        GetTime().EndFrame();
    }

    void Engine::TickOnce()
    {
        GetTime().BeginFrame();
        FrameArena().Reset();

        void* a = FrameArena().Allocate(256, 16);
        void* b = FrameArena().Allocate(1024, 64);
        (void)a; (void)b;

        GetTime().EndFrame();

        NOC_LOG_INFO("Core", "TickOnce() dt=%.6f sec arenaUsed=%zu bytes",
            GetTime().DeltaSeconds(),
            FrameArena().Used());
    }

    void Engine::Shutdown()
    {
		render_.Shutdown();
        world_.Shutdown();
        // Resource manager must shutdown while jobs + memory + log still exist.
        resources_.Shutdown();
		input_.Shutdown();
        registry_.ShutdownAll(this);
    }

} // namespace noc
```

### Listing 146 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Apps/NocturneHost/main.cpp` (MODIFIED: test scene)
```cpp
#include "Runtime/Engine.h"
#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h" // not used anymore, but ok if included elsewhere
#include "Core/Log.h"
#include "Core/Clock.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>

#ifndef NOC_CONTENT_ROOT
#define NOC_CONTENT_ROOT "."
#endif

// -------------------------------
// Phase 10 Host-side test state
// -------------------------------
static noc::SceneObjectHandle gParent;
static noc::SceneObjectHandle gCam;
static bool gCulling = true;

static noc::SceneObjectHandle gCullingProbe;
static float gProbeX = 0.0f;


static void BuildPhase10TestScene(noc::Engine& engine)
{
	auto& w = engine.GetWorld();

	// Camera object
	gCam = w.CreateObject();
	w.SetLocalTRS(gCam, noc::Vec3(0, 0, -5), noc::Quat::Identity(), noc::Vec3::One());
	w.SetCameraFromObject(gCam);

	// IMPORTANT: keep farZ large so GPU can still draw objects we "uncull"
	w.SetCameraParams(1.04719755f, 16.0f / 9.0f, 0.1f, 200.0f);

	// Triangle mesh handle
	noc::ResourceHandle tri = engine.Resources().RequestBinary("Meshes/triangle.nmsh");

	// Conservative bounds
	noc::AABB triBounds{ noc::Vec3(-1, -1, -1), noc::Vec3(1, 1, 1) };

	// Parent
	gParent = w.CreateObject();
	w.SetLocalTRS(gParent, noc::Vec3(-3.0f, 0.0f, 5.0f), noc::Quat::Identity(), noc::Vec3::One());
	//w.SetRenderable(gParent, tri, triBounds);

	gCullingProbe = w.CreateObject();
	w.SetLocalTRS(gCullingProbe, noc::Vec3(0.0f, 0.0f, 20.0f), noc::Quat::Identity(), noc::Vec3::One());
	w.SetRenderable(gCullingProbe, tri, triBounds);


	// Children follow parent (visible baseline)
	for (int i = 0; i < 5; ++i)
	{
		noc::SceneObjectHandle child = w.CreateObject();
		w.SetParent(child, gParent);
		w.SetLocalTRS(child, noc::Vec3((float)i * 1.5f, 0.0f, 0.0f), noc::Quat::Identity(), noc::Vec3::One());
		w.SetRenderable(child, tri, triBounds);
	}

	// ---- CULLING DEMO OBJECTS ----
	// These are inside far plane (z=30) so GPU can draw them.
	// They are far on X so the frustum *should* reject them when culling is enabled.

	// Off-right (should be culled when ON, visible when OFF)
	{
		noc::SceneObjectHandle offRight = w.CreateObject();
		w.SetLocalTRS(offRight, noc::Vec3(60.0f, 0.0f, 30.0f), noc::Quat::Identity(), noc::Vec3::One());
		w.SetRenderable(offRight, tri, triBounds);
	}

	// Off-left (should be culled when ON, visible when OFF)
	{
		noc::SceneObjectHandle offLeft = w.CreateObject();
		w.SetLocalTRS(offLeft, noc::Vec3(-60.0f, 0.0f, 30.0f), noc::Quat::Identity(), noc::Vec3::One());
		w.SetRenderable(offLeft, tri, triBounds);
	}

	// Slightly above (tests top plane)
	{
		noc::SceneObjectHandle offUp = w.CreateObject();
		w.SetLocalTRS(offUp, noc::Vec3(0.0f, 40.0f, 30.0f), noc::Quat::Identity(), noc::Vec3::One());
		w.SetRenderable(offUp, tri, triBounds);
	}

	gCulling = true;
	w.SetCullingEnabled(gCulling);

	NOC_LOG_INFO("Host",
		"Phase 10 test scene built. Expected: culling ON shows only the hierarchy; culling OFF increases draw count.");
}



static void UpdatePhase10TestScene(noc::Engine& engine)
{
	// Animate parent so hierarchy is obvious
	static float t = 0.0f;
	t += (float)noc::GetTime().DeltaSeconds();

	const float x = std::sinf(t) * 3.0f;
	engine.GetWorld().SetLocalTRS(gParent, noc::Vec3(x, 0, 5), noc::Quat::Identity(), noc::Vec3::One());

	gProbeX += (float)noc::GetTime().DeltaSeconds() * 20.0f; // move right
	if (gProbeX > 80.0f) gProbeX = -80.0f;
	engine.GetWorld().SetLocalTRS(gCullingProbe, noc::Vec3(gProbeX, 0.0f, 20.0f), noc::Quat::Identity(), noc::Vec3::One());

	// Toggle culling with C key (host-side debug input)
	static bool prevC = false;
	const bool nowC = (GetAsyncKeyState('C') & 0x8000) != 0;
	if (nowC && !prevC)
	{
		gCulling = !gCulling;
		engine.GetWorld().SetCullingEnabled(gCulling);

		// request one-shot debug dump next frame
		engine.GetWorld().DebugRequestCullDump();

		NOC_LOG_INFO("Host", "Culling toggled: %s", gCulling ? "ON" : "OFF");
	}

	prevC = nowC;

	// Exit with ESC (host-side)
	if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0)
	{
		PostQuitMessage(0);
	}
}

int main()
{
	noc::Engine engine;
	engine.SetContentRoot(NOC_CONTENT_ROOT);

	if (!engine.Init())
		return -1;

	noc::WinWindow window;
	noc::WinWindowDesc wd{};
	wd.title = L"NocturneHost - Phase 10 Test";
	wd.width = 1280;
	wd.height = 720;
	wd.resizable = true;

	if (!engine.CreateAndAttachMainWindow(wd, window))
	{
		engine.Shutdown();
		return -1;
	}

	BuildPhase10TestScene(engine);
	NOC_LOG_INFO("Host", "Controls: C = toggle culling, ESC = quit");

	// message pump
	MSG msg{};
	bool running = true;
	while (running)
	{
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT) { running = false; break; }
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		if (!running) break;

		engine.BeginFrame();
		UpdatePhase10TestScene(engine);
		engine.Tick();
		engine.EndFrame();
		static uint32_t sFrame = 0;
		if ((sFrame++ % 30) == 0)
		{
			const auto& st = engine.GetWorld().GetLastStats();
			NOC_LOG_INFO("Host", "Visible: %u / Total: %u (Culling: %s)",
				st.visible, st.total, gCulling ? "ON" : "OFF");
		}


	}

	window.Destroy();
	engine.Shutdown();
	return 0;
}
```

### Listing 147 — `Phase 10 — Scene Representation.md` — FULL C++ IMPLEMENTATIONS (every new/modified file) > `Data/Shaders/Basic.hlsl` (MODIFIED: instancing + viewProj + matrix buffer)
```hlsl
struct VSIn
{
    float3 pos : POSITION;
    float4 color : COLOR;
};

struct VSOut
{
    float4 pos : SV_Position;
    float4 color : COLOR;
};

// b0: per-frame
cbuffer PerFrame : register(b0)
{
    float4x4 gViewProj;
};

// t0: per-instance world matrices
StructuredBuffer<float4x4> gWorld : register(t0);

VSOut VSMain(VSIn input, uint instanceId : SV_InstanceID)
{
    VSOut o;

    float4 wpos = mul(gWorld[instanceId], float4(input.pos, 1.0));
    o.pos = mul(gViewProj, wpos);
    o.color = input.color;
    return o;
}

float4 PSMain(VSOut input) : SV_Target0
{
    return input.color;
}
```


