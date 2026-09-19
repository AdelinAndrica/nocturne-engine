---
id: "noc.editor"
doc_type: "system"
canonical: true
status: "implemented"
subsystem: "Editor"
phase_introduced: 13
description: "Canonical Nocturne Editor shell, runtime boundary, viewport, selection and authoring-tool contract."
source_files: ["Apps/NocturneEditor/EditorShellV3.h","Apps/NocturneEditor/EditorShellV3.cpp","Apps/NocturneEditor/EditorTheme.h","Apps/NocturneEditor/EditorTheme.cpp","Apps/NocturneEditor/EditorViewportController.h","Apps/NocturneEditor/EditorViewportController.cpp"]
source_docs: ["Docs/nocturne_engine_architecture.md","Docs/Phase 13 — Completion Report.md","Docs/Phase 14 — Completion Report.md","Docs/Phase 15 — Completion Report.md"]
book_grounding: ["Jason Gregory — Game Engine Architecture (3rd ed.), §15.4 The Game World Editor","Jason Gregory — Game Engine Architecture (3rd ed.), §15.4.1 Typical Features of a Game World Editor"]
aliases: ["Editor","Nocturne Editor","EditorShellV3"]
deprecated_aliases: ["EditorShell","EditorControls"]
---

# Editor

## Purpose

Nocturne Editor is an authoring/development tool built **on top of the same runtime engine used by the game**.

It is not a second engine and it does not own a competing world model or frame loop.

## Runtime boundary

The locked editor/runtime rule is:

```text
Editor UI / authoring state
          |
          v
runtime World + engine services
          |
          v
renderer / resources / input / jobs
```

The editor may embed and drive an engine instance, inspect runtime data and submit authoring operations, but the runtime retains system orchestration.

## Active shell

The current native Windows shell is:

`EditorShellV3`

It is the visual and structural baseline established by Phase 13 and retained through later phases.

The historical `EditorShell` / `EditorControls` path is not the current design authority.

Current shell areas include:

- menu and toolbar;
- Scene Hierarchy;
- central Viewport;
- Inspector surface;
- Content Browser;
- Console;
- Build / Play surface;
- status bar.

The Nocturne website derives its visual token baseline from this shell, but the website remains a separate tooling product and never becomes an editor dependency.

## Viewport controller

`EditorViewportController` owns editor-only viewport behavior:

- editor camera/input integration;
- viewport picking;
- selection synchronization;
- transform gizmo interaction;
- debug overlay;
- current deterministic validation scene bridge.

The controller does not own the engine frame loop.

The runtime's render-target attachment seam is reused to bind rendering to the editor child viewport.

## Single source of truth

Phase 15 removed authoritative editor mirrors for transform/render/name state.

Current authority is:

| Data | Authority |
|---|---|
| transform | `TransformComponent` |
| mesh / bounds | `RenderableComponent` |
| name | `NameComponent` |
| camera lens | `CameraComponent` |
| selection / hover / drag-start | editor interaction state |

Gizmo operations write changes through `World`.

Hierarchy and debug presentation read runtime components.

## Editor visual system

The current editor visual vocabulary is centralized around:

- `EditorTheme`;
- `EditorShellV3`;
- `EditorIconRenderer`;
- vendored Tabler SVG icons.

The native UI uses a dark navy palette, Segoe UI Variable typography, Cascadia Mono for console text and Tabler's outline icon language.

**Design choice (not directly from the book):** these are Nocturne product-design decisions.

## Current authoring boundary

The current implemented baseline includes selection and transform-gizmo interaction against the runtime world.

Full production scene authoring remains the responsibility of the active scene-editing/reflection work and later persistence phases.

Do not interpret validation geometry or editor scaffolding as a serialized scene-authoring model.

## Invariants

- Editor does not own a second runtime loop.
- Editor does not own duplicate authoritative world/component state.
- Runtime/editor communication uses narrow engine/world seams.
- Renderer does not receive editor/Win32 concepts merely to render world data.
- UI-only selection/hover/drag state may remain editor-owned.
- Historical editor shells are not the baseline for new UI work.

## Book grounding

Gregory's world-editor discussion identifies common editor responsibilities including world visualization, navigation, selection, property editing and object-placement aids, and describes editors that integrate with or communicate with the actual runtime engine.

Nocturne's exact native Win32 shell, child-HWND renderer integration, product theme and editor/runtime bridge are **Design choice (not directly from the book)**.

## Current implementation sources

- `Apps/NocturneEditor/EditorShellV3.h/.cpp`
- `Apps/NocturneEditor/EditorTheme.h/.cpp`
- `Apps/NocturneEditor/EditorIconRenderer.h/.cpp`
- `Apps/NocturneEditor/EditorViewportController.h/.cpp`
- `Apps/NocturneEditor/main.cpp`

## Related documentation

- [World & ECS](/docs/systems/world-ecs/)
- [Rendering](/docs/systems/rendering/)
- [Runtime](/docs/systems/runtime/)
- [Website specification](/docs/development/website-specification/)
