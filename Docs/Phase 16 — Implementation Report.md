# Nocturne Engine — Phase 16 Editor Scene Editing + Runtime Reflection Implementation Report

> **Status:** IMPLEMENTATION COMPLETE — MANUAL REGRESSION / FINAL COMPLETION GATES PENDING
>
> **Phase:** 16 — Editor Scene Editing + Runtime Reflection
>
> **Branch:** \`phase-16-editor-scene-editing\`
>
> **Implementation code baseline:** \`eb16b5eb\` — \`phase16: close build and repository hygiene gate\`
>
> **Date:** 2026-09-18
>
> This report documents the implemented Phase 16 architecture and code. It is not the Phase 16 Completion Report.

---

## 1. Scope implemented

Phase 16 now provides two integrated foundations:

1. engine-wide runtime reflection for explicitly registered Nocturne types;
2. World-backed editor scene authoring on top of the Phase 15 ECS.

The implemented editor authoring path includes:

- EntityHandle-based selection;
- World-backed Scene Hierarchy;
- create / create child;
- inline rename;
- delete subtree;
- duplicate subtree;
- preserve-world reparent / unparent;
- generic add/remove component;
- generic reflection-driven Inspector;
- nested reflected property editing;
- Transform / Camera / Renderable authoring;
- mesh ResourceHandle assignment;
- Local/World transform gizmo policy;
- command history and compound transactions;
- transient New Scene;
- dirty-scene warning;
- status diagnostics;
- transient reusable entity prototype seam.

Real Save/Open, persistent scene identity and serialized reference fixups remain Phase 17.

---

## 2. Primary architecture grounding

The implementation follows the project’s locked Phase 16 architecture documents and the provided books.

### Jason Gregory — Game Engine Architecture, 3rd Edition

The implementation uses the world-editor concepts from §15.4 / §15.4.1, object placement/alignment concepts from §15.4.1.7, rapid-iteration requirements from §15.4.1.10, property-centric architecture concepts from §16.2.2, and treats persistence/world formats as a later consumer boundary.

### Bob Nystrom — Game Programming Patterns

The Command pattern, including Undo/Redo, grounds the editor command/history architecture.

### Eric Lengyel — Foundations of Game Engine Development, Volume 2

§5.4.2 grounds parent/child transform hierarchy semantics.

Nocturne-specific implementation policies are explicitly labeled **Design choice (not directly from the book)** in code and architecture documentation.

---

## 3. Implementation sequence

The branch implementation sequence was incremental rather than a monolithic editor rewrite.

### Runtime Reflection foundation

Key milestones include:

- \`4f4745d3\` — stable reflection identity and metadata foundation;
- \`9b8c2bc8\` — Building/Frozen ReflectionRegistry;
- \`d6d09877\` — owned property metadata;
- \`dfe0f307\` — primitive/math reflected types;
- \`89e8a5f5\` — enum reflection;
- \`d203d6ae\` — typed attributes;
- \`56559fb7\` — OwnedReflectedValue;
- \`ad73d061\` — semantic property access;
- \`25309cf5\` — container adapters;
- \`a4c0f1cd\` — function metadata/invocation;
- \`e9b1aa77\` — reflected component operations;
- \`54720b37\` — ReflectionRegistry becomes component schema authority;
- \`e16dd246\` — Engine-owned reflection registry;
- \`aa6c7293\` — reflected semantic foundation components;
- \`68b74929\` — allocator-aware reflected strings;
- \`ce055de8\` — UTF-8 entity-name validation;
- \`825f43c4\` — OCP reflection extension proof;
- \`227cb1e9\` — reflection stress/performance baseline.

### Editor authoring core

- \`b5d03822\` — EditorSession + command history;
- \`02508f5e\` — selection routed through EditorSession;
- \`b8978f10\` — World-backed Scene Hierarchy;
- \`1bc0f671\` — create + rename commands;
- \`a7f29561\` — World-backed viewport picking;
- \`0d780ebd\` — one command per gizmo drag;
- \`d8d839b8\` — generic component edit commands;
- \`672ae8d9\` — reflected subtree delete/duplicate snapshots;
- \`3431273b\` — preserve-world reparent;
- \`6f8aac40\` / \`17214c87\` — generic reflection Inspector model/editing;
- \`feb79050\` — hierarchy drag/drop reparent;
- \`c03b279c\` — Local/World gizmo orientation;
- \`3635c091\` — Transform Inspector axis editing;
- \`be40a99d\` — reflected mesh ResourceHandle picker;
- \`d33f74cb\` / \`9af73882\` — bool/angle/enum drawers;
- \`a82097b4\` / \`ca4cc6ce\` / \`d6aa80f7\` — nested reflected editing;
- \`d2a61560\` — transient New Scene;
- \`42d4a00b\` — hierarchy authoring context menu;
- \`6769fe63\` — Create Child workflow;
- \`c69c4809\` — dirty-scene discard warning;
- \`3ee63e31\` — status-bar authoring diagnostics.

### Completion hardening

- \`e7144560\` — history failure/budget semantics;
- \`cad9f6f1\` — enum drawer/context-menu command routing verification;
- \`4aa7cd8c\` — compound transactions;
- \`1ad980b3\` — required-component authoring policy;
- \`9860f4da\` / \`e860db27\` — hierarchy/editor performance matrix;
- \`e243a1bc\` — semantic enum component property adapters;
- \`66e30ff5\` — hierarchy lifecycle matrix;
- \`972f212d\` / \`79219c79\` — Inspector robustness matrix;
- \`fd448f14\` — gizmo lifecycle matrix;
- \`4e5e46ef\` — real-engine reflected functions + reflection perf expansion;
- \`daa948ea\` — allocation-discipline gate;
- \`e1cf7745\` — transient prefab-prototype seam;
- \`eb16b5eb\` — build/repository hygiene.

---

## 4. Runtime Reflection implementation

\`Engine\` owns one authoritative \`ReflectionRegistry\` whose lifetime precedes \`World\` use and outlives World consumers.

The registry implements:

- stable \`TypeId\`;
- stable \`PropertyId\`;
- stable \`FunctionId\`;
- Uninitialized / Building / Frozen lifecycle;
- registry-owned metadata and strings;
- duplicate/schema/reference validation;
- deterministic enumeration;
- properties;
- typed attributes;
- enums;
- structs;
- fixed/dynamic containers;
- functions;
- component operation adapters;
- generic reflected values.

\`ComponentRegistry\` remains only as a compatibility facade and is no longer a competing metadata authority.

Foundation reflection covers:

- Name;
- Transform;
- Renderable;
- Camera;
- primitive numeric/bool/string types;
- Vec2 / Vec3 / Vec4;
- Quat;
- AABB;
- Mat4 policy;
- EntityHandle;
- ResourceHandle.

Semantic reflected writes reuse runtime validation instead of mutating raw component memory.

---

## 5. Real reflected functions

Phase 16 contains real engine function reflection, not synthetic tests only.

**Design choice (not directly from the book):** the first real reflected functions are side-effect-free Vec3 math operations:

- \`Nocturne.Vec3.Length(value: Vec3) -> Float32\`;
- \`Nocturne.Vec3.Dot(a: Vec3, b: Vec3) -> Float32\`.

The implementation validates function identity, canonical lookup, parameters, return type, Static semantics, generic invocation and mismatch/failure behavior.

Script exposure remains Phase 24 policy.

---

## 6. EditorSession and editor-owned state

\`EditorSession\` owns editor-session state rather than placing UI state in Engine/World:

- selected \`EntityHandle\`;
- tool-camera classification;
- active editor tool;
- Local/World transform orientation;
- \`EditorCommandHistory\`;
- active transient compound transaction;
- dirty-scene state;
- state version for editor consumers.

The session holds non-owning references to the World/reflection foundation.

Nested transactions are rejected.

---

## 7. Selection and tool-owned camera

Selection identity is \`EntityHandle\`, not row index, component pointer or validation-array index.

Selection validates liveness and excludes the tool-owned editor camera.

The editor camera:

- remains a World entity for viewport runtime reuse;
- is excluded from authored Scene Hierarchy;
- cannot be deleted/duplicated/reparented through authored workflows;
- survives transient New Scene reset;
- remains separate from authored Camera components.

**Design choice (not directly from the book):** tool ownership is an editor-session classification, not a second World.

---

## 8. Scene Hierarchy

The authoritative Scene Hierarchy is a transient projection of \`World\`.

\`EditorHierarchyModel\`:

- enumerates World entities;
- filters the tool camera;
- derives roots from transform parent relationships;
- carries \`EntityHandle\` in every authored row;
- emits depth and child state;
- supports empty, one-root, many-root, deep and wide scenes;
- survives stale handles through rebuild from authoritative World state.

\`EditorShellV3\` remains the one active shell authority.

Expansion state is keyed by EntityHandle.

The presentation caches visible hierarchy indices across paint/mouse events so it does not construct temporary visible-row vectors on every input/paint event.

---

## 9. Command/history architecture

All authored mutations converge on command-backed seams.

Implemented command types:

- \`CreateEntityCommand\`;
- \`RenameEntityCommand\`;
- \`DeleteEntityCommand\`;
- \`DuplicateEntityCommand\`;
- \`ReparentEntityCommand\`;
- \`AddComponentCommand\`;
- \`RemoveComponentCommand\`;
- \`SetTransformTRSCommand\`;
- \`SetReflectedPropertyCommand\`;
- \`CompoundEditorCommand\`.

History implements:

- multi-level Undo/Redo;
- cursor semantics;
- redo-tail invalidation;
- failed execute/undo/redo cursor safety;
- command-count budget;
- approximate byte budget;
- front eviction;
- command labels;
- adoption of already-live gizmo final state;
- geometric command-storage growth before runtime mutation.

Command ownership transfers through \`std::unique_ptr<IEditorCommand>\`.

---

## 10. Compound transactions

**Design choice (not directly from the book):** \`EditorSession\` exposes one deferred transaction builder.

Semantics:

- Begin opens one transaction;
- Append stores child commands without mutating World;
- nested Begin is rejected;
- Commit executes one \`CompoundEditorCommand\` through normal history;
- Cancel destroys the pending command group;
- failed child Execute/Redo compensates previously-applied children;
- failed compound Undo restores the already-undone suffix when possible;
- rollback failure is surfaced explicitly.

---

## 11. Reflection-backed snapshots

\`ReflectedComponentSnapshot\` captures reflected property state through lifecycle-aware \`OwnedReflectedValue\` instances.

\`ReflectedEntitySubtreeSnapshot\` captures:

- subtree topology;
- reflected component membership;
- reflected editable/serializable property values;
- source/current runtime bookkeeping.

Internal parent links are node indices, not durable EntityHandles.

Undo/Redo reconstruction creates fresh runtime handles.

Snapshot state is transient command/editor state, never scene serialization.

Allocation ownership and deterministic failure cleanup are explicit and tested.

---

## 12. Transient entity prototype seam

\`TransientEntityPrototype\` explicitly reuses \`ReflectedEntitySubtreeSnapshot\`.

It supports:

- capture without destroying source;
- repeated instantiation;
- fresh runtime handles per instance;
- parented instantiation;
- reflected component/property restoration;
- tool-camera rejection.

**Design choice (not directly from the book):** it does not define a prefab asset, file, persistent ID, serialized reference format, migration policy or compatibility guarantee.

This is the reusable in-memory seam passed to Phase 17, not the Phase 17 serialized prefab representation.

---

## 13. Create / rename / delete / duplicate / reparent

### Create

Create produces a new runtime entity and default authored foundation state.

Redo recreates semantically and does not assume handle reuse.

### Rename

Rename authors Name through the semantic runtime path and validation.

UTF-8 and length constraints are enforced.

### Delete

Delete captures the reflected subtree before destruction.

Undo reconstructs the subtree with fresh runtime handles and hierarchy.

### Duplicate

Duplicate captures the reflected subtree as a template while leaving source entities alive.

Undo removes the copy; Redo creates another fresh runtime instance.

### Reparent

Reparent rejects:

- stale entity;
- self parent;
- cycles;
- tool-owned targets;
- singular target-parent transforms;
- nonrepresentable TRS/shear cases.

Preserve-world reparent computes the desired local TRS through affine inverse + TRS decomposition.

---

## 14. Generic Inspector

\`EditorInspectorModel\` consumes reflection metadata rather than a component-schema switch.

It stores presentation IDs/copies and does not retain component pointers.

Implemented presentation/editing includes:

- Name;
- Transform;
- Renderable;
- Camera;
- generic scalar values;
- Vec axis editing;
- nested AABB;
- bool drawer;
- angle/radians-to-degrees presentation;
- generic enum popup;
- ResourceHandle mesh picker;
- add/remove component;
- scrolling.

Nested editing follows:

\`read reflected parent value -> modify reflected child copy -> commit one top-level semantic property command\`.

Camera writes reuse runtime validation.

Inspector robustness tests cover stale entities, invalid text, NaN/Inf, long names, invalid lens values, structural mutation and component-storage relocation.

---

## 15. Gizmo architecture

The production viewport uses \`EditorGizmoDragTransaction\` for live transform previews and final history ownership.

Automated coverage includes:

- Local Move;
- World Move;
- Local Rotate;
- World Rotate;
- Local Scale;
- parented entities;
- rotated parents;
- non-uniform scaled parent World Move;
- cancel;
- commit;
- Undo/Redo;
- no-op drag;
- destroyed entity during drag;
- capture/focus/tool-switch/shutdown termination policy.

**Design choice (not directly from the book):** arbitrary World scale remains unavailable in Phase 16 because it can introduce shear not representable by the engine’s pure TRS component model.

---

## 16. Dirty state and transient New Scene

Successful authoring marks the in-memory scene dirty.

Exit/New Scene discard paths warn when authoring changes would be lost.

New Scene:

- is explicitly in-memory;
- clears authored entities;
- retains the tool camera;
- clears selection;
- clears history;
- resets dirty state.

Open/Save remain explicit Phase 17 stubs; no disk I/O pretends to be scene persistence.

---

## 17. Performance and allocation discipline

The editor performance suite measures logical workloads without arbitrary CI timing thresholds.

Covered workloads include:

- hierarchy rebuild 100 / 1k / 10k;
- wide/deep hierarchy;
- selection;
- create 1k;
- history 10k push/undo/redo;
- Inspector refresh;
- delete subtree 1k;
- duplicate subtree 1k;
- reparent;
- gizmo preview/commit;
- history memory.

Allocation hardening includes:

- reusable hierarchy traversal scratch;
- warmed hierarchy rebuilds with zero further STL capacity growth;
- cached hierarchy visible-row projection;
- Inspector retained-capacity telemetry;
- engine allocator-call observations during Inspector refresh;
- geometric history capacity growth;
- explicit command/snapshot ownership;
- deterministic snapshot allocation-failure coverage;
- gizmo preview workload with zero Nocturne allocator calls.

**Design choice (not directly from the book):** timings are observations. Correctness, leaks, ownership and rollback invariants remain CI pass/fail conditions.

---

## 18. Build/repository hygiene

The active Editor project compiles \`EditorShellV3\` and no longer compiles historical \`EditorShell.cpp\` / \`EditorControls.cpp\`.

Historical files remain in the repository only for provenance.

Tracked generated residue identified by the audit was removed:

- MSVC object files;
- Visual Studio file-list output;
- tracked DerivedDataCache AssetGraph.

The existing \`.gitignore\` already blocks these generated classes.

Host Debug \`NOC_CONTENT_ROOT\` was normalized from a machine-specific absolute path to repository-relative \`Data\`.

---

## 19. Explicit ownership summary

| State | Owner |
|---|---|
| Reflection schema | Engine / ReflectionRegistry |
| Runtime entities/components | World / ECS systems |
| Selected entity | EditorSession |
| Active tool/orientation | EditorSession |
| Command history | EditorSession |
| Active compound transaction | EditorSession |
| Dirty state | EditorSession |
| Tool camera classification | EditorSession |
| Hierarchy rows | transient editor presentation model |
| Inspector presentation copies | EditorInspectorModel |
| Commands | EditorCommandHistory through unique ownership |
| Reflected property snapshot payloads | OwnedReflectedValue / command snapshot |
| Transient prototype template | TransientEntityPrototype |
| Prototype-created entities | World |

---

## 20. Threading model

Phase 16 preserves the Phase 15 main-thread structural mutation model.

Editor authoring, command Execute/Undo/Redo, hierarchy mutations, Inspector commit and gizmo preview/commit occur on the editor/main thread.

No multithreaded ECS scheduler or new component-storage locks were introduced.

---

## 21. Deliberately deferred scope

Phase 16 does not implement:

- persistent scene files;
- real Save/Load;
- persistent Entity IDs;
- serialized entity-reference fixups;
- schema migration execution;
- prefab files/assets/override persistence;
- physics;
- animation;
- audio;
- scripting/gameplay runtime;
- AI/navigation;
- PIE;
- asset preview framework;
- multithreaded ECS scheduler;
- networking;
- shipping/install pipeline.

These remain assigned to their later phases.

---

## 22. Implementation conclusion

The non-manual Phase 16 implementation is present on the branch.

The remaining completion gates are validation/documentation gates, not missing core architecture:

1. complete Windows CI validation of the final code/hygiene baseline;
2. run the preserved Phase 13/14/15 manual editor regression;
3. run the 15+ minute edit-session soak;
4. reconcile the final checklist against evidence;
5. finalize the Test and CI Validation Report;
6. issue the Completion Report only if every required gate passes;
7. finalize the Phase 17 handoff and roadmap transition.

Until those gates pass, Phase 16 remains **IN DEVELOPMENT / COMPLETION HARDENING**, not COMPLETE.
