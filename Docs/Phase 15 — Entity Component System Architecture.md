# Nocturne Engine — Phase 15 Entity Component System Architecture

> **Status:** IMPLEMENTED ARCHITECTURE CONTRACT  
> **Phase:** 15 — Entity / Component System  
> **Applies with:** `Docs/Production Engineering Standard.md`

## 1. Objective

Phase 15 replaces the previous hard-coded `World` object arrays and special-case camera state with a durable entity/component runtime that can support Phase 16 editor scene editing and Phase 17 serialization without replacing the foundation.

The runtime model is:

- one transient runtime identity type: `EntityHandle`;
- one `EntityRegistry` owning liveness, generations and slot reuse;
- one dense component storage per component type;
- explicit component metadata;
- foundation components:
  - `TransformComponent`;
  - `RenderableComponent`;
  - `CameraComponent`;
  - `NameComponent`;
- `World` as owner/orchestrator;
- renderer consumption through extracted frame data, not mutable ECS storage;
- editor mutation through the runtime world, not mirrored editor-owned scene state.

No archetype/chunk ECS, generic scheduler, persistence layer or full scene-authoring layer is introduced in Phase 15.

---

## 2. Primary book grounding

### Jason Gregory — Game Engine Architecture, 3rd Edition

**Section 16.2.1.6 — Pure Component Models**

Gregory describes the form in which the root game object can be removed and the logical object is represented by components linked indirectly by a shared unique identifier.

Phase 15 uses this as the conceptual basis for an entity being runtime identity plus independently stored components.

**Section 16.2.2 — Property-Centric Architectures**, especially the discussion of cache-friendly like-typed data organization

Gregory discusses layouts in which same-type data is organized contiguously instead of embedding all object state inside one heterogeneous object.

Phase 15 applies this principle through one dense storage per component type.

**Section 16.5 — Object References and World Queries**

Gregory states that game objects generally require unique IDs for distinction, lookup and targeting, including editor use, and discusses pointers and handles as runtime references.

Phase 15 uses a handle as transient runtime identity and exposes deterministic entity/component lookup through `World`.

### Bob Nystrom — Game Programming Patterns

**Component**

The Component pattern separates domains that otherwise become coupled inside a monolithic object or inheritance hierarchy.

Phase 15 separates transform, renderable, camera and name state into independent component domains.

**Data Locality**

Nystrom discusses contiguous homogeneous component storage as a common cache-friendly organization and warns that locality optimizations should follow real usage rather than dogma.

Phase 15 therefore uses dense per-type component arrays but does not introduce archetype/chunk storage without a measured need.

**Dirty Flag**

Nystrom uses scene transforms as a core example: local transforms are authoritative, world transforms are derived/cached, and parent changes invalidate descendants.

Phase 15 uses the same model for `TransformComponent`.

### Eric Lengyel — Foundations of Game Engine Development, Volume 2: Rendering

**Section 5.4.2 — Transform Hierarchy**

Lengyel describes world nodes as a transform hierarchy forming a tree, with independent objects near the root, child transforms relative to their parents, and object-to-world transforms computed top-down and stored for later use.

Phase 15 enforces a cycle-free parent tree and deterministic top-down world-transform propagation.

### Frank D. Luna — Introduction to 3D Game Programming with DirectX 12

**Camera chapter**

Luna's camera model grounds the use of camera position/orientation plus lens properties such as vertical FOV, aspect ratio, near distance and far distance.

Phase 15 stores these lens properties in `CameraComponent` and derives view/projection matrices from `TransformComponent`.

---

## 3. Runtime identity

### 3.1 EntityHandle

`EntityHandle` contains:

- slot index;
- generation.

A handle is valid only when:

1. its index is not the invalid sentinel;
2. its generation is non-zero;
3. `EntityRegistry::IsAlive()` confirms that the slot is alive and the stored generation matches.

**Design choice (not directly from the book):** Nocturne uses `{index, generation}` generational handles for runtime identity.

The generation prevents an old handle from becoming valid again when a destroyed slot is reused.

Generation overflow does not wrap back into a potentially stale identity. A slot whose generation reaches the terminal value is retired rather than reused.

### 3.2 Persistent identity boundary

Runtime `EntityHandle` values are transient process-local identity.

They must not be serialized as persistent scene identity.

**Design choice (not directly from the book):** stable persistent entity identity is explicitly deferred to Phase 17. Phase 17 must introduce a persistence-safe identity/reference representation instead of storing runtime slot index/generation.

---

## 4. EntityRegistry

`EntityRegistry` owns:

- generations;
- slot states;
- LIFO free list;
- next unused slot;
- alive count.

Its public header uses no STL types.

### Invariants

- a live slot has exactly one current generation;
- a stale generation never passes `IsAlive`;
- double destroy is rejected;
- invalid/out-of-range handles are rejected;
- destroyed reusable slots increment generation before entering the free list;
- alive count equals the number of live slots;
- deterministic slot inspection is available through `EntityAtIndex()`.

**Design choice (not directly from the book):** deterministic entity iteration is entity-index order. Dense component relocation must not change world/query/render iteration order.

---

## 5. Component storage

`ComponentStorage<T>` is a dense/sparse per-type storage.

It owns:

- contiguous dense `T` instances;
- dense owner handles;
- sparse entity-index to dense-index lookup.

### Invariants

- at most one component of type `T` per entity;
- sparse lookup is generation-aware through the stored full owner handle;
- duplicate add is rejected;
- remove-absent is rejected safely;
- swap-remove repairs the sparse lookup of the moved component;
- component constructors/destructors are honored;
- growth move-constructs non-trivial components instead of byte-copying them;
- storage alignment respects `alignof(T)`;
- shutdown destroys every live component.

Pointers/references returned from component storage are transient and may be invalidated by structural mutation.

**Design choice (not directly from the book):** Phase 15 requires component types stored by `ComponentStorage<T>` to be nothrow move constructible. This provides defined growth and swap-remove behavior without partial pool corruption.

**Design choice (not directly from the book):** Phase 15 uses dense/sparse storage instead of archetype/chunk storage. This matches the current workload and keeps the foundation small enough to measure before introducing more complex layout policy.

---

## 6. Component metadata

`ComponentTypeId` is an explicit numeric type identity.

`ComponentTypeMetadata` contains:

- type ID;
- canonical name;
- version;
- size;
- alignment;
- flags.

`ComponentRegistry` owns registered canonical names and enumerates metadata deterministically by explicit type ID.

### Foundation IDs

- Transform: `1`
- Renderable: `2`
- Camera: `3`
- Name: `4`

**Design choice (not directly from the book):** component IDs are explicit constants, never registration-order IDs, RTTI addresses or pointer-derived IDs.

This prevents startup order from changing schema identity and creates a stable seam for Phase 16/17.

Phase 15 metadata is intentionally not a full reflection or serialization system.

---

## 7. TransformComponent and TransformSystem

`TransformComponent` owns authoritative local spatial state:

- local translation;
- local rotation;
- local scale.

It caches:

- world matrix;
- dirty state.

Hierarchy links use `EntityHandle`:

- parent;
- first child;
- last child;
- previous sibling;
- next sibling.

### Hierarchy invariants

- hierarchy is a tree;
- self-parenting is rejected;
- direct and indirect cycles are rejected;
- parent and child must both have `TransformComponent`;
- hierarchy links never depend on component-storage addresses;
- dirty state propagates through descendants;
- world transforms update top-down;
- stackless traversal is used for deep hierarchies.

### Parent removal policy

When a transform is removed or a parent entity is destroyed:

- its direct children become roots;
- their local TRS is preserved;
- their world transform may therefore change;
- promoted subtrees are marked dirty.

**Design choice (not directly from the book):** Nocturne preserves local pose rather than world pose when promoting children after parent removal.

This matches the pre-Phase-15 runtime behavior and is explicitly tested.

---

## 8. RenderableComponent and RenderableSystem

`RenderableComponent` owns:

- mesh `ResourceHandle`;
- local AABB;
- enabled state.

It caches:

- world AABB;
- world-bounds dirty state.

Local bounds are authoritative component data. World bounds are derived from local bounds plus `TransformComponent` world state.

**Design choice (not directly from the book):** world bounds are cached in `RenderableComponent` and explicitly dirtied when transform state changes.

Removing a renderable or destroying its owner removes it from render extraction.

Renderable entities without a transform may exist as component state but are not render-extractable.

---

## 9. CameraComponent and CameraSystem

`CameraComponent` owns lens state:

- vertical FOV;
- aspect ratio;
- near distance;
- far distance;
- enabled state.

It caches:

- view;
- projection;
- view-projection.

Spatial state is not duplicated in `CameraComponent`. The camera position/orientation comes from the entity's `TransformComponent`.

The active camera is referenced by `EntityHandle`.

### Invariants

- a camera can exist without a transform but cannot become active or rebuild until a transform exists;
- disabled cameras cannot become active;
- removing/disabling the active camera clears active selection;
- removing the active camera's transform clears active selection;
- destroying the active entity clears active selection;
- invalid perspective parameters are rejected.

**Design choice (not directly from the book):** active camera selection is a world/runtime handle, not a special camera index or singleton object.

---

## 10. NameComponent and NameSystem

`NameComponent` is runtime/editor display metadata only.

Entity identity never depends on name.

Rules:

- empty names are valid;
- duplicate names are valid;
- maximum payload is 63 UTF-8 bytes plus terminator;
- over-limit names are rejected;
- truncation is never silent.

**Design choice (not directly from the book):** Phase 15 stores names inline in a fixed 64-byte buffer to keep the public component free of STL ownership and allocator concerns.

Phase 17 may revise serialized string representation without changing entity identity.

---

## 11. World ownership and public API

`World` remains the public runtime owner/orchestrator.

It owns:

- `EntityRegistry`;
- `ComponentRegistry`;
- `TransformSystem`;
- `RenderableSystem`;
- `CameraSystem`;
- `NameSystem`.

The public API exposes:

### Entities

- create;
- destroy;
- alive check;
- alive count;
- deterministic index inspection.

### Components

For each foundation component:

- add;
- remove;
- has;
- get.

It also exposes narrow mutation helpers required by current runtime/editor behavior.

### Compatibility seam

`SceneObjectHandle` is a type alias to `EntityHandle`.

There are not two semantically equivalent handle implementations.

`CreateObject()` is a Phase 14 compatibility helper that creates an entity and adds `TransformComponent`.

**Design choice (not directly from the book):** compatibility helpers remain temporarily so Phase 14 behavior can migrate incrementally. They must not become a second world model.

---

## 12. Destroy semantics

`World::DestroyEntity()` owns destruction cascade order.

While entity identity is still alive, it removes:

1. Camera;
2. Renderable;
3. Name;
4. Transform.

Then it destroys the entity registry slot and advances generation.

This order is explicit because individual systems reject structural operations on dead entities.

### Required result

After `DestroyEntity()` returns successfully:

- `IsAlive()` is false for the old handle;
- no foundation component remains accessible through the old handle;
- active camera is not stale;
- transform hierarchy no longer references the destroyed transform;
- later slot reuse cannot resurrect the old handle.

---

## 13. Query and iteration model

Phase 15 deliberately avoids a generic query DSL.

Supported query mechanisms are:

- entity-index enumeration through `EntityAtIndex()`;
- per-component `Has/Get`;
- dense per-type enumeration inside systems;
- explicit intersections where needed by a subsystem, such as Transform + Renderable during render extraction.

**Design choice (not directly from the book):** renderer extraction iterates deterministic entity-index order rather than dense storage order, so swap-remove cannot reorder frame submission.

No heap allocation is performed per entity query.

---

## 14. Render extraction boundary

Renderer ownership is unchanged.

The renderer does not receive mutable ECS storage.

`World::BuildRenderQueue()`:

1. updates derived transform/bounds state;
2. rebuilds active camera matrices;
3. computes frustum state;
4. iterates live entities deterministically;
5. finds entities with enabled Transform + Renderable;
6. performs culling;
7. writes renderer POD instances into `LinearArena`;
8. returns `RenderQueue`.

Frame extraction allocation comes from the frame arena. It does not allocate through the persistent ECS allocator.

This preserves the architectural rule that the renderer consumes extracted data rather than becoming an owner or alternate authority for scene components.

---

## 15. Editor/runtime single source of truth

The Phase 14 viewport controller no longer owns authoritative transform copies.

Validation objects store only:

- `EntityHandle`;
- editor-only selectability state.

Picking, gizmo rendering, debug selection and hierarchy labels read runtime components.

Gizmo drag keeps only ephemeral drag-start state:

- starting translation;
- starting rotation;
- starting scale.

Each edit writes back through `World::SetLocalTRS()`.

Validation entities now contain:

- `NameComponent`;
- `TransformComponent`;
- `RenderableComponent`.

The editor camera contains:

- `NameComponent`;
- `TransformComponent`;
- `CameraComponent`.

The editor therefore remains a client of the runtime world, not a second scene authority.

---

## 16. Threading contract

**Design choice (not directly from the book):** Phase 15 structural mutation and ordinary component writes are main-thread-only.

There is:

- no ECS scheduler;
- no storage locking;
- no concurrent structural mutation;
- no renderer access to mutable ECS stores.

This is intentional scope, not missing synchronization.

A future phase may add job-based processing only when ownership and workload justify it.

---

## 17. Allocation and relocation contract

- registry/storage capacity grows geometrically;
- growth is allocator-backed;
- component growth respects object lifetime/alignment;
- component structural mutation may relocate dense storage;
- pointers/references to components must not be retained across structural mutation;
- hot render extraction uses `LinearArena`;
- Phase 15 performance tests measure allocator calls and bytes around representative workloads.

No per-frame shader/resource compilation behavior is introduced by the ECS migration.

---

## 18. Diagnostics and invalid operations

Invalid operations return failure or null rather than silently corrupting runtime state.

Covered cases include:

- invalid handles;
- stale generation;
- out-of-range entity index;
- double destroy;
- duplicate component add;
- remove absent component;
- component mutation on dead entity;
- self-parent;
- hierarchy cycle;
- missing transform for camera activation;
- invalid camera lens parameters;
- missing active camera;
- frame-arena extraction allocation failure.

One-shot culling diagnostics remain available through `DebugRequestCullDump()`.

---

## 19. Performance policy

The Phase 15 baseline measures representative workloads rather than asserting an arbitrary time budget.

Measured categories:

- 10k entity creation;
- 10k `IsAlive` checks;
- 5k destroy/reuse;
- 10k transform component mutations;
- 10k independent dirty roots;
- 1% dirty independent roots;
- 10k-wide hierarchy propagation;
- 2,048-deep hierarchy propagation;
- 10k render extraction;
- allocator call/byte deltas;
- frame-arena usage.

**Design choice (not directly from the book):** CI fails on correctness, capacity, leaks and forbidden persistent allocations, but not on a hard timing threshold. Timing budgets should be established later from target game workloads and stable benchmarking hardware.

---

## 20. Deliberately deferred scope

Not Phase 15:

- full scene editing / Inspector / undo-redo / prefab authoring — Phase 16;
- persistent entity IDs, serialization, save/load, reference fixups — Phase 17;
- physics/collision — Phase 18;
- animation — Phase 19;
- audio — Phase 20;
- scripting/gameplay runtime — Phase 24;
- navigation/AI — Phase 25;
- full PIE — Phase 27;
- generic parallel ECS scheduler;
- archetype/chunk ECS migration without measured justification.

---

## 21. Removed competing runtime model

The abandoned incompatible runtime files were removed in Phase 15:

- `Engine/Runtime/Transform.h`;
- `Engine/Runtime/RenderPackets.h`;
- `Engine/Runtime/VisibilitySystem.h`;
- `Engine/Runtime/VisibilitySystem.cpp`.

They represented an older, incompatible entity/transform/render-packet model and were not part of the active project build.

Phase 15 keeps one runtime world/component authority.
