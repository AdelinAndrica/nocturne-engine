# Phase 1 — Core Systems (Final, Updated)

> **Status:** COMPLETE ✅\
> **Verified:** Logging, Assert, Clock/Time, Memory, Subsystem lifecycle with dependency ordering

This document is the **authoritative implementation reference** for Phase 1 of Nocturne Engine. It reflects the **actual final code**, including fixes and adjustments discovered during implementation.

---

## 1. Phase Objective

Establish a **rock-solid engine core** that provides:

- Deterministic startup and shutdown
- Visibility (logging + asserts)
- Stable time measurement
- Controlled memory allocation
- Explicit subsystem dependency ordering

No rendering, windows, or gameplay systems exist yet.

---

## 2. Final Folder Structure (After Phase 1)

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

---

## 3. Subsystem Model (Final)

### 3.1 SubsystemDesc

```cpp
struct SubsystemDesc
{
    const char* name;
    std::span<const char* const> dependencies;

    bool (*startup)(void* ctx);
    void (*shutdown)(void* ctx);
};
```

**Why:**

- No virtual dispatch
- No global initialization order reliance
- Context pointer allows safe access to Engine internals without globals

---

## 4. SubsystemRegistry (Final Behavior)

### Responsibilities

- Detect duplicate subsystem names
- Resolve dependencies via **topological sort**
- Detect:
  - missing dependencies
  - dependency cycles
- Enforce deterministic startup/shutdown order

### Startup Rules

- All dependencies start **before** dependents
- On startup failure, already-started systems are shut down in reverse order

### Shutdown Rules

- Reverse of startup order

### Algorithm

- DFS-based topological sort
- States: `Unvisited`, `Visiting`, `Visited`
- Cycles detected when visiting a `Visiting` node

> **Design choice (not directly from the book):** DFS topo-sort implementation. Requirement for ordered startup/shutdown is book-grounded.

---

## 5. Logging System

### Files

- `Core/Log.h`
- `Core/Log.cpp`

### Key Types

```cpp
enum class LogLevel { Trace, Debug, Info, Warn, Error, Fatal };
class Logger;
```

### Usage

```cpp
NOC_LOG_INFO("Core", "Message %d", value);
```

### Behavior

- Enabled in Debug / Dev
- Compiled out in Ship
- Output to:
  - Visual Studio Output window (`OutputDebugStringA`)
  - stdout

---

## 6. Assert System

### Files

- `Core/Assert.h`
- `Core/Assert.cpp`

### Macros

- `NOC_ASSERT(expr)`
- `NOC_ASSERT_MSG(expr, msg)`
- `NOC_VERIFY(expr)` (evaluates in all builds)

### Behavior

- Debug/Dev: logs + breaks into debugger
- Ship: compiled out (except VERIFY evaluation)

---

## 7. Clock / Time System

### Files

- `Core/Clock.h`
- `Core/Clock.cpp`
- `Platform/Win32/WinPlatform.h / .cpp`

### Why renamed

Originally named `Time.h`, but this **collided with CRT \*\*\*\*****\<time.h>**, breaking `<ctime>`. Renamed to `Clock` to permanently avoid standard header collisions.

### Key Classes

```cpp
class HiResClock;
class TimeSystem;
```

### Features

- Uses `QueryPerformanceCounter`
- dt clamping to suppress breakpoint spikes
- `SecondsSinceStart()` for profiling/debug

---

## 8. Memory System

### Components

#### IAllocator

Abstract allocation interface.

#### MallocAllocator

- Wraps `_aligned_malloc` / `_aligned_free`
- Baseline allocator

#### DebugAlloc

- Wraps another allocator
- Tracks:
  - total allocated bytes
  - outstanding bytes
  - allocation count
- Debug/Dev only

#### LinearArena

- Bump allocator
- Used for **per-frame transient allocations**
- Reset once per frame

### Engine Integration

- Engine owns allocator instances
- Frame arena allocated once at startup
- Freed at shutdown
- Outstanding memory verified to be 0

---

## 9. Engine Class (Runtime)

### Responsibilities

- Register subsystems
- Own allocator + frame arena
- Drive startup / tick / shutdown

### Public Accessors

```cpp
IAllocator& Allocator();
LinearArena& FrameArena();
```

### Internal Lifecycle

```cpp
bool InitMemory();
void ShutdownMemory();
```

Subsystems call these via context pointer.

---

## 10. Verified Startup Order (Final)

### Startup

1. Log
2. Time
3. Memory
4. Assert

### Shutdown

1. Assert
2. Memory
3. Time
4. Log

---

## 11. Negative Test Results (Verified)

### Missing Dependency

```
[ERROR][Core] Missing dependency 'DoesNotExist' required by 'Memory'
```

### Cycle Detection

```
[ERROR][Core] Subsystem dependency cycle detected:
  -> Log
  -> Memory
  -> Log
```

---

## 12. Phase 1 Completion Criteria

All criteria met:

- Deterministic lifecycle
- Memory correctness
- Time stability
- Explicit dependency management

Phase 1 is **DONE**.

---

## 13. Next Phase Handoff

**Phase 2 Objective:**

> Create a real OS window, message pump, and continuous game loop.

Phase 2 will implement:

- Win32 window creation
- Message processing
- Real-time main loop stages
- Frame pacing (sleep vs spin)

---

## 14. Implementations

> This section contains the **full, final implementations** of every file produced in Phase 1.\
> Paths are shown exactly as they exist in the repository.\
> These implementations are **canonical** and must be kept in sync with engine code.

---

### `Engine/Core/BuildConfig.h`

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

---

### `Engine/Core/Assert.h`

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

---

### `Engine/Core/Assert.cpp`

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

---

### `Engine/Core/Log.h`

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

---

### `Engine/Core/Log.cpp`

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

---

### `Engine/Core/Clock.h`

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

---

### `Engine/Core/Clock.cpp`

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

---

### `Apps/NocturneHost/main.cpp`

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

---

### `Engine/Platform/Win32/WinPlatform.h`

```cpp
#pragma once
#include <cstdint>

namespace noc::platform {

uint64_t QueryHiResCounter();
uint64_t QueryHiResFrequency();

} // namespace noc::platform
```

---

### `Engine/Platform/Win32/WinPlatform.cpp`

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

---

### `Engine/Core/Memory/Allocator.h`

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

---

### `Engine/Core/Memory/Allocator.cpp`

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

---

### `Engine/Core/Memory/LinearArena.h`

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

---

### `Engine/Core/Memory/LinearArena.cpp`

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

---

### `Engine/Core/Memory/DebugAlloc.h`

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

---

### `Engine/Core/Memory/DebugAlloc.cpp`

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

---

### `Engine/Core/Subsystems/Subsystem.h`

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

---

### `Engine/Core/Subsystems/SubsystemRegistry.h`

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

---

### `Engine/Core/Subsystems/SubsystemRegistry.cpp`

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

---

### `Engine/Runtime/Engine.h`

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

---

### `Engine/Runtime/Engine.cpp`

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

---

**End of Phase 1**

