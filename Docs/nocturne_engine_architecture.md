# Nocturne Engine — Architecture Overview

> Target: Windows | Language: Modern C++ (C++20+)
>
> Genre Target: First‑Person Survival Horror
>
> Primary Source of Truth: *Game Engine Architecture (3rd Edition)* — Jason Gregory

---

## 1. Architectural Philosophy

Nocturne Engine is a **layered, subsystem‑oriented game engine**. Each subsystem has a clearly defined responsibility, explicit ownership rules, and a deterministic initialization and shutdown order.

Key principles:

- Clear separation between **engine code** and **game code**
- Explicit **platform abstraction**
- Engine **owns the main loop**
- Subsystems communicate through **well‑defined interfaces**, not global state
- Gameplay is **data‑driven where possible**, code‑driven where necessary

---

## 2. High‑Level Layering

```
Nocturne/
├── Engine/
│   ├── Core/
│   ├── Platform/
│   ├── Runtime/
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

**Purpose:** Provide low‑level foundational systems used by all other engine modules.

Responsibilities:

- Memory allocation (global allocator, arenas, pools)
- Math library (vectors, matrices, quaternions)
- Time system (high‑resolution timing, frame delta)
- Logging and assertions
- Engine‑wide type definitions and utilities

Rules:

- No dependency on platform‑specific APIs
- No dependency on rendering, physics, or gameplay

---

### 3.2 Engine/Platform

**Purpose:** Isolate operating‑system‑specific functionality.

Responsibilities:

- Window creation and management (Win32)
- OS message pump
- High‑resolution timers (QueryPerformanceCounter)
- File system access
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

Canonical loop structure:

```
while (engineRunning)
{
    processOSMessages();
    updateInput();
    updateSimulation();
    renderFrame();
}
```

---

### 3.4 Engine/Render

**Purpose:** All rendering and GPU interaction.

Responsibilities:

- Rendering API abstraction (Direct3D 12)
- GPU resource management
- Pipeline state management
- Camera and view handling
- Frame submission

Rules:

- No gameplay logic
- Rendering is driven by data produced by gameplay systems

---

### 3.5 Engine/Audio

**Purpose:** Audio playback and spatial sound simulation.

Responsibilities:

- Audio device management (XAudio2 or WASAPI)
- Sound effect playback
- Music playback and transitions
- 3D spatialization

---

### 3.6 Engine/Physics

**Purpose:** Physical simulation and collision queries.

Responsibilities:

- Collision detection
- Rigid body simulation
- Character controller support
- Spatial queries (raycasts, sweeps)

Notes:

- May integrate third‑party middleware
- Engine maintains abstraction boundary regardless of implementation

---

### 3.7 Engine/Input

**Purpose:** Unified input handling.

Responsibilities:

- Keyboard, mouse, controller input
- Action mapping
- Input state buffering per frame

---

### 3.8 Engine/Gameplay

**Purpose:** Gameplay foundation layer shared by all games built on the engine.

Responsibilities:

- Game object model
- Component system
- World representation
- Messaging and events
- Scripting integration (future)

Rules:

- Depends on engine systems
- Never depends on Game/* code

---

## 4. Game Layer

### Game/HorrorGame

**Purpose:** Game‑specific code and content.

Responsibilities:

- Player mechanics
- Enemy logic
- Horror‑specific systems
- Game rules and progression

Rules:

- Can depend on Engine modules
- Must not modify Engine internals

---

## 5. Tools Layer

**Purpose:** Offline and development tools.

Responsibilities:

- Asset importers
- Build and cook pipeline
- Debug and profiling tools
- Editor (future)

---

## 6. Dependency Rules (Non‑Negotiable)

- Core → used by everything
- Platform → used by Runtime, Input, File I/O
- Runtime → orchestrates all systems
- Render / Physics / Audio → independent of gameplay
- Gameplay → depends on engine systems
- Game → depends on Gameplay + Engine

No upward dependencies are allowed.

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

## 9. What Comes Next

Phase 1 will implement:

- Memory system
- Time system
- Logging
- Engine startup skeleton

This document remains the top‑level reference for all future work.

