# Nocturne Engine — Phase 15 Entity Component System Implementation Report

> **Status:** PHASE 15 IMPLEMENTATION COMPLETE  
> **Phase:** 15 — Entity / Component System  
> **Architecture contract:** `Docs/Phase 15 — Entity Component System Architecture.md`  
> **Engineering standard:** `Docs/Production Engineering Standard.md`

## 1. Scope implemented

Phase 15 replaced the old `World` parallel arrays and special-case camera with a real entity/component runtime while preserving the Phase 14 editor/rendering behavior.

Implemented foundation:

- generational `EntityHandle`;
- `EntityRegistry`;
- generic dense/sparse `ComponentStorage<T>`;
- explicit component type metadata registry;
- `TransformComponent` + hierarchy system;
- `RenderableComponent`;
- `CameraComponent`;
- `NameComponent`;
- `World` ownership/orchestration migration;
- runtime-to-render extraction from components;
- editor single-source-of-truth migration;
- Windows CI for Phase 15 tests and editor compile regression;
- stress/performance baseline;
- removal of abandoned competing runtime model.

---

## 2. Implementation sequence and commits

The implementation followed the Phase 15 checklist order.

### Entity identity / registry

Commit:

`9c207861a2fd5c311c69cc412510ac3c706df004`

Message:

`phase15: add generational entity registry foundation`

Added:

- `Engine/Runtime/Entity.h`;
- `Engine/Runtime/EntityRegistry.h/.cpp`;
- entity registry tests.

### Windows CI

Commit:

`9497ca7ce4383b771ed9aac261f27601d97efd4e`

Message:

`ci: run Phase 15 entity tests on Windows`

Added the initial Windows GitHub Actions build/test path.

### Generic component storage

Commit:

`1ff6623a91c99604bc7d766e1bf73cb277171677`

Message:

`phase15: add dense generic component storage`

Added:

- dense/sparse storage;
- alignment/lifetime-safe growth;
- swap-remove;
- generation-aware owner lookup;
- non-trivial component tests.

### Component metadata

Commit:

`7b3f915e5c94e2fd6e40dfe12062ae93082c3d24`

Message:

`phase15: add component type metadata registry`

Added:

- explicit `ComponentTypeId`;
- metadata;
- deterministic registry;
- canonical-name ownership;
- duplicate/invalid metadata tests.

### Transform foundation

Commit:

`043b712da894e70de0ede493215e927dae4f1d83`

Message:

`phase15: add transform hierarchy foundation`

Added:

- local TRS;
- cached world;
- parent/child/sibling links;
- cycle rejection;
- stackless dirty propagation;
- deterministic top-down update;
- deep hierarchy coverage.

### Renderable foundation

Commit:

`df5c01678446a951f6a21189a7500e07d67d72d2`

Message:

`phase15: add renderable component foundation`

Added:

- mesh handle;
- local/world AABB;
- enabled state;
- explicit world-bounds cache invalidation;
- dense enumeration.

### Camera foundation

Commit:

`2f082da2ea9315acd7bc1da3d5a80eed20e53766`

Message:

`phase15: add camera component foundation`

Added:

- lens state;
- derived view/projection cache;
- active camera handle;
- transform-driven camera;
- active camera lifecycle tests.

### Name foundation

Commit:

`b29bddc56e23b33a380aedb116e22c973e9291a4`

Message:

`phase15: add name component foundation`

Added:

- fixed inline UTF-8 runtime names;
- duplicate-name support;
- max length validation;
- metadata/tests.

### World migration

Commit:

`c3c3ac4fddd444d4fe8d4323d995bbbd73bf3252`

Message:

`phase15: migrate World to entity component runtime`

Replaced:

- World-owned generations/alive arrays;
- hard-coded transform array;
- hard-coded renderable array;
- special camera-follow index and camera object.

With:

- `EntityRegistry`;
- component systems;
- component metadata registry;
- component-based render extraction.

Also changed `SceneObjectHandle` into a compatibility alias of `EntityHandle`.

### Editor failure-path cleanup

Commit:

`c701e1a87ef192633e6fad057c8f7fef57647b57`

Message:

`phase15: handle editor world mutation failures`

Handled all `[[nodiscard]]` world mutation results in the Phase 14 viewport controller instead of discarding them.

### Editor single source of truth

Commit:

`716fbec3cbf0a78382d5ff78dc7063796d5971d7`

Message:

`phase15: make editor use runtime component state`

Removed editor-authoritative mirrored transform/bounds state.

Picking, gizmos, debug selection and hierarchy labels now read runtime components.

### Stress/performance + dead runtime cleanup

Commit:

`c267dfd01d2a8e25ec6881d8efcf9ef13e32f13c`

Message:

`phase15: add stress baselines and remove dead runtime model`

Added:

- 10k stress/performance baseline;
- allocator observations;
- render-extraction persistent-allocation guard.

Removed:

- `Engine/Runtime/Transform.h`;
- `Engine/Runtime/RenderPackets.h`;
- `Engine/Runtime/VisibilitySystem.h/.cpp`.

### World component query API completion

Commit:

`b3048d5b76c4f213414b6557d1fd2c0f68c567c8`

Message:

`phase15: complete World component query API`

Completed add/remove/has/get public component access for all four foundation components.

---

## 3. Files added

Runtime foundation:

- `Engine/Runtime/Entity.h`
- `Engine/Runtime/EntityRegistry.h`
- `Engine/Runtime/EntityRegistry.cpp`
- `Engine/Runtime/ComponentStorage.h`
- `Engine/Runtime/ComponentType.h`
- `Engine/Runtime/ComponentRegistry.h`
- `Engine/Runtime/ComponentRegistry.cpp`
- `Engine/Runtime/Components/TransformComponent.h`
- `Engine/Runtime/Components/RenderableComponent.h`
- `Engine/Runtime/Components/CameraComponent.h`
- `Engine/Runtime/Components/NameComponent.h`
- `Engine/Runtime/TransformSystem.h/.cpp`
- `Engine/Runtime/RenderableSystem.h/.cpp`
- `Engine/Runtime/CameraSystem.h/.cpp`
- `Engine/Runtime/NameSystem.h/.cpp`

Tests:

- `Apps/NocturneHost/Tests/phase15_entity_registry_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_component_storage_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_component_registry_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_transform_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_renderable_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_camera_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_name_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_world_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_stress_perf_tests.cpp`

CI:

- `.github/workflows/windows-ci.yml`

---

## 4. Old World defects removed

The previous World implementation had multiple structural problems:

- entity creation selected `idx = capacity` when no free slot existed, creating sparse capacity jumps instead of consuming the next unused slot;
- transforms and renderables were hard-coded parallel arrays;
- the camera was a special-case object outside normal components;
- transform parenting did not reject cycles;
- the editor mirrored mutable transform state;
- abandoned ECS-like visibility files represented a second incompatible runtime model.

Phase 15 removes these defects rather than layering another ECS API on top of them.

---

## 5. EntityRegistry behavior implemented

Validated behavior includes:

- sequential first-time slots;
- geometric growth;
- stale-handle rejection;
- generation bump on destroy;
- double-destroy rejection;
- invalid/out-of-range rejection;
- free-list reuse;
- terminal-generation slot retirement;
- deterministic entity-index inspection;
- 10k stress.

Runtime identity remains transient and is not a serialized persistent identity.

---

## 6. ComponentStorage behavior implemented

Validated behavior includes:

- dense components;
- dense owners;
- sparse lookup;
- one component per entity/type;
- duplicate-add rejection;
- remove-absent rejection;
- swap-remove lookup repair;
- growth;
- over-aligned types;
- non-trivial constructors/destructors/move construction;
- generation-aware lookup;
- leak-free shutdown.

Storage does not byte-copy non-trivial `T` during relocation.

---

## 7. Transform hierarchy implemented

Validated behavior includes:

- default local TRS;
- multi-level parent/child hierarchy;
- multiple siblings;
- local-to-world propagation;
- dirty subtree propagation;
- on-demand world update;
- detach;
- reparent;
- self-parent rejection;
- indirect cycle rejection;
- parent removal;
- local-pose-preserving child promotion;
- stale handle rejection;
- stackless deep traversal.

A 1,024-level correctness test remains in the unit suite. The performance baseline additionally measures a 2,048-level deep hierarchy.

---

## 8. Renderable implementation

Validated behavior includes:

- mesh handle;
- local bounds;
- world bounds;
- enabled/disabled state;
- world-bounds invalidation;
- translated/scaled AABB rebuild;
- storage growth;
- swap-remove;
- stale entity rejection;
- dense enumeration.

World-level render extraction consumes Transform + Renderable component state.

---

## 9. Camera implementation

Validated behavior includes:

- default lens parameters;
- valid/invalid perspective updates;
- aspect changes;
- active camera selection;
- camera without transform handling;
- transform hierarchy contribution to camera eye;
- disabled camera handling;
- active camera removal;
- stale handle rejection;
- degenerate basis rejection.

World no longer owns a special-case camera-follow index.

---

## 10. Name implementation

Validated behavior includes:

- empty name;
- rename;
- copied/owned bytes;
- exact maximum length;
- too-long rejection without mutation/truncation;
- duplicate names;
- storage growth/swap-remove;
- stale handle rejection.

The editor validation scene now uses real runtime Name components.

---

## 11. World migration

`World` now owns and initializes the entity/component runtime.

It provides:

- entity create/destroy/alive/enumeration;
- add/remove/has/get for foundation components;
- transform parenting/mutation;
- active camera integration;
- metadata lookup;
- render extraction.

Destroy cascade removes components before the entity registry invalidates the handle.

Render extraction is deterministic by entity index and writes renderer instances into the frame arena.

---

## 12. Editor migration

Before Phase 15, `EditorViewportController::ValidationObject` mirrored:

- translation;
- rotation;
- scale;
- local bounds.

These mirrors have been removed.

The editor now reads:

- `TransformComponent`;
- `RenderableComponent`;
- `NameComponent`.

Only drag-start state remains editor-local for the duration of a gizmo interaction.

This satisfies the single-source-of-truth requirement from the production engineering standard.

---

## 13. Validation scene after migration

Runtime validation entities:

- Cube_A: Name + Transform + Renderable
- Cube_B: Name + Transform + Renderable
- Cube_C: Name + Transform + Renderable
- Ground: Name + Transform + Renderable
- Main Camera: Name + Transform + Camera

Procedural sky remains renderer/editor validation scaffolding and is not an ECS-authored scene object in Phase 15.

---

## 14. Stress/performance baseline

The Phase 15 aggregate test now includes representative measured workloads.

### Entity registry

- create 10k;
- `IsAlive` 10k;
- destroy 5k;
- free-list reuse 5k.

### Component storage/query hot paths

- add 10k NameComponents;
- Has/Get lookup over 10k components;
- dense iteration over 10k components;
- remove 5k components through swap-remove;
- allocator call/byte deltas recorded around add/remove.

### Transform

- mutate 10k;
- update 10k dirty independent roots;
- update 10k roots with 1% dirty;
- update 10k-wide hierarchy;
- update 2,048-deep hierarchy.

### Render extraction

- 10k Transform + Renderable entities;
- culling disabled to isolate extraction;
- frame-arena usage recorded;
- persistent allocator calls/bytes around extraction recorded and required to remain zero.

Timing values are logged observations only. No arbitrary wall-clock threshold is used in CI.

This follows the Production Engineering Standard requirement to establish a measured baseline before sign-off while avoiding unstable hardware-dependent pass/fail budgets.

---

## 15. CI coverage

`.github/workflows/windows-ci.yml` runs on Windows.

It:

1. checks out the repository;
2. configures MSBuild;
3. builds `NocturneHost` Debug x64;
4. runs `NocturneHost.exe --phase15-tests`;
5. builds `NocturneEditor` Debug x64 as a Phase 14 regression compile target;
6. builds `NocturneEngine`, `NocturneHost` and `NocturneEditor` directly as Development x64 projects;
7. builds `Nocturne.slnx` as a Debug x64 solution regression.

The Phase 15 aggregate test executes all Phase 15 runtime unit/integration/stress suites before Engine initialization, so tests do not depend on a GPU, DX12 device, native window or content mount.

The direct-project and solution steps also validate that repository-root discovery and project references do not rely on Visual Studio setting `$(SolutionDir)` implicitly.

Final branch validation status and measured values are recorded in `Docs/Phase 15 — Test and CI Validation Report.md` and the Completion Report.

---

## 16. Removed dead/competing implementation

Removed:

- `Engine/Runtime/Transform.h`
- `Engine/Runtime/RenderPackets.h`
- `Engine/Runtime/VisibilitySystem.h`
- `Engine/Runtime/VisibilitySystem.cpp`

These files were not in the active Visual Studio project and targeted an incompatible older API.

They are not retained as an alternate world/ECS implementation.

---

## 17. Explicit ownership summary

- Engine owns World.
- World owns entity registry, component registry and foundation component systems.
- Component systems own their `ComponentStorage<T>`.
- Component storage owns component lifetime.
- Renderer owns no components.
- Editor owns no authoritative runtime transform/render/name component data.
- Runtime handles do not own entities.
- Component pointers are borrowed and invalidated by structural mutation.

---

## 18. Threading summary

**Design choice (not directly from the book):** Phase 15 structural mutations and ordinary component writes are main-thread-only.

No locks or ECS scheduler were added.

Renderer consumption remains decoupled through extracted frame data.

---

## 19. Deferred work

Deliberately deferred:

- full authoring hierarchy/Inspector/undo-redo — Phase 16;
- persistent entity identity and serialization — Phase 17;
- physics-owned component relationships — Phase 18;
- scheduler/parallel ECS processing until a measured workload justifies it;
- archetype/chunk storage until a measured workload justifies it.

No deferred item is required to replace the Phase 15 foundation before those phases can build on it.


---

## 20. Final build-system hardening

Phase 15 completion exposed and fixed two build-system weaknesses that were masked by the original CI command line.

### Direct project root independence

Commit:

`254f83b8137ce50dbbd10d5e6e9d2c33dd6571b6`

Message:

`build: make NocturneHost project root-independent`

A local direct `NocturneHost.vcxproj /t:Rebuild` initially failed because Host include/output paths depended on `$(SolutionDir)`, which is not guaranteed when MSBuild is invoked directly on a project.

The project now derives `RepoRoot` from `$(MSBuildProjectDirectory)`, matching the robust Editor pattern.

The same completion pass subsequently made `NocturneEngine.vcxproj` root-independent for Debug/Development direct builds.

### Development and solution CI

Commit:

`a1e47771b6e7b890cf2158a9ccf7df1886fc7da5`

Message:

`ci: validate Phase 15 development and solution builds`

The CI matrix was expanded to include:

- NocturneEngine Development x64;
- NocturneHost Development x64;
- NocturneEditor Development x64;
- full `Nocturne.slnx` Debug x64 build.

The first expanded solution run intentionally exposed a duplicate `NocturneEngine` project-instance/PDB conflict rather than being ignored.

### Project-reference deduplication

Commit:

`5b890be76f4b4444c29f7515a86791bad4b3b2c1`

Message:

`build: deduplicate engine project references`

Host and Editor no longer inject a distinct `SolutionDir` project-reference property into `NocturneEngine`. Since all three projects can now derive repository root themselves, this removes competing MSBuild project instances that targeted the same compiler PDB/output paths.

---

## 21. Final performance coverage addition

Commit:

`07608b3d40fd89554efdbc4a90c26915d48027c4`

Message:

`phase15: measure component storage hot paths`

The stress/performance suite was extended to measure:

- component add;
- component Has/Get;
- dense component iteration;
- component remove/swap-remove;
- allocation deltas around those operations.

This closes the performance checklist gap between generic component-storage correctness tests and measured component hot-path behavior.

Exact final CI measurements are recorded in the Test and CI Validation Report.

---

## 22. Manual Phase 14 regression

The final Windows desktop/GPU regression was performed manually after rebuilding and running the Phase 15 branch.

Validated behavior:

- EditorShellV3 launch;
- DX12 child-HWND viewport;
- validation geometry/grid/sky;
- camera navigation;
- picking;
- hierarchy/viewport selection synchronization;
- runtime NameComponent display names;
- Move/Rotate/Scale gizmos;
- selection bounds;
- viewport resize;
- camera after resize;
- no recurring frame-stutter regression;
- no observed shader-recompile-every-frame regression.

The user reported the full regression as functioning.

### Accepted editor limitation

The transform gizmo currently operates in **Local Axis** space.

This does not invalidate the Phase 15 entity/component runtime contract or the Phase 14 regression accepted for this phase.

**Design choice (not directly from the book):** Local/Global transform-coordinate-space authoring UX is deferred to Phase 16 — Editor Scene Editing and is explicitly carried in the Phase 16 handoff.

---

## 23. Final implementation state

The final validated code revision for Phase 15 is recorded in the Completion Report.

At Phase 15 handoff:

- the runtime entity/component foundation is the sole world authority;
- the editor reads/writes runtime component state rather than mirrored transform state;
- renderer consumption remains extracted data;
- negative lifecycle paths are tested;
- component/entity/transform/render hot paths have measured baselines;
- Debug/Development direct project builds and solution build are part of CI;
- future persistent identity remains deliberately deferred to Phase 17.

No remaining known issue requires replacing the Phase 15 foundation before Phase 16.
