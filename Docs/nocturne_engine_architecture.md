# Nocturne Engine — Architecture Overview

> Target: Windows | Language: Modern C++ (C++20+)
>
> Genre Target: First-Person Survival Horror
>
> Primary Source of Truth: *Game Engine Architecture (3rd Edition)* — Jason Gregory

---

## 1. Architectural Philosophy

Nocturne Engine is a **layered, subsystem-oriented game engine**. Each subsystem has a clearly defined responsibility, explicit ownership rules, and a deterministic initialization and shutdown order.

Key principles:

- Clear separation between **engine code** and **game code**
- Explicit **platform abstraction**
- Engine **owns the main loop**
- Subsystems communicate through **well-defined interfaces**, not global state
- Gameplay is **data-driven where possible**, code-driven where necessary
- **Asynchronous, non-blocking systems by default** (I/O, resource loading)

---

## 2. High-Level Layering

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

---

## 3. Engine Layers

### 3.1 Engine/Core

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

### 3.2 Engine/Platform

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

### 3.3 Engine/Runtime

**Purpose:** Coordinate engine execution and subsystem lifetimes.

Responsibilities:

- Engine startup and shutdown sequencing
- Subsystem registration and dependency ordering
- Main game loop ownership
- Frame lifecycle management
- Multi-rate subsystem scheduling

Canonical loop structure (conceptual):

```

while (engineRunning)
{
processOSMessages();
pollInput();
serviceScheduledSystems();   // multi-rate stepping
renderStage();
}

```

#### Multi-Rate Update Model (Locked)

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

### 3.3.1 Engine Configuration Model (Boot-Time)

The engine exposes an **engine-owned configuration model** used to control
boot-time behavior.

Key rules:

- The engine defines its own configuration structures (e.g. `EngineConfig`)
- Configuration provides **mechanisms**, not policies
- The application (game/editor/host) may override configuration values
- Configuration is **consumed during engine initialization**

#### Configuration Lifecycle

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

### 3.4 Engine/Resources

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

#### Content Root & File System Policy

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

### 3.5 Engine/Render

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

### 3.6 Engine/Audio

**Purpose:** Audio playback and spatial sound simulation.

Responsibilities:

- Audio device management
- Sound effect playback
- Music playback and transitions
- 3D spatialization

---

### 3.7 Engine/Physics

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

### 3.8 Engine/Input

**Purpose:** Unified human interface device handling.

Responsibilities:

- Keyboard, mouse, controller input
- Per-frame input state buffering
- Action mapping (logical actions decoupled from physical devices)

Rules:

- Input is polled and buffered once per frame
- Gameplay consumes input data produced by this system

---

### 3.8.5 Engine/Camera

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

### 3.9 Engine/Gameplay

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

## 4. Game Layer

### Game/HorrorGame

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

## 5. Tools Layer

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

### Editor ↔ Runtime Boundary (Locked)

The editor is a tool built on top of the runtime engine. The runtime retains ownership of the main loop and system orchestration.

Rules:

- The editor may embed and drive an instance of the runtime engine.
- The editor must not replace or fork the engine loop; it can only provide hooks (e.g., pause/step, inspection, gizmos).
- Play-In-Editor runs the same runtime loop with editor-provided bridges, not a separate “editor loop.”

---

## 6. Dependency Rules (Non-Negotiable)

- Core → used by everything
- Platform → used by Runtime, Input, File I/O
- Runtime → orchestrates all systems
- Resources → used by Render, Audio, Physics, Gameplay
- Render / Physics / Audio → independent of gameplay
- Gameplay → depends on engine systems
- Game → depends on Gameplay + Engine

No upward dependencies are allowed.

---

### 6.1 Policy vs Mechanism Ownership (Locked)

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

## 7. Naming and Code Conventions

- Engine namespace: `noc::`
- Game namespace: `game::`
- No STL types in public engine headers
- Explicit ownership (no hidden globals)

---

## 8. What This Document Is

- A **living architectural contract**
- The reference used before adding any new system
- The baseline against which refactors are judged

---

## 9. Roadmap (Phase-Based)

### Completed

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

### Full Roadmap

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

