# Nocturne Website — Web 7.1 Beginner-Oriented C++ API Documentation Report

> **Status:** IMPLEMENTED — final validation pending on latest documentation head
>
> **Track:** Nocturne Website / C++ API
>
> **Sub-milestone:** Web 7.1 — Beginner-Oriented C++ API Documentation
>
> **Branch:** `web-docs-foundation`
>
> **Canonical contract:** `Docs/Development/C++ API Reference.md`

---

## 1. Objective

Web 7 generated a correct structural Doxygen reference, but that is not sufficient for a developer learning the engine.

Web 7.1 changes the default question from:

> What symbols exist?

to:

> What does this type/function do, should I call it, what owns it, how long does it live, and what can go wrong?

The goal is to remove required guesswork from the primary Nocturne C++ surface.

---

## 2. Book grounding

The API explanations follow the same architecture concepts as the canonical subsystem documentation.

Primary grounding:

- Jason Gregory, *Game Engine Architecture (3rd Edition)*, §7.2 / §7.2.2 — runtime resources;
- Gregory §8.2 — master game loop/frame orchestration;
- Gregory §8.6 — job/task decomposition context;
- Gregory §11.2 / §11.2.1 — rendering pipeline separation;
- Gregory §15.4 / §15.4.1 — game-world editor responsibilities;
- Gregory §16.2 / §16.5 — runtime object model, references and world queries;
- Bob Nystrom, *Game Programming Patterns* — Component and Data Locality;
- Frank Luna / Eric Lengyel — camera/projection fundamentals used by CameraComponent.

The exact Doxygen wording template, beginner guide, XML coverage gate and hidden-private presentation policy are **Design choice (not directly from the book)**.

---

## 3. Beginner documentation standard

Class/type documentation now explains, where applicable:

- responsibility;
- when to use;
- when not to use;
- ownership;
- borrowed lifetime;
- frame lifetime;
- threading assumptions;
- mutation/invalidation rules;
- normal flow;
- common beginner traps;
- related types.

Public function documentation explains, where applicable:

- operation semantics;
- parameter meaning;
- return value;
- failure behavior;
- blocking/asynchronous behavior;
- preconditions/postconditions;
- ownership transfer or lack of transfer;
- correct call order.

The goal is meaningful contract coverage, not comment-count coverage.

---

## 4. Runtime documentation

Materially expanded:

- `noc::Engine`;
- `noc::EngineConfig`;
- `noc::MainLoop`.

Examples of beginner-critical distinctions now documented:

- `Engine::Run()` vs embedded editor frame driving;
- `BeginFrame() -> Tick() -> EndFrame()`;
- `TickOnce()` is a diagnostic helper, **not** a complete frame;
- FrameArena allocations die at the next `BeginFrame()`;
- subsystem accessors return borrowed references;
- configuration is a pre-`Init()` concern.

---

## 5. World / ECS documentation

Materially expanded:

- `EntityHandle`;
- `EntityRegistry`;
- `World`;
- `TransformComponent`;
- `RenderableComponent`;
- `CameraComponent`;
- `NameComponent`.

Important beginner rules now explicit:

- `EntityHandle::IsValid()` does not mean the entity is alive;
- use `World::IsAlive()` / `EntityRegistry::IsAlive()` for liveness;
- handles own nothing;
- component pointers are borrowed;
- structural mutation may relocate dense component storage;
- local Transform TRS is authoritative;
- world transforms and render bounds are derived;
- Name is display/authoring data, not entity identity;
- RenderQueue extraction is same-frame data.

---

## 6. Resources documentation

Materially expanded:

- `VirtualFileSystem`;
- `ResourceHandle`;
- `ResourceHandleT<T>`;
- `ResourceManager`;
- typed Text/Mesh/Texture/Material resource records.

Important beginner rules now explicit:

- virtual paths are preferred over machine-specific physical paths;
- later VFS mounts have higher lookup priority;
- a valid ResourceHandle does not imply Ready;
- ResourceManager owns loaded resource storage;
- getters return borrowed pointers;
- worker work becomes publicly Ready/Failed through `ResourceManager::Update()`;
- `WaitUntilReady()` is deliberately blocking and should not be normal frame-path behavior.

Gregory §7.2.2 directly supports the resource-manager responsibilities that these comments explain.

---

## 7. Rendering documentation

Materially expanded:

- `RenderQueue`;
- `RenderSystem`;
- `Dx12Renderer`;
- frame submission structs.

Important beginner boundary:

```text
World / ECS
    -> BuildRenderQueue()
    -> RenderQueue
    -> RenderSystem
    -> Dx12Renderer
```

Normal application/editor code should generally stop at `RenderSystem`.

`Dx12Renderer` is documented as backend implementation detail rather than a convenient API to call directly.

---

## 8. Editor documentation

Materially expanded:

- `EditorShellV3`;
- `EditorViewportController`;
- `EditorTheme`.

The docs now make the authority split explicit:

```text
EditorShellV3
    = presentation/chrome

EditorViewportController
    = editor-only 3D interaction

World
    = authoritative entity/component state
```

The historical `EditorShell` / `EditorControls` path remains excluded.

---

## 9. Beginner Guide

Created:

`Docs/API/BeginnerGuide.dox`

The Doxygen landing page directs new readers there before the alphabetical class list.

Recommended order:

1. Runtime;
2. World & ECS;
3. Resources;
4. Rendering;
5. Editor.

The guide also defines Nocturne documentation vocabulary:

- owns;
- borrowed;
- same-frame;
- structural mutation.

---

## 10. Doxygen presentation policy

Web 7 originally used broad extraction as a structural bootstrap.

Web 7.1 keeps `EXTRACT_ALL = YES` for existing symbol discovery, but the human reference now hides primary private implementation detail:

```text
EXTRACT_PRIVATE       = NO
EXTRACT_PRIV_VIRTUAL  = NO
EXTRACT_LOCAL_CLASSES = NO
```

This prevents private helpers from visually competing with the APIs a beginner is expected to use.

Source browsing remains available for deeper implementation study.

---

## 11. Automated documentation quality gate

`Website/scripts/validate-api.ps1` now parses generated Doxygen XML.

For the beginner-critical classes it requires:

- a generated compound description;
- a non-empty description for every generated public function.

Enforced callable types include:

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
nocturne::editor::EditorShellV3
nocturne::editor::EditorViewportController
nocturne::editor::EditorTheme
```

Component types also require class/struct-level descriptions:

```text
noc::TransformComponent
noc::RenderableComponent
noc::CameraComponent
noc::NameComponent
```

This makes “signature-only documentation” a CI regression rather than a subjective review issue.

---

## 12. Alias-generation defect exposed by beginner extraction

After hiding private members, Doxygen's XML index could still enumerate nested/private compounds that no longer had standalone HTML pages.

The stable alias generator initially created redirects for those compounds, producing broken links.

Fixed rule:

> Publish a stable `/api-symbol/` alias only when the corresponding generated Doxygen HTML target actually exists.

This keeps the symbol index aligned with the beginner-facing generated HTML surface.

---

## 13. Verification checklist

- [x] Beginner Guide exists.
- [x] Doxygen landing page points to Beginner Guide.
- [x] runtime ownership/frame rules documented.
- [x] World/ECS liveness and pointer invalidation documented.
- [x] resource async/readiness/ownership documented.
- [x] render extraction boundary documented.
- [x] editor authority split documented.
- [x] private class members hidden from primary reference.
- [x] legacy editor shell excluded.
- [x] vendored d3dx12 excluded.
- [x] XML validator enforces key class descriptions.
- [x] XML validator enforces public-function descriptions on core callable types.
- [x] stable aliases skip compounds without HTML pages.
- [ ] final Web Quality run green on the report head.
- [ ] final API Docs run green on the report head.

---

## 14. Common pitfalls this documentation now calls out

- treating `IsValid()` as entity/resource liveness/readiness;
- retaining component pointers across structural mutation;
- retaining FrameArena-backed RenderQueue data across frames;
- calling `Dx12Renderer` directly from normal application/editor code;
- blocking every frame on `WaitUntilReady()`;
- confusing display Name with stable identity;
- creating a second editor scene model instead of editing World;
- treating `TickOnce()` as a real frame;
- freeing ResourceManager-owned data manually.

---

## 15. Scope intentionally not claimed

Web 7.1 does not claim that every internal Nocturne header is now a polished teaching chapter.

It establishes a strong beginner contract for the **architectural surface you need first** and makes regressions detectable.

Future systems should meet the same documentation standard when they become part of the primary engine-learning surface.

---

## 16. Completion statement

Web 7.1 is implemented.

Final completion requires the latest API Docs and Web Quality runs to pass on the documentation-complete head.

---

## 17. Next chat handoff

After final validation, return to:

> **Phase 16 — Editor Scene Editing**, using the newly documented C++ API as the symbol-level learning reference and the canonical system documents/books as architectural authority.
