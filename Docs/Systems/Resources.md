---
id: "noc.resources"
doc_type: "system"
canonical: true
status: "implemented"
subsystem: "Resources"
phase_introduced: 4
description: "Canonical virtual file system, resource loading, typed resource and runtime asset-lifetime contract."
source_files: ["Engine/Resources/VirtualFileSystem.h","Engine/Resources/ResourceManager.h","Engine/Resources/ResourceManager.cpp","Engine/Resources/ResourceHandle.h","Engine/Resources/Typed/ResourceLoaderRegistry.h"]
source_docs: ["Docs/nocturne_engine_architecture.md","Docs/Phase 4 — Resource Manager.md","Docs/Phase 5 — Typed Resources & Loader Registry.md","Docs/Phase 11 — Asset Import Pipeline.md"]
book_grounding: ["Jason Gregory — Game Engine Architecture (3rd ed.), §7.2 The Resource Manager","Jason Gregory — Game Engine Architecture (3rd ed.), §7.2.2 Runtime Resource Management"]
aliases: ["Resources","Resource Manager","VFS","Virtual File System"]
deprecated_aliases: []
api_symbols: ["noc::VirtualFileSystem","noc::ResourceManager","noc::ResourceHandle"]
---

# Resources

## Purpose

The Resources layer provides one runtime path for locating, requesting, loading and accessing engine assets.

The system separates **where bytes come from** from **what a resource means**.

## Architecture

```text
virtual path
    |
    v
VirtualFileSystem
    |
    v
ResourceManager
    |
    +--> binary resource
    |
    +--> typed loader registry
             |
             +--> Text
             +--> Mesh
             +--> Texture
             +--> Material
```

The application supplies physical content layout through configuration. The runtime does not guess where project content lives.

## Virtual File System

The VFS is the runtime mechanism for mapping the engine's virtual namespace onto mounted storage.

Current mount implementations include:

- loose-file mounts;
- archive-file mounts.

The runtime architecture permits development content to come from loose files and packaged content to come from archives while resource users continue to address assets through virtual paths.

**Design choice (not directly from the book):** the application defines the physical content roots and mount policy; the engine mounts exactly the paths it is given.

## Resource manager

`ResourceManager` is initialized with the owning `Engine` and `VirtualFileSystem`.

Current public request/state operations include:

```text
RequestBinary(...)
IsReady(...)
HasFailed(...)
GetBytes(...)
GetSize(...)
GetError(...)
WaitUntilReady(...)
Update()
```

Typed requests currently include:

```text
RequestText(...)
RequestMesh(...)
RequestTexture(...)
RequestMaterial(...)
```

Typed resources are resolved through `ResourceLoaderRegistry`.

## Asynchronous loading

The resource manager owns load-job submission internally and exposes readiness/failure state instead of requiring all callers to perform synchronous file I/O.

Gregory identifies streaming/asynchronous loading as a normal runtime resource-manager responsibility and describes the resource manager as responsible for loading, unloading, lifetime and composite-resource relationships.

Nocturne's exact job integration is **Design choice (not directly from the book)**.

## Tool/runtime boundary

The runtime consumes engine-ready resources.

Offline authoring/import/cook/package work belongs to tools and the asset pipeline.

The runtime is responsible for reading mounted content; tools are responsible for producing packaged/archive content.

This prevents asset-authoring policy from leaking into runtime resource access.

## Invariants

- Resource access begins from the virtual namespace, not ad-hoc absolute paths in gameplay.
- The application owns content-layout policy; the engine owns VFS/resource mechanisms.
- Resource handles are validated before access.
- A failed resource exposes a diagnostic state instead of becoming silently valid.
- Typed loaders are registered through the resource-loader registry.
- Rendering may consume loaded resources, but Resources does not contain gameplay logic.

## Non-responsibilities

Resources does not own:

- editor Content Browser presentation;
- GPU draw submission;
- game-specific asset semantics;
- scene serialization;
- source-asset DCC authoring.

## Book grounding

Primary grounding:

- Jason Gregory — *Game Engine Architecture, 3rd Edition*, §7.2, for the resource-manager/tool-chain split.
- Jason Gregory — *Game Engine Architecture, 3rd Edition*, §7.2.2, for runtime loading, lifetime, single-instance/composite-resource and streaming responsibilities.

Nocturne's VFS namespace, handle formats, typed-loader API and application-owned mount policy are **Design choice (not directly from the book)**.

## Current implementation sources

- `Engine/Resources/VirtualFileSystem.h/.cpp`
- `Engine/Resources/IFileMount.h`
- `Engine/Resources/LooseFileMount.h/.cpp`
- `Engine/Resources/ArchiveFileMount.h/.cpp`
- `Engine/Resources/ResourceManager.h/.cpp`
- `Engine/Resources/ResourceHandle.h`
- `Engine/Resources/Typed/`
- `Engine/Assets/AssetImportPipeline.*`

## Related documentation

- [Runtime](/docs/systems/runtime/)
- [Rendering](/docs/systems/rendering/)
- [Architecture Overview](/docs/architecture/overview/)
