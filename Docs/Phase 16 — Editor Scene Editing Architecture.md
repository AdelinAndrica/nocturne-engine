# Nocturne Engine — Phase 16 Editor Scene Editing + Runtime Reflection Architecture

> **Status:** ACTIVE IMPLEMENTATION CONTRACT  
> **Phase:** 16 — Editor Scene Editing + Runtime Reflection  
> **Branch:** `phase-16-editor-scene-editing`  
> **Applies with:** `Docs/Production Engineering Standard.md`  
> **Reflection contract:** `Docs/Phase 16 — Runtime Reflection Architecture Contract.md`  
> **Professional-grade contract:** `Docs/Phase 16 — Professional Grade Implementation Contract.md`  
> **Implementation checklist:** `Docs/Phase 16 — Editor Scene Editing Implementation Checklist.md`

---

## 1. Phase objective

Phase 16 has two ordered deliverables:

1. a production-grade engine-wide runtime reflection system for explicitly registered Nocturne types;
2. a real scene-authoring workflow layered over the Phase 15 runtime World.

The reflection system is implemented first. The generic Inspector, generic property commands, reflected snapshots and future Phase 17 serialization / Phase 24 scripting must consume the same schema rather than inventing parallel registries.

The editor remains a client of the runtime engine. There is one `Engine`, one `World`, one `MainLoop`, one authoritative component state and one reflection registry.

---

## 2. Primary book grounding

### Jason Gregory — Game Engine Architecture, 3rd Edition

- §15.4 — **The Game World Editor** grounds the editor as part of the gameplay/tooling foundation.
- §15.4.1 — **Typical Features of a Game World Editor** grounds world visualization and authoring workflows.
- §15.4.1.7 — **Object Placement and Alignment Aids** grounds dedicated transform manipulation in a world editor.
- §15.4.1.10 — **Rapid Iteration** grounds the requirement that frequent editing operations remain responsive.
- §16.2.1.6 — **Pure Component Models** grounds the Phase 15 component-oriented runtime that Phase 16 extends.
- §16.2.2 — **Property-Centric Architectures** grounds generic property-oriented tooling and metadata-driven access.
- §16.3 — **World Chunk Data Formats** is a future consumer boundary for Phase 17.
- §16.5 — **Object References and World Queries** grounds runtime object identity/query seams.
- §16.9 — **Scripting** is a future consumer boundary for Phase 24.

Gregory does not prescribe the exact Nocturne reflection registry, stable hash/ID scheme, freeze lifecycle or adapter ABI described below.

### Bob Nystrom — Game Programming Patterns

- **Command**, including Undo and Redo, grounds the editor command/history model.
- **Component** grounds separation of component domains rather than a monolithic scene object.
- **Data Locality** continues to ground the Phase 15 dense component storage retained by Phase 16.

### Eric Lengyel — Foundations of Game Engine Development, Volume 2

- §5.4.2 — **Transform Hierarchy** grounds parent/child transform semantics and the need for correct hierarchy propagation.

All Nocturne-specific policies below that go beyond these concepts are explicitly marked:

**Design choice (not directly from the book)**

---

## 3. Repository audit — current code state

This section records the real branch state inspected before Phase 16 implementation.

### 3.1 Runtime foundation that remains authoritative

The Phase 15 runtime is structurally suitable and is retained:

- `EntityHandle` is the one transient runtime entity identity.
- `EntityRegistry` owns liveness/generation/slot reuse.
- `ComponentStorage<T>` owns dense component lifetime and sparse lookup.
- `TransformSystem`, `RenderableSystem`, `CameraSystem`, `NameSystem` own their component domains.
- `World` owns/orchestrates entity/component runtime state.
- renderer consumption occurs through extracted `RenderQueue` data.
- structural ECS mutation is main-thread-owned.
- component pointers are borrowed and invalidated by structural mutation.

No second ECS or archetype migration is justified for Phase 16.

### 3.2 Phase 15 component metadata seam

Current files:

- `Engine/Runtime/ComponentType.h`
- `Engine/Runtime/ComponentRegistry.h/.cpp`

Current metadata exposes only:

- explicit numeric `ComponentTypeId`;
- canonical name;
- version;
- size/alignment;
- component flags.

Current `ComponentRegistry`:

- owns copied canonical names;
- rejects duplicate IDs/names;
- enumerates deterministically by explicit component type ID;
- is owned by `World::Impl`;
- has no Building/Frozen lifecycle;
- has no properties, enums, functions, containers, lifecycle ops or component operation adapters.

This is a migration seam, not the final Phase 16 authority.

### 3.2.1 Math-type discrepancy found by the audit

The Phase 16 reflection contract names `Vec2`, `Vec3`, `Vec4`, `Quat`, `Mat4` and `AABB` as Nocturne math types to integrate.

The current codebase actually defines `Vec3`, `Quat`, `Mat4` and `AABB`. `Vec2` and `Vec4` are referenced by Phase 16 documentation but are not implemented runtime types yet.

They must therefore be introduced as a bounded math/reflection dependency before claiming complete math-type reflection coverage. The architecture does not pretend those types already exist.

### 3.3 World semantic APIs

Current `World` already provides useful semantic mutation seams:

- `SetLocalTRS`;
- `SetParent`;
- `SetRenderableEnabled`;
- `SetName`;
- camera activation/lens helpers;
- add/remove/has/get for foundation components.

Gaps discovered by the audit:

- arbitrary `CameraComponent` entities do not yet have a World-level semantic `SetPerspective(entity,...)` / `SetCameraEnabled(entity,...)` API;
- renderable mesh/local-bounds property mutation is currently coupled through `SetRenderable`;
- stale/invalid parent passed to `World::SetParent` is treated as detach for Phase 14 compatibility, which is too ambiguous for Phase 16 authoring commands;
- transform authoring input does not currently reject NaN/Inf at the semantic runtime seam;
- Name validation enforces byte length but not UTF-8 validity.

These must be resolved through semantic APIs/validators rather than reflected raw-memory writes.

### 3.4 Engine ownership

Current `Engine` owns:

- memory;
- VFS/resources/assets;
- jobs/input/render;
- one `World`.

The current startup sequence initializes `World` after renderer setup. No reflection registry exists yet.

Phase 16 must insert the registry as an Engine-owned subsystem initialized after persistent memory is available and before `World::Init()`.

### 3.5 Editor authority defect to remove

The active shell is correctly `EditorShellV3`, but the Scene Hierarchy currently has two overlapping presentation models:

1. `EditorShellV3::PopulateScene_()` populates the shell tree with a small runtime summary;
2. `EditorViewportController` subclasses the same HWND and paints/handles a separate fixed hierarchy.

The active controller still contains:

- `kValidationObjectCount = 4`;
- `validationObjects_[4]`;
- `selectedIndex_`;
- `dragObjectIndex_`;
- fixed row ↔ validation index mapping;
- hard-coded hierarchy structure;
- fixed picking over validation entities.

This is validation scaffolding from Phase 14/15 and cannot remain authoring authority.

Selection is currently an integer validation-array index rather than `EntityHandle`.

### 3.6 Picking/gizmo limitations discovered

Current picking is valid for the root-only Phase 14 validation scene but is not general scene-authoring picking:

- it iterates only four validation entities;
- it transforms the ray using local TRS rather than arbitrary hierarchy world transform.

The current gizmo:

- operates in local orientation;
- mutates `World::SetLocalTRS()` continuously;
- has no command transaction;
- does not create one history entry per drag;
- capture loss clears drag state but does not restore the pre-drag value.

These are Phase 16 migration targets after Reflection Core and command/history infrastructure exist.

### 3.7 Inspector / Undo / authoring status

`EditorShellV3` currently contains:

- an Inspector placeholder;
- Undo/Redo toolbar stubs;
- New Scene stub;
- Open/Save correctly deferred to Phase 17.

No generic Inspector or authoring command stack exists yet.

### 3.8 Editor camera

The editor viewport camera is currently represented by a runtime entity with Name + Transform + Camera and is stored in `EditorViewportController::cameraObject_`.

Phase 16 keeps one World but must classify this handle as tool-owned Editor Session state so hierarchy authoring cannot delete/duplicate/reparent/save it as authored content.

### 3.9 Build and CI audit

Current Windows CI preserves the Phase 15 gates:

- Host Debug x64;
- `--phase15-tests`;
- Editor Debug x64;
- Engine/Host/Editor Development x64;
- solution Debug x64.

Phase 16 tests do not exist yet and must be added without removing Phase 15 regression coverage.

Visual Studio projects enumerate source/test files explicitly. Every new Reflection source/header/test must be added to the relevant `.vcxproj` and filter file.

Historical `EditorShell.cpp` and `EditorControls.cpp` are still compiled even though `main.cpp` uses `EditorShellV3`. They are not current runtime authority, but Phase 16 must not route new authoring behavior through them.

### 3.10 Repository hygiene findings

Tracked generated artifacts exist under:

- `Ide/VS2026/NocturneEngine/x64/Development/`

including object/build-list files. `.gitignore` already ignores generic build artifacts, so these are legacy tracked files and should be removed from version control.

A tracked `Ide/VS2026/NocturneHost/DerivedDataCache/AssetGraph.json` also exists despite `DerivedDataCache/` being ignored. Its intentional/test status must be verified before removal.

The Host Debug x64 project contains a machine-specific `NOC_CONTENT_ROOT="D:/Projects/Nocturne/Data"`. This is a repository portability defect, but it is not allowed to derail the bounded Reflection milestone. It should be corrected in a dedicated build-hygiene change when Phase 16 project files are touched.

---

## 4. Target ownership model

**Design choice (not directly from the book)**

```text
Engine
 ├── ReflectionRegistry            authoritative type schema
 ├── Resources / Assets
 ├── Input
 ├── Render
 └── World                         authoritative entity/component state
      ├── EntityRegistry
      ├── TransformSystem
      ├── RenderableSystem
      ├── CameraSystem
      └── NameSystem

Editor process
 └── EditorSession                 editor-only state
      ├── selected EntityHandle
      ├── tool-owned camera EntityHandle
      ├── active transform tool/orientation
      ├── CommandHistory
      ├── active transaction
      ├── hierarchy expansion state
      ├── scene-dirty state
      └── diagnostics
```

There is no editor-owned copy of component data.

---

## 5. Reflection module placement

**Design choice (not directly from the book)**

Reflection implementation will live under:

`Engine/Runtime/Reflection/`

Rationale:

- reflection is runtime infrastructure;
- it is engine-wide rather than Editor-owned;
- it is consumed by World/editor now and serialization/scripting later;
- introducing a new top-level architectural layer solely for Phase 16 is unnecessary.

Public reflection headers remain free of Win32/editor dependencies and STL ownership types.

---

## 6. Stable reflection identity

**Design choice (not directly from the book)**

Introduce:

- `TypeId` — 64-bit stable explicit identity;
- `PropertyId` — 64-bit stable explicit identity;
- `FunctionId` — 64-bit stable explicit identity.

Rules:

- zero is invalid;
- identity is independent of registration order;
- identity is independent of memory addresses and RTTI;
- identity is independent of property byte offset;
- duplicate ID and duplicate canonical name are rejected at schema validation.

Foundation component values retain their existing small explicit identities (1–4) inside the new TypeId domain for migration compatibility. Built-in reflection types use a separate reserved explicit range so they cannot collide with foundation component IDs.

Stable deterministic hashing helpers may be provided for declarations, but canonical engine registrations use named constants so schema identity changes are reviewable.

---

## 7. ComponentRegistry migration

**Design choice (not directly from the book)**

`ReflectionRegistry` becomes the only schema authority.

`ComponentRegistry` is retained temporarily as a compatibility facade/view:

- it does not own independent metadata;
- it does not register a parallel copy;
- `Find(ComponentTypeId)` maps to reflected component `TypeMetadata`;
- deterministic component enumeration comes from the frozen ReflectionRegistry.

This preserves Phase 15 APIs/tests while eliminating duplicate schema authority.

After Phase 16 consumers migrate, the facade can be removed in a later bounded cleanup without changing the canonical reflection schema.

---

## 8. Reflection registry lifecycle

**Design choice (not directly from the book)**

```text
Uninitialized
    ↓ Init(allocator)
Building
    ↓ Register...
Freeze / Validate
    ↓
Frozen
    ↓ Shutdown
Uninitialized
```

Rules:

- registration only in Building;
- no cross-translation-unit static initialization dependency;
- Engine controls registration order explicitly;
- Freeze performs full schema validation;
- failed Freeze never exposes a partially-valid Frozen schema;
- after Frozen, metadata is immutable;
- metadata addresses remain stable until Shutdown;
- frozen lookups/enumeration allocate zero persistent memory;
- registration after Freeze fails with diagnostics.

---

## 9. Type metadata and lifecycle

`TypeMetadata` describes:

- TypeId;
- canonical name;
- TypeKind;
- version;
- size/alignment;
- flags;
- lifecycle operations;
- property range;
- function range;
- optional enum/container/component descriptors.

Lifecycle operations cover non-trivial values:

- default construct;
- destruct;
- copy construct;
- move construct;
- copy assign;
- move assign;
- equality;
- reset/default.

Generic reflected ownership never assumes `memcpy` is valid for a non-trivial type.

---

## 10. Property model: structural metadata vs semantic mutation

Structural reflection answers:

> What property exists and what is its type?

Semantic mutation answers:

> How may this property legally change in the current runtime object?

A reflected property never gains permission to bypass runtime invariants.

### Component property access context

**Design choice (not directly from the book)**

Component-property adapters receive an explicit runtime context containing enough information to address the semantic target without retaining storage pointers across structural mutation:

- `World*` / `const World*`;
- `EntityHandle`;
- component TypeId;
- property metadata.

The adapter resolves the component on each operation.

Examples:

- Transform translation/rotation/scale setters call World/TransformSystem semantic mutation.
- Camera setters call entity-specific World camera APIs and validate lens invariants.
- Name setter calls `World::SetName`.
- hierarchy parent is not a normal editable property.
- derived matrices/world bounds/cache flags are read-only or hidden/transient.

Plain reflected structs may use object views when no World semantic invariant is involved.

---

## 11. Generic reflected values

**Design choice (not directly from the book)**

Provide three explicit value abstractions:

- const non-owning reflected value view;
- mutable non-owning reflected value view;
- allocator-owned reflected value.

Owned values carry:

- TypeId;
- storage pointer;
- size/alignment;
- allocator ownership;
- lifecycle operations.

They are suitable for undo/redo property values and transient snapshots.

No public `std::any` is introduced.

---

## 12. Component operation adapters

Each reflected component type exposes generic operations:

- Has;
- Add;
- Remove;
- const access where meaningful;
- policy/flags.

Generic component enumeration for an entity is implemented initially by deterministic traversal of reflected component types and their Has adapters.

**Design choice (not directly from the book):** no entity component bitmask/archetype migration is introduced solely for Phase 16. The reflected-component count is small, traversal is allocation-free after freeze, and this path will be measured before any indexing optimization.

---

## 13. Foundation component reflection

### NameComponent

Editable semantic property:

- `value`

Mutation uses `World::SetName`.

### TransformComponent

Editable semantic properties:

- local translation;
- local rotation;
- local scale.

Not generic editable properties:

- parent/children/sibling links;
- cached world transform;
- dirty flag.

Hierarchy is a specialized structural command.

### RenderableComponent

Editable:

- mesh;
- enabled;
- local bounds when retained as authorable policy.

Derived:

- world bounds;
- world-bounds dirty state.

Derived properties are hidden/read-only/transient as appropriate.

### CameraComponent

Editable semantic properties:

- vertical FOV;
- aspect;
- near;
- far;
- enabled.

Derived matrices are hidden/read-only/transient.

---

## 14. Function / enum / container reflection

Phase 16 implements these in Reflection Core now so later systems do not create competing registries.

Function metadata includes:

- FunctionId;
- canonical name;
- return TypeId;
- parameter metadata;
- flags;
- invocation adapter.

Invocation validates object/context, parameter count/types and return contract.

Enum metadata includes underlying type and named values.

Container metadata provides adapters for fixed arrays and dynamic sequences without exposing STL containers in public engine interfaces.

The initial foundation components do not need to use every container feature; tests provide synthetic coverage.

---

## 15. World API hardening required before reflected foundation components

**Design choice (not directly from the book)**

Add narrow semantic APIs where Reflection cannot legally use raw component writes:

- entity-specific camera perspective setter;
- entity-specific camera enabled setter;
- renderable mesh/local-bounds semantic setters;
- strict hierarchy/reparent operation that distinguishes intentional unparent from stale parent;
- transform finite-value validation policy;
- UTF-8 validation/diagnostic path for authoring names.

Compatibility helpers required by Phase 14 may remain temporarily, but Reflection adapters use the strict semantic APIs.

---

## 16. EditorSession and authored/tool-owned classification

After Reflection Core passes its gate, introduce one `EditorSession`.

Selection identity is `EntityHandle`.

Tool-owned editor camera state is recorded by the session and excluded from authored hierarchy operations.

**Design choice (not directly from the book):** an entity created from the scene editor receives `NameComponent` + `TransformComponent` by editor policy. This does not constrain generic runtime entity creation.

---

## 17. Scene Hierarchy migration

The final hierarchy has one model:

```text
World + EditorSession
        ↓
Hierarchy projection
        ↓
EditorShellV3 Scene panel
```

The Phase 14 fixed hierarchy painter/index mapping is removed as authority.

Hierarchy rows carry `EntityHandle`, not validation-array index.

Hierarchy discovery enumerates World entities and Transform parent/child relations.

The tool-owned editor camera and non-authored renderer scaffolding are not exposed as normal authored entities.

---

## 18. Command / transaction model

Grounding: Bob Nystrom — **Command / Undo and Redo**.

Every user-facing authoring mutation owned by Phase 16 passes through commands:

- create/delete/duplicate;
- rename;
- reparent/unparent;
- add/remove component;
- property edit;
- transform gizmo commit.

Generic property commands identify:

```text
EntityHandle
Component TypeId
PropertyId
Old reflected value
New reflected value
```

Apply/Undo invokes the same semantic reflected setter.

Structural operations remain specialized commands.

Transactions coalesce continuous UI interaction. Gizmo drag creates one history entry, not one per mouse move.

Cancel/capture-loss restores the pre-transaction state.

---

## 19. Transient snapshots

Delete/duplicate/undo may use in-memory reflected subtree snapshots.

Snapshots:

- enumerate components through ReflectionRegistry;
- copy reflected values through lifecycle operations;
- record hierarchy relations;
- remap runtime handles when restored;
- never assume restored entities receive the same EntityHandle;
- are not persistent file formats;
- are not Phase 17 persistent IDs.

---

## 20. Reparenting authoring semantics

Runtime `TransformSystem::SetParent()` remains hierarchy mechanism.

**Design choice (not directly from the book):** editor reparent/unparent preserves world-space pose where the resulting local TRS can be represented correctly.

The authoring command rejects and diagnoses:

- self-parent;
- cycles;
- stale child/parent;
- non-invertible parent transforms;
- results requiring unsupported shear/non-representable TRS.

The operation is atomic/undoable.

---

## 21. Generic Inspector architecture

Inspector is generic-first:

```text
EditorSession.selected
      ↓
ReflectionRegistry component enumeration
      ↓
TypeMetadata
      ↓
PropertyMetadata
      ↓
property drawer
      ↓
semantic reflected setter
      ↓
Command / Transaction
      ↓
World
```

No central per-component `if Transform / else Camera / ...` switch is accepted.

Custom UI may extend special types/components but is keyed by reflection identity and cannot redefine the canonical schema.

---

## 22. Performance and allocation policy

Frozen reflection lookup/enumeration/property read is allocation-free.

Baselines include:

Reflection:
- 100 and 1,000 reflected types;
- 10k property lookups;
- 100k lookup/read operations;
- component enumeration;
- function invocation.

Editor:
- 100 / 1k / 10k entity hierarchy projection;
- wide/deep hierarchy;
- subtree snapshot/delete/duplicate;
- command-history stress;
- Inspector traversal;
- selection update;
- gizmo transaction commit.

No arbitrary timing budget is introduced before measurements.

---

## 23. Testing / CI plan

Retain all Phase 15 tests.

Add `--phase16-tests` covering:

- Type/Property/Function IDs;
- registry lifecycle/freeze;
- duplicate/invalid schema rejection;
- lifecycle operations;
- primitive/builtin types;
- enum/struct/container reflection;
- semantic property adapters;
- generic reflected values;
- component reflection/enumeration;
- function invocation;
- foundation component schemas;
- OCP synthetic component;
- stress/performance.

Later Phase 16 test families cover EditorSession, command history, snapshots and authoring semantics without requiring a GPU where possible.

Windows CI retains Debug/Development Engine/Host/Editor and solution regression.

---

## 24. Implementation order

1. Phase 16 targeted repository/code audit — **complete**. The separate checklist item requiring an integral review of every historical phase Markdown file is not claimed complete here.
2. Architecture contract — **this document**.
3. Stable reflection IDs, kinds, flags and metadata primitives.
4. Lifecycle operations.
5. ReflectionRegistry Building/Freeze/Frozen lifecycle.
6. Built-in primitive/math reflection.
7. enum / struct / property metadata.
8. typed attributes.
9. generic reflected values.
10. semantic property access.
11. container reflection.
12. component reflection.
13. generic component enumeration.
14. function reflection/invocation.
15. migrate ComponentRegistry to ReflectionRegistry facade.
16. reflect Name/Transform/Renderable/Camera.
17. diagnostics/freeze validation.
18. OCP synthetic component test.
19. reflection stress/performance baseline.
20. generic property command proof.
21. EditorSession + EntityHandle selection.
22. CommandHistory/transactions.
23. reflected transient subtree snapshots.
24. dynamic World-backed hierarchy.
25. authoring operations.
26. generic Inspector.
27. component add/remove.
28. Local/World gizmo semantics + command transactions.
29. input/focus/diagnostics.
30. regression/performance/final reports.

---

## 25. Explicit non-goals

Not Phase 16:

- scene-file persistence;
- persistent entity identity;
- serialized fixups/migrations;
- scripting VM;
- physics;
- animation;
- audio;
- AI/navigation;
- PIE;
- hot reload;
- arbitrary reflection of unregistered C++/third-party types.

Phase 17 and Phase 24 must consume the Phase 16 ReflectionRegistry rather than create new schema authorities.

---

## 26. Audit verdict

The Phase 15 runtime foundation is retained.

Required structural migrations are bounded:

1. move schema authority from World-owned `ComponentRegistry` to Engine-owned `ReflectionRegistry`;
2. turn `ComponentRegistry` into a compatibility facade rather than a registry of independent metadata;
3. add semantic World mutation seams needed by reflected properties;
4. replace fixed validation-array hierarchy/selection with EditorSession + World projection after Reflection Core;
5. integrate all Phase 16 authoring changes through commands/transactions;
6. preserve `EditorShellV3`, the dedicated child-HWND DX12 viewport, one MainLoop, renderer extraction and Phase 13/14 visual baseline.

No rewrite of EntityRegistry, ComponentStorage, World ownership, renderer ownership or the main loop is justified by the audit.
