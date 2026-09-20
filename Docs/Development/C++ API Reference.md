---
id: "noc.development.cpp-api"
doc_type: "reference"
canonical: true
status: "active"
subsystem: "Documentation"
phase_introduced: 16
description: "Canonical contract for generated Nocturne C++ API documentation, symbol links and Doxygen XML tooling output."
source_files: ["Docs/API/Doxyfile","Docs/API/MainPage.dox
Docs/API/BeginnerGuide.dox","Docs/API/BeginnerGuide.dox","Website/scripts/prepare-api.ps1","Website/scripts/validate-api.ps1"]
source_docs: ["Docs/Development/Documentation Model.md","Docs/Web/Nocturne Website — Design & Technical Specification.md"]
book_grounding: ["Jason Gregory — Game Engine Architecture (3rd ed.), §1.7 Tools and the Asset Pipeline","Jason Gregory — Game Engine Architecture (3rd ed.), §7.2.2 Runtime Resource Management","Jason Gregory — Game Engine Architecture (3rd ed.), §8.2 Game Loop","Jason Gregory — Game Engine Architecture (3rd ed.), §11.2 Rendering Pipeline","Jason Gregory — Game Engine Architecture (3rd ed.), §15.4 Game World Editor","Jason Gregory — Game Engine Architecture (3rd ed.), §16.2 Runtime Object Model Architectures"]
aliases: ["C++ API Reference","API Reference","Doxygen API"]
deprecated_aliases: []
api_symbols: ["noc::Engine","noc::ResourceManager","noc::RenderSystem","noc::World","nocturne::editor::EditorShellV3"]
---

# C++ API Reference

## Purpose

The generated C++ API reference answers a different question from the conceptual documentation:

> **What does this type/function do, when should I use it, what owns it, and what rules do I need to obey?**

Canonical Architecture, Systems and Development documents remain responsible for:

- ownership;
- subsystem responsibilities;
- lifecycle;
- dependency direction;
- rationale;
- design constraints;
- current architectural contracts.

The API reference is a beginner-oriented symbol/use companion to those documents, not a replacement for them.

Web 7.1 deliberately changes the default from **"show every symbol and let the reader infer usage"** to **"teach the supported public surface first"**.

## Human API route

Generated HTML is published at:

`/api/`

The Doxygen landing page is:

`/api/index.html`

The generated API uses the current Nocturne dark visual vocabulary while remaining a separate generated reference surface.

The landing page sends new readers to:

`Beginner Guide`

before the alphabetical class list.

The recommended learning order is:

```text
Engine / MainLoop
    ↓
EntityHandle / World / components
    ↓
VirtualFileSystem / ResourceManager
    ↓
RenderQueue / RenderSystem
    ↓
EditorShellV3 / EditorViewportController
```


## Machine/tooling route

Doxygen XML is published at:

`/api-xml/index.xml`

The full XML compound set is available below `/api-xml/`.

A Doxygen tag file is also published at:

`/api/nocturne.tag`

These outputs are intended for tooling that needs symbol-level structure rather than conceptual prose.

## Stable symbol aliases

Web 7 generates stable redirect routes from qualified C++ names.

Examples:

```text
/api-symbol/noc.Engine.html
/api-symbol/noc.ResourceManager.html
/api-symbol/noc.RenderSystem.html
/api-symbol/noc.World.html
/api-symbol/nocturne.editor.EditorShellV3.html
```

The redirect target is derived from Doxygen XML, so conceptual documentation does not need to depend directly on Doxygen's internal `refid` filenames.

A generated machine index is published at:

`/api-symbols.json`

It records:

- fully qualified symbol name;
- compound kind;
- Doxygen refid;
- stable alias route;
- direct Doxygen HTML route.

## Extraction scope

Current Doxygen input:

```text
Engine/
Apps/NocturneEditor/
Apps/NocturneHost/
Docs/API/MainPage.dox
```

Header patterns:

```text
*.h
*.hpp
*.inl
```

The generator intentionally excludes:

- `ThirdParty/`;
- test directories;
- `Engine/Render/DX12/d3dx12.h`;
- legacy `Apps/NocturneEditor/EditorShell.h`;
- legacy `Apps/NocturneEditor/EditorControls.h`.

`EditorShellV3` is the current canonical editor shell.

## Beginner-oriented extraction policy

The repository did not originally have complete Doxygen prose.

Web 7 used `EXTRACT_ALL = YES` as a bootstrap mechanism so the existing type graph could be inspected.

Web 7.1 keeps structural extraction available, but changes what the default human reference emphasizes:

```text
EXTRACT_PRIVATE       = NO
EXTRACT_PRIV_VIRTUAL  = NO
EXTRACT_LOCAL_CLASSES = NO
```

Private implementation details are therefore hidden from the primary beginner reference.

This is intentional: a beginner should first understand the contract they are expected to call, not implementation helpers they are not meant to use.

Source browsing remains available when deeper implementation inspection is needed.

## Required beginner documentation contract

For the primary architectural surface, a class/struct description must explain the role of the type.

For important public functions, documentation must explain enough of the following when applicable:

- what the function does;
- when normal application/editor code should call it;
- when it should **not** be called directly;
- parameter meaning;
- return/failure semantics;
- ownership and borrowed lifetime;
- frame lifetime;
- structural-mutation invalidation;
- blocking/asynchronous behavior;
- threading assumptions;
- normal call sequence;
- common beginner trap.

The goal is not comment volume. The goal is to remove required guesswork.

### Enforced types

Web 7.1 enforces beginner descriptions on the current core surface:

```text
noc::Engine
noc::MainLoop
noc::VirtualFileSystem
noc::ResourceManager
noc::ResourceHandle
noc::EntityHandle
noc::EntityRegistry
noc::World
noc::RenderQueue
noc::RenderSystem
noc::Dx12Renderer
noc::TransformComponent
noc::RenderableComponent
noc::CameraComponent
noc::NameComponent
nocturne::editor::EditorShellV3
nocturne::editor::EditorViewportController
nocturne::editor::EditorTheme
```

For the core classes with callable public APIs, every generated public function must have a non-empty Doxygen description.

`Website/scripts/validate-api.ps1` reads the generated XML and fails CI when this contract regresses.

## Public versus internal members

The beginner reference intentionally hides private class members.

This does **not** mean private code is unimportant or inaccessible: the repository and source browser remain the authority for implementation detail.

It means the generated learning surface no longer presents private helpers at the same visual level as APIs you are expected to use.

**Design choice (not directly from the book):** when Nocturne later defines a supported external SDK, that SDK should get its own narrower compatibility policy. Web 7.1 is an internal-engine learning/reference surface, not an ABI/API stability promise.

## Conceptual page integration

Canonical system pages author an `api_symbols` list in frontmatter.

Example:

```yaml
api_symbols: ["noc::Engine","noc::MainLoop","noc::World"]
```

The documentation synchronizer generates the visible **C++ API reference** section from that metadata.

The same symbol names are also included in the machine-readable knowledge manifest.

This preserves one authored relationship instead of maintaining separate human and AI symbol mappings.

## Validation

Web 7.1 validates that generated output contains and explains the current key symbols for:

- Runtime;
- Resources;
- Rendering;
- World & ECS;
- Editor.

Validation also rejects accidental inclusion of:

- the legacy `EditorControls` shell surface;
- vendored `d3dx12` helper code;
- stable alias redirects whose Doxygen HTML target does not exist.

The last rule matters after private/internal symbols are hidden: `index.xml` may still enumerate nested compounds that are not emitted as standalone HTML. Alias generation therefore only publishes an alias when the concrete Doxygen HTML target exists.

## Source precedence

For current behavior:

1. canonical Nocturne architecture/system/development contracts;
2. current source code and generated API structure for exact implementation detail;
3. completion/implementation reports;
4. historical Phase documentation.

The generated API cannot override a canonical architectural contract merely because an internal member currently exists.

## Book grounding

Gregory §1.7 supports reliable tooling and development pipelines as part of the engine ecosystem.

The explanations attached to the core APIs also follow the same book grounding as their canonical systems: Gregory §7.2.2 for resource management, §8.2 for the game loop, §11.2 for rendering-pipeline separation, §15.4 for the world editor, and §16.2/§16.5 for runtime object/component/reference concepts.

The exact Doxygen presentation, Beginner Guide, wording template, XML coverage gate, hidden-private policy and symbol-alias behavior are **Design choice (not directly from the book)**.

## Related documentation

- [Documentation Model](/docs/development/documentation-model/)
- [Architecture Overview](/docs/architecture/overview/)
- [Runtime](/docs/systems/runtime/)
- [Resources](/docs/systems/resources/)
- [Rendering](/docs/systems/rendering/)
- [World & ECS](/docs/systems/world-ecs/)
- [Editor](/docs/systems/editor/)
