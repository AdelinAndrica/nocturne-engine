---
id: "noc.world"
doc_type: "system"
canonical: true
status: "implemented"
subsystem: "World & ECS"
phase_introduced: 15
description: "Canonical runtime entity identity, component ownership, transform hierarchy, camera and render-extraction contract."
source_files: ["Engine/Runtime/World.h","Engine/Runtime/World.cpp","Engine/Runtime/Entity.h","Engine/Runtime/EntityRegistry.h","Engine/Runtime/ComponentStorage.h","Engine/Runtime/ComponentRegistry.h"]
source_docs: ["Docs/nocturne_engine_architecture.md","Docs/Phase 15 — Entity Component System Architecture.md","Docs/Phase 15 — Completion Report.md"]
book_grounding: ["Jason Gregory — Game Engine Architecture (3rd ed.), §16.2.1.6 Pure Component Models","Jason Gregory — Game Engine Architecture (3rd ed.), §16.2.2 Property-Centric Architectures","Jason Gregory — Game Engine Architecture (3rd ed.), §16.5 Object References and World Queries","Bob Nystrom — Game Programming Patterns, Component and Data Locality"]
aliases: ["World","ECS","Entity Component System","World & ECS"]
deprecated_aliases: []
api_symbols: ["noc::World","noc::EntityHandle","noc::EntityRegistry"]
---

# World & ECS

## Purpose

`World` is the authoritative runtime owner/orchestrator for Nocturne's entity/component state.

Phase 15 replaced the earlier temporary parallel-array world model with one production entity/component foundation.

## Entity identity

The canonical runtime identity type is `EntityHandle`.

Current identity semantics include:

- slot/index identity;
- generation;
- invalid sentinel;
- stale-handle rejection;
- slot reuse without resurrecting an old handle.

Runtime entity handles are transient process-lifetime identities.

Persistent scene identity remains serialization scope and is not represented by a raw runtime handle.

## World ownership

`World` orchestrates:

- `EntityRegistry`;
- `ComponentRegistry`;
- `TransformSystem`;
- `RenderableSystem`;
- `CameraSystem`;
- `NameSystem`.

The public world API provides entity lifecycle plus component-domain operations.

Current foundation components are:

| Component | Authority |
|---|---|
| Transform | local TRS and hierarchy; world transform is derived |
| Renderable | mesh handle, bounds and enabled state |
| Camera | lens/camera state associated with an entity |
| Name | runtime/editor display name, not entity identity |

## Component storage

Phase 15 established dense/sparse component storage with:

- dense component data;
- dense owner handles;
- sparse entity-to-dense lookup;
- one component of a given type per entity;
- duplicate-add rejection;
- safe absent-remove behavior;
- swap-remove reverse-lookup repair;
- support for non-trivial/over-aligned component lifetime.

Component storage is not exposed as mutable renderer authority.

## Transform hierarchy

Transform state uses local translation/rotation/scale as authoritative authored/runtime data.

World transforms are derived and cached.

Hierarchy rules include:

- no self-parenting;
- no indirect cycles;
- deterministic top-down propagation;
- dirty descendant propagation;
- explicit stale-handle rejection.

## Camera authority

Camera lens state belongs to `CameraComponent`.

Spatial state belongs to `TransformComponent`.

The active camera is referenced by an entity handle.

The previous special-case camera authority outside ECS was removed.

## Render extraction

`World::BuildRenderQueue()` is the renderer handoff.

It reads component state and produces frame-owned rendering data rather than allowing the renderer to mutate or own ECS storage.

This preserves a single authority for world state.

## Editor/runtime single source of truth

Editor selection, hierarchy labels, picking and gizmo operations read/write the same runtime components.

The editor may own transient presentation/interaction state, such as selection index or gizmo drag-start values, but it does not maintain a second authoritative transform/name/renderable model.

## Invariants

- A stale entity handle cannot access live components.
- Destroying an entity removes its foundation components before invalidating identity.
- Transform hierarchy cannot contain cycles.
- A disabled/removed/destroyed active camera cannot remain authoritative.
- Display name is not identity.
- Renderer sees extracted data, not mutable ECS ownership.
- Editor-authored changes flow through `World`.

## Deferred scope

Not part of the current World/ECS contract:

- persistent scene/entity identity;
- scene serialization;
- reference fixups across saved data;
- scripting ownership;
- physics body authority.

These remain assigned to later roadmap phases.

## Book grounding

Primary grounding:

- Jason Gregory — *Game Engine Architecture, 3rd Edition*, §16.2.1.6.
- Jason Gregory — *Game Engine Architecture, 3rd Edition*, §16.2.2.
- Jason Gregory — *Game Engine Architecture, 3rd Edition*, §16.5.
- Bob Nystrom — *Game Programming Patterns*, Component and Data Locality.

Nocturne's exact handle layout, component IDs, storage policy and destruction semantics are **Design choice (not directly from the book)**.

## Current implementation sources

- `Engine/Runtime/World.h/.cpp`
- `Engine/Runtime/Entity.h`
- `Engine/Runtime/EntityRegistry.h/.cpp`
- `Engine/Runtime/ComponentStorage.h`
- `Engine/Runtime/ComponentRegistry.h/.cpp`
- `Engine/Runtime/TransformSystem.*`
- `Engine/Runtime/RenderableSystem.*`
- `Engine/Runtime/CameraSystem.*`
- `Engine/Runtime/NameSystem.*`
- `Engine/Runtime/Components/`

## Related documentation

- [Runtime](/docs/systems/runtime/)
- [Rendering](/docs/systems/rendering/)
- [Editor](/docs/systems/editor/)
