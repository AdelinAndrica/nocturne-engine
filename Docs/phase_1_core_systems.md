# Nocturne Engine — Phase 1.md

> **Phase 1: Core Systems — Memory, Time, Logging, Assertions, and Subsystem Lifecycle**
>
> Target: **Windows** | Language: **C++20**
>
> **Primary source of truth:** Jason Gregory, *Game Engine Architecture (3rd Edition)*
>
> This document is the **authoritative spec** for Phase 1 implementation.

---

## 0. Book-grounded scope (what Phase 1 is based on)

Phase 1 is derived from these parts of the book:

- **Chapter 6 — Engine Support Systems**: why engines need low-level support systems; subsystem start-up/shut-down ordering; memory management motivation and practices. fileciteturn3file1 fileciteturn3file2 fileciteturn3file4
- **Chapter 8.5 — Measuring and Dealing with Time**: high-resolution timers, delta time measurement issues (v-sync quantization context; breakpoint spikes; clock drift). fileciteturn3file15
- **Chapter 10.1 — Logging and Tracing**: practical need for printf-style tracing and platform-specific output considerations (e.g., OutputDebugString on Win32). fileciteturn3file9
- **Section 3.2.3.3 — Assertions**: why assertions exist, build-config control, and macro-based implementation patterns. fileciteturn3file18

Everything else is explicitly labeled **Design choice (not directly from the book)**.

---

## 1. Phase objective

By the end of Phase 1 we will have a working **engine core library** that provides:

- Deterministic **subsystem registration + start-up/shut-down ordering** (no reliance on global static init order)
- A **time module** that measures per-frame delta time using a high-resolution timer and protects against debugger breakpoint spikes
- A robust **assertion system** (configurable by build type)
- A configurable **logging system** (channels, severity, sinks; Win32 debug output)
- A minimal **memory layer** (arenas + allocator interfaces + tracking hooks)

The Phase 1 deliverable is a runnable host app that initializes the core, prints timing/log output, and shuts down cleanly.

---

## 2. Repository & folder structure (Phase 1 state)

> This section is updated **every phase** and is treated as source-of-truth for physical architecture.

```
Nocturne/
├── Engine/
│   ├── Core/
│   │   ├── Assert.h
│   │   ├── BuildConfig.h
│   │   ├── Log.h
│   │   ├── Log.cpp
│   │   ├── Time.h
│   │   ├── Time.cpp
│   │   ├── Memory/
│   │   │   ├── Allocator.h
│   │   │   ├── Allocator.cpp
│   │   │   ├── Arena.h
│   │   │   ├── Arena.cpp
│   │   │   ├── LinearArena.h
│   │   │   ├── LinearArena.cpp
│   │   │   ├── DebugAlloc.h
│   │   │   └── DebugAlloc.cpp
│   │   └── Subsystems/
│   │       ├── Subsystem.h
│   │       ├── SubsystemRegistry.h
│   │       └── SubsystemRegistry.cpp
│   ├── Platform/
│   │   └── Win32/
│   │       ├── WinPlatform.h
│   │       └── WinPlatform.cpp
│   └── Runtime/
│       ├── Engine.h
│       └── Engine.cpp
│
├── Game/
│   └── HorrorGame/
│       └── (empty in Phase 1)
│
├── Apps/
│   └── NocturneHost/
│       ├── main.cpp
│       └── NocturneHost.vcxproj (or CMakeLists.txt)
│
└── Docs/
    ├── Nocturne Engine Architecture.md
    └── Phase 1.md
```

### Notes

- `Engine/Core/*` must be usable with **no renderer, no physics, no assets**.
- `Engine/Runtime/Engine` owns startup/shutdown and (later) the main loop.

---

## 3. Build configurations (required in Phase 1)

Grounded conceptually in the book’s discussion of multiple build configurations. fileciteturn3file17

We support three configurations (Design choice: exact naming):

- **Debug**: asserts on, logging verbose, debug allocator tracking enabled
- **Dev**: asserts on, logging moderate, some checks compiled out
- **Ship**: asserts mostly off, logging minimal, tracking off

Implementation is centralized in `BuildConfig.h`.

---

## 4. Subsystem start-up / shut-down (core runtime contract)

### 4.1 Why we do this

The book is explicit:

- A game engine is made of **many interacting subsystems**.
- Startup must occur in a **specific order** based on dependencies.
- C++ static/global initialization order is **unpredictable**, so relying on it is unsafe. fileciteturn3file1 fileciteturn3file4

### 4.2 What we implement

We implement a lightweight subsystem registry:

- Subsystems are registered in code with:
  - **Name**
  - **Dependency list** (names)
  - `Startup()` and `Shutdown()` callbacks
- Registry performs a **topological sort** and starts subsystems in dependency order
- Shutdown is in the exact reverse order

> Design choice (not directly from the book): using explicit dependency lists + topo-sort rather than function-static singleton “construct on demand.” Gregory discusses construct-on-demand as a workaround, but we prefer explicit ordering because it is testable, deterministic, and debuggable. fileciteturn3file4

### 4.3 Types

#### `noc::SubsystemDesc`
**File:** `Engine/Core/Subsystems/Subsystem.h`

```cpp
struct SubsystemDesc {
  const char* name;
  std::span<const char* const> dependencies;
  bool (*startup)();
  void (*shutdown)();
};
```

**Why / when:** Defines a subsystem in a data-only form suitable for registration.

**Usage:** Each module exposes a `RegisterSubsystems(SubsystemRegistry&)` function and adds its descriptors.

---

#### `noc::SubsystemRegistry`
**File:** `Engine/Core/Subsystems/SubsystemRegistry.h/.cpp`

**Responsibilities:**

- `Register(const SubsystemDesc&)`
- `StartupAll()` → returns success/failure
- `ShutdownAll()` → always attempts best-effort shutdown
- Diagnostics: missing deps, cycles, duplicate names

**Relationships:**

- Used by `noc::Engine` to boot core systems first.
- Core systems register themselves: `Log`, `Time`, `Memory`.

**Failure policy:**

- If startup fails, registry shuts down any already-started subsystems in reverse order.

---

## 5. Assertions

### 5.1 Why

Assertions are “land mines for bugs” and enforce programmer assumptions early; they should be configurable by build type. fileciteturn3file18

### 5.2 What we implement

**Files:** `Engine/Core/Assert.h`, `Engine/Core/BuildConfig.h`

- `NOC_ASSERT(expr)`
- `NOC_ASSERT_MSG(expr, fmt, ...)`
- `NOC_VERIFY(expr)` (Design choice: evaluates expression even if asserts compiled out)

### 5.3 Platform behavior (Windows)

- If a debugger is attached, trigger a breakpoint (Design choice: `__debugbreak()`)
- Otherwise, log and terminate

### 5.4 API surface

#### `noc::AssertFailHandler`
**File:** `Assert.h`

```cpp
using AssertFailHandler = void(*)(
  const char* expr,
  const char* file,
  int line,
  const char* msg);

void SetAssertFailHandler(AssertFailHandler);
```

**Why:** Allows routing assertion failures into logs, crash reporting, or tests.

---

## 6. Logging

### 6.1 Why

The book notes that printf-style tracing remains essential, especially for timing-dependent bugs or long event sequences; platform output varies (e.g., OutputDebugString for Win32 windowed apps). fileciteturn3file9

### 6.2 What we implement

**Files:** `Engine/Core/Log.h/.cpp`

- Log levels: `Trace, Debug, Info, Warn, Error, Fatal`
- Channels: string name (e.g., `"Core"`, `"Render"`)
- Sinks:
  - Win32 debug sink (`OutputDebugStringA/W`)
  - Console sink (if console is present)
  - File sink (Design choice: optional in Phase 1; implemented if easy)

### 6.3 Types

#### `noc::LogLevel`
**File:** `Log.h`

```cpp
enum class LogLevel : uint8_t {
  Trace, Debug, Info, Warn, Error, Fatal
};
```

#### `noc::LogMessage`
**File:** `Log.h`

Holds:
- timestamp (from `Time`)
- level
- channel
- formatted text

> Design choice: timestamp is stored as microseconds since engine start.

#### `noc::ILogSink`
**File:** `Log.h`

```cpp
struct ILogSink {
  virtual ~ILogSink() = default;
  virtual void Write(const LogMessage& msg) = 0;
};
```

**Why:** decouple logging from output devices.

#### `noc::Logger`
**File:** `Log.h/.cpp`

Responsibilities:

- owns sink list
- formats messages
- thread-safety policy

Thread safety:

- **Design choice:** `Logger` is thread-safe via a mutex for Phase 1 (job system comes later).

### 6.4 Front-end macros

**File:** `Log.h`

- `NOC_LOG_INFO(channel, fmt, ...)`
- `NOC_LOG_WARN(channel, fmt, ...)`
- `NOC_LOG_ERROR(channel, fmt, ...)`

**Why:** Compile-time stripping by log level in Ship builds (Design choice).

---

## 7. Time system

### 7.1 Why

The book’s timing section explains:

- Measure wall-clock frame time with **high-resolution timers**
- Beware **v-sync quantization** and **frame-rate assumptions**
- Handle **debugger breakpoints** to avoid huge dt spikes
- Beware **timer drift** on some multi-core systems (don’t compare unrelated core clocks). fileciteturn3file15

### 7.2 What we implement

**Files:** `Engine/Core/Time.h/.cpp`, `Engine/Platform/Win32/WinPlatform.*`

- Use Win32 `QueryPerformanceCounter/QueryPerformanceFrequency`
- Provide:
  - `NowTicks()` and `TicksToSeconds()`
  - Per-frame `BeginFrame()` / `EndFrame()`
  - `DeltaSeconds()`
  - Clamping for breakpoint spikes

Breakpoint spike handling:

- If computed dt exceeds `kMaxDtSeconds` (default 1.0f), replace dt with a target dt (e.g., 1/60). This is directly recommended as a simple approach. fileciteturn3file15

> Design choice: use 60Hz target dt in Nocturne.

### 7.3 Types

#### `noc::HiResClock`
**File:** `Time.h/.cpp`

```cpp
class HiResClock {
public:
  void Init();
  uint64_t NowTicks() const;
  double   TicksToSeconds(uint64_t dtTicks) const;
  uint64_t Frequency() const;
private:
  uint64_t freq_ = 0;
};
```

**Where used:**

- Only inside `TimeSystem` and profiling helpers.

#### `noc::TimeSystem`
**File:** `Time.h/.cpp`

Responsibilities:

- Maintain engine start time
- Track last-frame tick
- Compute dt
- Expose time in seconds

Key methods:

```cpp
class TimeSystem {
public:
  void Init();
  void BeginFrame();
  void EndFrame();

  float DeltaSeconds() const;
  double SecondsSinceStart() const;

  void SetMaxDeltaSeconds(float maxDt);
  void SetBreakpointFallbackDelta(float dt);
};
```

**When called:**

- `Engine::Tick()` calls `time.BeginFrame()` at top of each frame
- `time.EndFrame()` after work is completed

---

## 8. Memory system

### 8.1 Why

Chapter 6 emphasizes two performance axes:

1) General-purpose allocation (`malloc/new`) is slow → use custom allocators
2) Memory access patterns dominate performance → keep data contiguous and cache-friendly fileciteturn3file2

### 8.2 Phase 1 scope

We are **not** building a full production allocator suite yet.

Phase 1 delivers:

- An allocator interface
- A linear (bump) arena used for frame-temporary allocations
- A debug allocation wrapper for tracking/leak detection in Debug builds

> Design choice: We keep the interface minimal now and expand it once systems (ECS, renderer, resources) force real requirements.

### 8.3 Types

#### `noc::IAllocator`
**File:** `Engine/Core/Memory/Allocator.h`

```cpp
class IAllocator {
public:
  virtual ~IAllocator() = default;
  virtual void* Allocate(size_t size, size_t alignment) = 0;
  virtual void  Deallocate(void* p) = 0;
};
```

**Where used:**

- All Phase 1 systems that allocate memory accept an `IAllocator&` (Design choice: dependency injection rather than globals).

#### `noc::MallocAllocator`
**File:** `Allocator.cpp`

A baseline allocator using aligned `::operator new` / `::operator delete`.

**Why:** provides a reference implementation.

#### `noc::LinearArena`
**File:** `LinearArena.h/.cpp`

A bump allocator with:

- `Reset()` to reuse memory
- No individual frees

Key methods:

```cpp
class LinearArena {
public:
  void Init(void* backingMemory, size_t bytes);
  void* Allocate(size_t size, size_t alignment);
  void Reset();
  size_t Capacity() const;
  size_t Used() const;
};
```

**When used:**

- Per-frame temporary allocations inside the game loop

**Why:** eliminates per-allocation overhead and improves locality (aligned with book guidance on avoiding slow general-purpose alloc). fileciteturn3file2

#### `noc::DebugAlloc`
**File:** `DebugAlloc.h/.cpp`

Tracks:

- total bytes allocated
- allocation count
- optional callsite tags

> Design choice: callstack capture is deferred to later (Windows stack walking adds complexity).

---

## 9. Engine bootstrap (Phase 1 host)

### 9.1 `noc::Engine`

**File:** `Engine/Runtime/Engine.h/.cpp`

Responsibilities:

- Own `SubsystemRegistry`
- Register core subsystems (log, time, memory)
- Provide `Init()`, `Tick()`, `Shutdown()`

Key methods:

```cpp
class Engine {
public:
  bool Init();
  void TickOnce();
  void Shutdown();

  Logger&    Log();
  TimeSystem& Time();
  IAllocator& Allocator();
};
```

> Design choice: `TickOnce()` in Phase 1 (no real loop yet). The full loop becomes Phase 2/3 when window + input enter.

### 9.2 `Apps/NocturneHost/main.cpp`

The host application:

- constructs Engine
- calls `Init()`
- calls `TickOnce()` N times (or a simple loop)
- logs dt
- calls `Shutdown()`

---

## 10. Relationships & dependency graph (Phase 1)

```
Platform/Win32
   ↑
Core/Time  ←── Core/Log (timestamps)
   ↑              ↑
Runtime/Engine ───┘
   ↑
Apps/NocturneHost

Core/Memory is used by all (optionally in Phase 1, mandatory later)
```

Rules:

- `Core` has no dependency on `Runtime`
- `Runtime` depends on `Core` and `Platform`

---

## 11. Usage examples (how we expect you to use these systems)

### 11.1 Logging

- Use logging for:
  - subsystem startup failures
  - unexpected but survivable runtime conditions
  - dev-only tracing

- Do NOT use logging for:
  - tight per-object per-frame spam (performance)

### 11.2 Assertions

- Use assertions for:
  - invalid invariants
  - programmer mistakes
  - impossible states

- Do NOT use assertions for:
  - normal runtime errors (use return status + log)

### 11.3 Linear arena

- Use for:
  - per-frame scratch buffers
  - transient containers

- Do NOT use for:
  - long-lived objects
  - resources with independent lifetimes

---

## 12. Verification checklist (Phase 1 is done when…)

- [ ] Engine starts and shuts down deterministically every run
- [ ] Subsystem dependency ordering is enforced; cycles are detected and reported
- [ ] `TimeSystem` reports stable dt and clamps dt after a debugger pause
- [ ] Logging prints to Visual Studio output window (Win32) and/or console
- [ ] Assertions break into the debugger in Debug/Dev builds
- [ ] Linear arena allocates aligned memory and resets correctly

---

## 13. Common pitfalls (Phase 1)

- Relying on global static initialization order (explicitly discouraged). fileciteturn3file4
- Letting dt spikes propagate after breakpoints (explicitly causes bad behavior). fileciteturn3file15
- Overusing logs in hot loops (printf debugging is great, but can destroy performance). fileciteturn3file9
- Using assertions for recoverable errors (assertions are for violated assumptions). fileciteturn3file18

---

## 14. Next phase handoff

When Phase 1 is implemented and verified, the next document will be **Phase 2.md** and will add:

- Window + message pump
- Input capture (keyboard/mouse)
- Real main loop ownership and frame stages

Say in the next chat:

> “Phase 1 is complete. Here’s my repo tree and a log output screenshot. Start Phase 2: Window + main loop.”
