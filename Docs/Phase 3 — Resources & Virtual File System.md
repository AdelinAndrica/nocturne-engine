# Phase 3 — Resources & Virtual File System

> **Status:** READY FOR IMPLEMENTATION ⏳  
> **Scope:** Virtual File System (VFS), mount points, virtual paths, file I/O abstraction  
> **Depends on:** Phase 1 — Core Systems, Phase 2 — Window & Main Loop  
>
> **Primary source of truth:** Jason Gregory, *Game Engine Architecture (3rd Edition)*

This document is the **authoritative design and implementation reference** for Phase 3 of Nocturne Engine.

Phase 3 introduces the **first data-facing engine system**: a **Virtual File System** that decouples engine code from physical storage and forms the foundation for all future resource loading.

No rendering.  
No asset parsing.  
No async loading.

Only **bytes in → bytes out**, correctly.

## Phase 3 — Resources & Virtual File System (Checklist)

### ✅ Completed
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
  - Smoke test confirmed: mount → open → read → close works (`hello.txt`).
- [x] **Basic archive mount code compiled**
  - Archive-related classes build and link (ZIP indexing path compiles).

---

### ⏳ Not Yet Completed (Remaining Work)
#### 1) Clean up Phase-3 testing
- [ ] **Move the `hello.txt` smoke test out of `Engine::Init()`**
  - Prefer host/app-side test or a dedicated debug test function.

#### 2) VFS convenience APIs (high leverage for upcoming phases)
- [ ] **`VirtualFileSystem::ReadAllBytes(vpath)`**
  - Open → allocate buffer → read full file → close.
  - Decide allocator strategy (engine allocator vs `std::vector` with standard allocator).
- [ ] **`VirtualFileSystem::ReadAllText(vpath)` (debug-only helper)**
  - Wraps `ReadAllBytes`, appends `'\0'`, returns `std::string`.

#### 3) Archive mount functionality validation (ZIP)
- [ ] **Archive indexing test**
  - Mount a `.zip` and log number of entries indexed.
- [ ] **Stored-entry read support verified**
  - Confirm files using ZIP method **0 (stored)** can be opened and read.
- [ ] **Graceful handling of unsupported compression**
  - If method != 0, log clear error and fail open cleanly.

#### 4) Mount priority behavior (policy)
- [ ] **Explicit mount priority documented + tested**
  - Confirm “earlier mounts win” (or change to “later mounts win”).
  - Add a simple override test (`DataOverrides/hello.txt` beats `Data/hello.txt`) if desired.

#### 5) Documentation updates (must match decisions)
- [ ] **Update `Docs/Nocturne Engine Architecture.md`**
  - Add “Engine Configuration Model” section.
  - Add “Content Root & File System Policy” section.
  - Add “Policy vs Mechanism Ownership (Locked)” section.
- [ ] **Create/Update `Docs/Phase 3 — Resources and File System.md`**
  - Scope, implementation notes, and verification checklist.
  - Include the final content root policy you’re using.

---

### ✅ Phase 3 Exit Criteria
- [ ] Loose mount works with deterministic content root policy (already validated).
- [ ] Archive mount indexing + stored-entry reading validated.
- [ ] `ReadAllBytes` helper exists and is used by at least one test.
- [ ] Smoke test lives outside engine init (host-side or debug test harness).
- [ ] Architecture doc updated with locked config + content-root decisions.


---

## 1. Phase Objective

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

## 2. Book-Grounded Scope (Why This Phase Exists)

Phase 3 is grounded primarily in:

- **Chapter 7 — Engine Support Systems**  
  File systems as foundational, platform-isolated engine services
- **Chapter 8 — Resources and the File System**  
  Virtual paths, mount-based resolution, and resource indirection

Jason Gregory explicitly stresses that **resource identity must be decoupled from physical storage**, enabling packaging, streaming, and platform portability.

Everything not explicitly mandated is marked **Design choice (not directly from the book)**.

---

## 3. Phase 3 Responsibilities (Hard Rules)

Phase 3 introduces **exactly four responsibilities**:

1. **Virtual path abstraction**
2. **Mount-based resolution**
3. **Platform-isolated file I/O**
4. **Unified read-only file access API**

### Explicit Non-Goals

- ❌ No resource manager
- ❌ No async I/O
- ❌ No asset parsing (textures, meshes, audio)
- ❌ No hot reload or file watching
- ❌ No compression strategy tuning
- ❌ No editor or tooling

Those come later.

---

## 4. Updated Folder Structure (After Phase 3)

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

---

## 5. Virtual Paths

All file access uses **virtual paths**, never OS paths.

Examples:

```

textures/ui/crosshair.dds
models/characters/doctor.mesh
audio/ambient/hallway_loop.wav

```

Rules:

- Forward slashes `/` only
- Case-sensitive (engine rule)
- No drive letters
- No absolute paths
- No `..` traversal

> **Book grounding:** Virtual paths enable resource packaging, redirection, and portability.

---

## 6. Mount System Design

### Mount Concept

A **mount** maps a virtual root (`/`) to a physical data source.

Example mounts:

| Priority | Virtual Root | Physical Source |
|--------|--------------|-----------------|
| 0 | `/` | `D:/Nocturne/Data/` |
| 1 | `/` | `D:/Nocturne/Packed/game.zip` |

Resolution rules:

1. Mounts are queried **in order**
2. First successful open wins
3. Later mounts override earlier ones

This supports development overrides without repackaging.

---

## 7. File System Backends

Phase 3 introduces **file system backends**, not resources.

### 7.1 Loose Directory Mount

- Reads directly from the OS filesystem
- Development-friendly
- Read-only from engine perspective

### 7.2 Archive Mount

- Read-only
- Central directory lookup
- No runtime modification

> **Design choice:** ZIP-like archive format for simplicity and tooling support.

---

## 8. Platform Isolation (Mandatory)

All OS file access is isolated to:

```

Engine/Platform/Win32/WinFileSystem.*

```

Responsibilities:

- Open file
- Read bytes
- Seek
- Query file size
- Close file

No other engine layer may include `<Windows.h>` for file I/O.

---

## 9. VFS Ownership & Lifetime

Ownership hierarchy:

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

* [ ] Engine mounts a loose data directory at startup
* [ ] Files open correctly via virtual paths
* [ ] Archive and loose mounts coexist
* [ ] Mount priority resolves deterministically
* [ ] No OS file APIs are used outside Platform layer
* [ ] Clean shutdown with no leaked file handles

---

## 13. Common Pitfalls (Phase 3)

* Letting OS paths leak above Platform
* Mixing resource identity with file location
* Designing async APIs too early
* Hardcoding archive assumptions into higher layers

---

## 14. Phase 3 Completion Criteria

Phase 3 is complete when:

* A Virtual File System exists
* All file access goes through it
* No resource-specific logic exists yet

This becomes the **foundation for all asset loading and streaming**.

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

## 16. Implementations

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

        for (auto& m : mounts_)
        {
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
		// Mount priority rule (current VFS behavior): earlier mounts win.
		// For dev overrides, mount overrideRoot FIRST so it wins.
		if (cfg_.overrideRoot && cfg_.overrideRoot[0] != 0)
		{
			if (!vfs_.MountLooseDirectory(cfg_.overrideRoot))
				NOC_LOG_WARN("VFS", "Failed to mount overrideRoot: %s", cfg_.overrideRoot);
		}

		if (cfg_.contentRoot && cfg_.contentRoot[0] != 0)
		{
			if (!vfs_.MountLooseDirectory(cfg_.contentRoot))
				NOC_LOG_WARN("VFS", "Failed to mount contentRoot: %s", cfg_.contentRoot);
		}

		if (cfg_.archivePath && cfg_.archivePath[0] != 0)
		{
			if (!vfs_.MountArchive(cfg_.archivePath))
				NOC_LOG_WARN("VFS", "Failed to mount archivePath: %s", cfg_.archivePath);
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