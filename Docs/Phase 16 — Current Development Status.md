# Nocturne Engine — Phase 16 Current Development Status

> **Phase:** 16 — Editor Scene Editing + Runtime Reflection
>
> **Status:** **IN DEVELOPMENT — AUTHORING CORE IMPLEMENTED; COMPLETION HARDENING ACTIVE**
>
> **Branch:** phase-16-editor-scene-editing
>
> **Implementation baseline audited:** e71445605af987773566df687be3c2db3eb60fb2
>
> **Baseline commit:** phase16: cover editor history failure and budget semantics
>
> **Compared against:** phase-15-entity-component-system
>
> **Branch delta at audit baseline:** 72 commits ahead, 0 behind
>
> **CI at audit baseline:** Nocturne Windows CI — PASS
>
> **Date:** 2026-09-18
>
> **This is a development-status snapshot, not a Completion Report. Phase 16 is not COMPLETE.**

---

## 1. Executive status

Phase 16 is already functionally advanced. The work is no longer at the reflection-foundation or initial-editor-authoring stage.

The authoring core is implemented end-to-end across Runtime Reflection, EditorSession, EntityHandle selection, World-backed Scene Hierarchy, command history, reflected snapshots, create/rename/delete/duplicate/reparent, generic Inspector editing, component add/remove and transform gizmo history integration.

The remaining work is now limited to **completion validation and finalization**:

- manual Phase 13/14/15 editor regression;
- 15+ minute edit-session soak;
- final checklist reconciliation of manual-only gates;
- Test and CI Validation Report finalization;
- Phase 16 Completion Report only after manual gates pass;
- final Phase 17 handoff + roadmap transition.

Do not rebuild the Phase 16 core. Continue from the current implementation and close the remaining completion gates.

---

## 2. What is implemented now

| Area | Status | Current implementation |
|---|---|---|
| Runtime Reflection Core | Implemented / advanced | ReflectionRegistry, stable TypeId/PropertyId/FunctionId, Building/Frozen lifecycle, owned metadata, schema validation, attributes, enums, structs, containers, functions and generic reflected values |
| Reflection ownership | Implemented | Engine owns one authoritative ReflectionRegistry; World/editor consume it non-owning |
| ComponentRegistry migration | Implemented | Phase 15 ComponentRegistry is a read-only compatibility facade over ReflectionRegistry, not a parallel schema authority |
| Primitive/math reflection | Implemented | bool/int/uint/float/double/string, Vec2/Vec3/Vec4, Quat, Mat4 policy, AABB, EntityHandle and ResourceHandle |
| Foundation components | Implemented | Name, Transform, Renderable and Camera are reflected |
| Semantic property setters | Implemented | Inspector/commands author through runtime semantic seams instead of arbitrary raw writes |
| Reflected component operations | Implemented | generic Has/Add/Remove/Get and reflected component enumeration per entity |
| Generic reflected values | Implemented | OwnedReflectedValue with allocator-aware lifetime/alignment/copy semantics |
| OCP proof | Implemented | synthetic reflected component works without modifying central Inspector/property-command switches |
| Reflection perf baseline | Implemented / expanded | 100/1000 synthetic types; TypeId/name/property/function lookup; property read/enumeration; raw+generic function invoke; reflected component enumeration; frozen hot lookup paths checked for allocator calls |
| EditorSession | Implemented | owns selection, tool camera classification, active tool, Local/World orientation, command history and dirty state |
| Selection | Implemented | EntityHandle is the authoritative identity; no row/index identity |
| Tool editor camera | Implemented | protected from authored hierarchy/delete/duplicate/reset workflows |
| Dynamic Scene Hierarchy | Implemented | World-backed hierarchy projection; rows carry EntityHandle |
| Create Entity | Implemented | command + undo/redo; default Name + Transform; create-under-parent exists |
| Rename Entity | Implemented | inline hierarchy rename + command/history + runtime validation |
| Delete subtree | Implemented | reflected transient subtree snapshot + undo/redo |
| Duplicate subtree | Implemented | reflected snapshot/instantiate + new runtime handles + undo/redo |
| Reparent / Unparent | Implemented | hierarchy drag/drop + command + cycle checks + preserve-world semantics |
| Reparent safety | Implemented | affine inverse/TRS decomposition; singular and non-representable shear cases rejected |
| Add / Remove Component | Implemented | generic reflection-driven commands + undo/redo |
| Command History | Implemented | multi-level undo/redo, cursor, redo-tail invalidation, count/byte budgets, failure semantics |
| Gizmo → History | Implemented | one history entry per completed drag |
| Local / World gizmo | Implemented with explicit scope | World Move and World Rotate supported; Scale remains Local |
| Reflection-driven Inspector | Implemented | generic reflected component/property enumeration |
| Nested Inspector editing | Implemented | reflected nested edits for vector/math structures |
| Transform Inspector | Implemented | axis editing + quaternion/Euler presentation |
| Camera Inspector | Implemented | reflected lens/settings editing with runtime validation |
| Renderable Inspector | Implemented | reflected fields + mesh resource picker |
| Bool / enum / angle drawers | Implemented | dedicated reflected presentation/edit paths |
| Inspector scrolling | Implemented | scrollable Inspector controls |
| Asset assignment | Partial / functional | mesh resource picker works; broader preview tooling remains deferred |
| Hierarchy context menu | Implemented | Create/Create Child/Rename/Duplicate/Delete/Reparent actions |
| Keyboard shortcuts | Implemented | routed through the same authoring command pathways |
| Dirty scene | Implemented | authoring marks dirty; exit warns before discard |
| New Scene | Implemented | transient in-memory reset; tool camera retained; selection/history reset |
| Save/Open | Correctly deferred | explicit Phase 17 messaging; no fake persistence |
| Diagnostics | Implemented / improving | authoring failures surface in log/console/status UI |
| CI | Passing at audited baseline | Phase 15 + Phase 16 tests and Debug/Development/solution build regression pass |

---

## 3. Runtime Reflection state

Phase 16 introduced the runtime reflection layer under Engine/Runtime/Reflection, including:

~~~text
ReflectionIds.h
ReflectionMetadata.h
ReflectionRegistry.h/.cpp
ReflectedValue.h
ReflectionString.h
PropertyAccess.h
FunctionInvocation.h
ComponentReflection.h
BuiltinTypes.h
FoundationComponents.h/.cpp
~~~

Engine owns ReflectionRegistry before World in lifetime order. ComponentRegistry is no longer a second canonical metadata registry.

The reflection schema currently covers:

~~~text
Type
Property
Enum
Struct
Container
Function
Component
Entity Reference
Resource Reference
Attributes
Lifecycle Operations
~~~

This is the authoritative foundation used by the generic Inspector, generic property commands, reflected snapshots and later Phase 17 serialization work.

---

## 4. Authoring command state

The current authoring boundary is:

~~~text
Editor UI
  ↓
IEditorCommand / concrete command
  ↓
Reflection metadata + semantic World APIs
  ↓
World / ECS
~~~

Implemented command types:

~~~text
CreateEntityCommand
RenameEntityCommand
DeleteEntityCommand
DuplicateEntityCommand
ReparentEntityCommand
AddComponentCommand
RemoveComponentCommand
SetTransformTRSCommand
SetReflectedPropertyCommand
~~~

EditorCommandHistory already supports:

- multiple undo/redo levels;
- cursor semantics;
- redo-tail invalidation;
- failed execute not entering history;
- failed undo/redo preserving cursor consistency;
- count budget;
- approximate byte budget;
- oldest-entry eviction;
- labels/diagnostics;
- RecordExecuted for already-live interactive edits;
- deterministic clear/shutdown ownership.

The audited HEAD ends specifically in history failure/budget hardening.

---

## 5. Scene Hierarchy state

The active Scene Hierarchy is World-backed.

EditorShellV3::PopulateScene_ currently:

- enumerates World entities;
- filters tool-owned editor state;
- determines authored roots;
- traverses Transform parent/child relationships;
- builds rows that carry EntityHandle;
- preserves expand/collapse state;
- synchronizes selection through EditorSession;
- supports hierarchy drag/drop reparenting;
- uses NameComponent labels;
- falls back to runtime handle text if needed.

The old fixed validation-array model is no longer the authoring authority.

Cube_A, Cube_B, Cube_C and Ground still exist only as bootstrap viewport scene content. They are not fixed hierarchy identities.

---

## 6. Inspector state

EditorInspectorModel is generic and reflection-driven.

The base Inspector enumerates reflected components/properties rather than maintaining a central switch for Name/Transform/Renderable/Camera.

Nested editing uses:

~~~text
read reflected top-level value
→ modify nested reflected leaf
→ semantic write of complete parent value
→ SetReflectedPropertyCommand
→ history
~~~

Current Inspector support includes strings, bools, numeric values, vector/math structures, quaternion/Euler presentation, nested AABB values, enums, angle presentation, mesh resource references, read-only presentation, component add/remove and scrolling.

---

## 7. Gizmo state

The viewport authoring path uses the same EditorSession selection and command history.

Implemented:

- Move;
- Rotate;
- Scale;
- Local orientation;
- World Move;
- World Rotate;
- one command per completed drag;
- cancel/revert path;
- parent-space conversion;
- TRS decomposition validation;
- Inspector refresh after authoring.

**Design choice (not directly from the book):** Scale remains Local in Phase 16 because arbitrary world scaling can require shear that the runtime pure-TRS representation cannot represent safely.

---

## 8. CI and automated tests

At implementation baseline e71445605af987773566df687be3c2db3eb60fb2, Nocturne Windows CI passed.

Validated CI steps include:

~~~text
Build NocturneHost Debug x64          PASS
Run Phase 15 foundation tests         PASS
Run Phase 16 tests                    PASS
Build NocturneEditor Debug x64        PASS
Build Development x64 projects        PASS
Build solution Debug x64              PASS
~~~

The Phase 16 aggregate runs:

~~~text
RunPhase16ReflectionFoundationTests
RunPhase16ReflectionRegistryTests
RunPhase16ReflectionOcpTests
RunPhase16ReflectionPerfTests
RunPhase16EditorSessionTests
~~~

Automated coverage already includes reflection registry/schema validation, enum/container/function reflection, generic reflected values, foundation component reflection, OCP extension, allocation/leak checks, Inspector reads/edits, nested Vec3/AABB editing, history budgets/failure semantics, redo-tail behavior, create/rename, create-under-parent, atomic TRS commands, add/remove component, delete/restore subtree, duplicate/redo with new handles, tool-camera protection, preserve-world reparent/unparent, singular/shear rejection, stale selection and transient New Scene reset.

Green CI is necessary but does not satisfy the full completion contract by itself.

---

## 9. Milestone closure — generic Enum drawer + Hierarchy context menu

**Status: VERIFIED / CLOSED**

The generic non-flags Enum Inspector path is implemented from reflection metadata rather than a component-specific switch. The Win32 enum popup resolves reflected enum values and commits the selected canonical value through:

~~~text
Enum drawer
→ EditorInspectorModel::CommitTextEdit()
→ SetReflectedPropertyCommand
→ EditorCommandHistory
→ reflected semantic property write
~~~

Phase 16 editor-session tests now include a synthetic reflected enum component and verify:

- generic enum property discovery;
- canonical enum presentation;
- enum edit through the generic reflected command path;
- exactly one history entry for the edit;
- undo;
- redo;
- invalid enum value rejection without state mutation or history insertion.

The Scene Hierarchy context menu also reuses the existing authoring entry points:

~~~text
Create      → ExecuteCreateEntity_()      → CreateEntityCommand
Rename      → BeginRenameSelection_()
               → CommitRename_()          → RenameEntityCommand
Duplicate   → ExecuteDuplicateSelection_()→ DuplicateEntityCommand
Delete      → ExecuteDeleteSelection_()   → DeleteEntityCommand
Reparent    → ExecuteReparentEntity_()    → ReparentEntityCommand
Unparent    → ExecuteReparentEntity_()    → ReparentEntityCommand
AddComponent→ ShowAddComponentPopup_()
               → ExecuteAddComponent_()   → AddComponentCommand
~~~

Keyboard shortcuts, Actor menu actions, hierarchy drag/drop and hierarchy context-menu actions converge on the same command-backed execution helpers. No parallel direct World mutation path is introduced for these authoring operations.

---

## 10. What is still open

| Completion requirement | Current status |
|---|---|
| General Transaction / CompoundCommand system | IMPLEMENTED — deferred Begin/Append/Commit/Cancel builder over CompoundEditorCommand |
| EditorSession-owned active transient transaction | IMPLEMENTED |
| Nested transaction policy | IMPLEMENTED — nested Begin is rejected |
| Generic compound rollback semantics | IMPLEMENTED — execute/redo compensate prior children; failed undo restores already-undone suffix when possible |
| Required-component policy | IMPLEMENTED — Required is opt-in removal protection; foundation components remain non-required by Phase 16 policy; Inspector/RemoveComponentCommand precedence is tested |
| Full editor performance baseline | VERIFIED for Phase 16 logical authoring workloads — hierarchy 100/1k/10k, wide/deep, selection, create 1k, Inspector refresh, history 10k/memory, subtree 1k, reparent, gizmo preview/commit and allocation telemetry measured |
| 10k hierarchy stress gate | IMPLEMENTED |
| 10k command-history stress | IMPLEMENTED |
| 1k delete/duplicate subtree stress | IMPLEMENTED |
| Dedicated hierarchy test matrix | VERIFIED |
| Dedicated Inspector robustness matrix | VERIFIED |
| Dedicated gizmo matrix | VERIFIED |
| Real-engine function reflection proof | VERIFIED — real Vec3.Length / Vec3.Dot functions are registered in builtin runtime schema and invoked generically |
| Reflection perf contract expansion | VERIFIED — TypeId/name/property/function lookup, raw+generic invoke, property enumeration and reflected component enumeration measured |
| Explicit prefab prototype seam | VERIFIED — TransientEntityPrototype explicitly reuses reflected subtree snapshots with no prefab file/asset ID/persistent identity/serialization claims |
| Manual Phase 13/14/15 UI regression | OPEN |
| 15+ minute edit-session soak | OPEN |
| Final Phase 16 Implementation Report | IMPLEMENTED — code baseline documented; completion status still pending manual gates |
| Final Phase 16 Test and CI Validation Report | IMPLEMENTED — automated Windows CI is fully green; manual editor regression/soak remains pending |
| Phase 16 Completion Report | OPEN |
| Phase 17 Handoff | DRAFT — reusable Phase 16 boundary documented; finalize after Completion Report |
| Roadmap Phase 16 completion update | OPEN |

---

## 11. Remaining structural work in detail

### 11.1A Selection remap after history recreation — IMPLEMENTED; CI PENDING

`EditorCommandHistory` exposes a type-agnostic `LastSelectionHint()`. Create/Delete/Duplicate commands provide fresh runtime handles after operations that create or recreate authored entities. `EditorShellV3` validates current selection after Undo/Redo and applies the hint when present.

This avoids RTTI or command-type switches in UI code. Automated tests cover create execute/redo, delete undo restored-root identity, and duplicate execute/redo.

**Design choice (not directly from the book):** invalid hint means preserve current selection if it is still alive; stale destroyed handles are cleared by `EditorSession::ValidateSelection()`.

### 11.1 Transaction / CompoundCommand — IMPLEMENTED

**Design choice (not directly from the book):** EditorSession owns one deferred transaction builder. Nested Begin is rejected. Append only records child commands and does not mutate World. Commit moves the resulting CompoundEditorCommand through the same EditorCommandHistory::Execute path; Cancel discards the pending transaction.

Compound semantics:

- execute children in append order;
- undo children in reverse order;
- redo children in append order;
- execute/redo failure compensates already-applied children in reverse;
- failed undo attempts to restore children that were already undone during that attempt;
- compensation failure emits an EditorHistory error diagnostic and marks the compound rollback-failed;
- commit is rejected if history changed after Begin, preventing an interleaved raw-history mutation from silently changing transaction ordering;
- shutdown/reset cancel any deferred active transaction before history/runtime teardown.

Automated editor-session tests cover begin, append, nested-begin rejection, commit as one history entry, execution order, reverse undo, redo, cancel, execute-failure rollback and shutdown with an active deferred transaction.

The existing gizmo preview remains the contract-approved coalescing exception: preview state is transient and a completed drag records one SetTransformTRSCommand rather than one command per mouse event.

### 11.1D Create/restore allocation rollback — IMPLEMENTED; CI PENDING

Deterministic allocator-injection coverage now forces two partial-mutation risks: `CreateEntityCommand` fails after runtime entity creation but before required editor components can grow, and reflected snapshot `Instantiate()` fails after entity recreation but before component restoration can allocate. Both tests require unchanged `AliveCount`, invalid current command/snapshot identity, and no partial restored entity.

**Design choice (not directly from the book):** a switchable allocator wrapper is test-only fault injection; production allocation policy is unchanged.

### 11.1E Optional component subtree snapshot proof — IMPLEMENTED; CI PENDING

Delete/undo and duplicate subtree coverage now includes a Camera on the root and a Renderable on the child, with semantic camera lens/enabled state and renderable ResourceHandle/local bounds/enabled state verified after restore/duplicate. This proves the reflection-backed snapshot does not only handle Name + Transform.

### 11.1B Transient snapshot source-to-current remap — IMPLEMENTED; CI PENDING

`ReflectedEntitySubtreeSnapshot::CurrentEntityForSource()` exposes the source→current runtime mapping already maintained by snapshot nodes. This is transient editor bookkeeping only, never persistent identity. Automated coverage validates root and child mappings after destruction/reinstantiation.

### 11.1C Leaf operations, asset command path and create-scale coverage — IMPLEMENTED; CI PENDING

Headless coverage now explicitly tests single-leaf duplicate/delete, reflected Renderable mesh `ResourceHandle` assignment with Undo/Redo, and command-backed create workloads at 100 and 10k entities.

Resource decode/type/missing-asset validation remains in the editor ResourceManager picker path because `World::SetRenderableMesh` intentionally does not own ResourceManager knowledge. The headless asset test validates the reflected authoring/history seam rather than pretending to validate live asset loading.

### 11.2 Required-component policy — IMPLEMENTED

**Design choice (not directly from the book):** \`ComponentReflectionFlags::Required\` is an authoring removal-protection policy, not a declaration that every runtime Entity must contain that component.

Phase 16 foundation components remain non-required:

- Name may be absent; hierarchy has an EntityHandle fallback label.
- Transform may be absent for logical/non-spatial runtime entities.
- Renderable is optional.
- Camera is optional.
- editor-created scene entities still receive Name + Transform by CreateEntityCommand policy.

When a reflected component is flagged Required and is present:

- Inspector marks the component non-removable even if EditorRemovable is also present;
- RemoveComponentCommand::Init rejects the operation;
- low-level component lifecycle adapters remain available for engine teardown, snapshot restoration and controlled tests;
- undo of an Add operation may restore the previous "absent" state because undo restores pre-command state rather than acting as a new user-facing Remove action.

The synthetic reflected enum component in Phase 16 editor-session tests is also flagged Required, proving Required precedence in both Inspector presentation and RemoveComponentCommand admission.

### 11.3 Editor stress/performance + allocation discipline — VERIFIED

The production World→Hierarchy traversal is now extracted into \`EditorHierarchyModel\`, a transient projection consumed directly by \`EditorShellV3::PopulateScene_()\`. The editor stress suite uses this same implementation rather than duplicating hierarchy logic in tests.

Automated measured workloads now include:

- hierarchy rebuild at 100 entities;
- hierarchy rebuild at 1k entities;
- hierarchy rebuild at 10k entities;
- deterministic repeated hierarchy projection;
- tool-owned entity filtering;
- 1k-wide hierarchy traversal;
- 1k-deep hierarchy traversal;
- 10k small command push;
- 10k undo;
- 10k redo;
- retained history bytes after the 10k editing workload;
- 1k Inspector refreshes on an entity with Name/Transform/Renderable/Camera;
- delete 1k-node subtree;
- undo delete 1k-node subtree;
- duplicate 1k-node subtree;
- undo duplicate subtree;
- allocator leak invariant across the suite.

**Design choice (not directly from the book):** performance timings are logged as observations only. CI pass/fail is based on correctness, capacity, rollback/history semantics and allocator leak invariants until stable hardware baselines justify timing budgets.

The Phase 16 logical authoring performance matrix is now covered, including select, create 1k, isolated reparent and gizmo commit.

Allocation-discipline evidence now includes:

- Hierarchy model capacity-growth telemetry plus retained-capacity estimates;
- warmed hierarchy rebuilds asserted to perform zero further STL capacity growth;
- Inspector refresh timing plus engine-allocator call counts and retained presentation-model capacity;
- geometric command-history growth performed before runtime mutation, replacing the former reserve(size + 1) pattern;
- explicit command/snapshot ownership contracts;
- deterministic snapshot allocation-failure coverage;
- gizmo preview hot path exercised for 10k updates with zero engine-allocator calls;
- Scene Hierarchy visible-row projection cached across paint/mouse events and invalidated only by structural or expand/collapse changes, eliminating temporary visible-row vectors from the paint/input hot path.

**Design choice (not directly from the book):** STL presentation allocations are evidenced through capacity-growth/retained-capacity telemetry rather than pretending that Nocturne's DebugAlloc intercepts the CRT heap. Win32/GDI draw-object creation remains bounded, immediately released and visually regression-tested separately.

### 11.3B Error-handling policy reconciliation — VERIFIED

The Phase 16 error matrix now has explicit policy and evidence for stale selection/targets, allocation failures, duplicate/absent/required components, invalid hierarchy targets, singular/non-representable transforms, invalid names/numerics/camera lens, invalid asset/resource selection, history budget failures, compound rollback and tool-camera misuse.

Recoverable authoring errors return a result/status, preserve state, and surface user-visible Console diagnostics at the EditorShellV3 interaction boundary. Assertions are not the expected control path for routine invalid authoring input.

### 11.4 Completion test matrices

#### Hierarchy — VERIFIED / CLOSED

The hierarchy automated matrix now exercises the same \`EditorHierarchyModel\` consumed by \`EditorShellV3\` and covers:

- empty World;
- one authored root;
- many authored roots;
- duplicate display names without identity aliasing;
- deep parent/child depth projection;
- expansion-state lookup keyed by EntityHandle and used by the production shell;
- rename refresh without identity replacement;
- reparent refresh;
- cycle rejection;
- stale-row eviction after authoritative World destruction;
- delete refresh;
- undo restore with new runtime EntityHandles;
- redo delete refresh.

The editor performance suite supplies the complementary wide/deep/10k hierarchy workloads and deterministic repeated row generation. Expansion rendering/click behavior remains part of the separate manual UI regression gate, but expansion-state preservation semantics are now automated.

#### Inspector — VERIFIED / CLOSED for headless authoring model

The generic Inspector model/command matrix now covers:

- no selection;
- Name-only, Transform-only, Renderable-only and Camera-only entities;
- multi-component entities;
- add component → refresh → undo → refresh → redo → refresh;
- remove component → refresh → undo restore → refresh;
- stale entity refresh clearing presentation state;
- stale Inspector presentation after structural component removal;
- invalid numeric text;
- NaN and Inf rejection;
- overlong Name rejection through runtime semantic validation;
- invalid Camera FOV/aspect/near/far rejection;
- valid Camera edit + undo;
- component-storage churn followed by property commit/refresh, proving the model does not retain raw component pointers.

Win32 text-control focus, Enter/focus-loss commit and Escape cancel remain intentionally assigned to the separate manual UI regression/input-routing gate; they are not properties of EditorInspectorModel.

#### Gizmo — VERIFIED / CLOSED for headless transform + lifecycle core

The viewport now delegates gizmo drag state, transform previews and history commit/cancel to \`EditorGizmoDragTransaction\`, shared by production code and automated tests.

Coverage includes:

- Local Move;
- World Move;
- Local Rotate;
- World Rotate;
- Local Scale, including forced-Local policy when World orientation is requested;
- parented transforms;
- rotated parents;
- non-uniform scaled parent World Move;
- begin + cancel restoring original local TRS;
- begin + commit as one history entry;
- undo/redo;
- no-op commit producing no history entry;
- entity destruction during an active drag;
- explicit termination policy: capture/focus loss commit, Escape/tool-switch/shutdown cancel;
- tool/orientation changes invalidating an active interaction;
- controller shutdown cancelling live preview state instead of silently dropping it.

Win32 hit-testing and OS delivery of mouse/capture/focus messages remain part of manual UI regression, but production termination routing now resolves those events through the tested policy seam.

### 11.5 Allocation discipline gate — VERIFIED

The Phase 16 allocation gate now has both corrective changes and evidence:

- `EditorCommandHistory` grows command storage geometrically before mutating runtime state, preserving allocation-failure atomicity while avoiding repeated one-element reserve growth;
- `EditorHierarchyModel` owns reusable traversal scratch buffers; warmed rebuilds assert zero capacity growth;
- `EditorShellV3` caches visible hierarchy indices instead of constructing temporary vectors per paint/mouse event;
- `EditorInspectorModel` reports retained presentation capacity and perf tests report engine-allocator calls during refresh workloads;
- reflected snapshots document allocator ownership explicitly and have deterministic allocation-failure coverage;
- `EditorGizmoDragTransaction::PreviewMove` is exercised through a 10k-update hot-path workload with zero Nocturne allocator calls.

This closes the Phase 16 allocation-discipline gate without claiming that `DebugAlloc` observes unrelated CRT or Win32 internal allocations.

### 11.6A Generic direct-member component property access — IMPLEMENTED; CI PENDING

The OCP editor-consumer proof exposed a real gap: editor property contexts previously populated only `userContext`, which supported Nocturne foundation semantic adapters but not newly registered components using ordinary direct-member reflected properties.

`MakeComponentPropertyAccessContext()` now populates both direct object/mutable-object pointers from `ComponentMetadata` and the semantic World+Entity `userContext`. Inspector refresh/read, `SetReflectedPropertyCommand`, and reflected snapshot capture/restore all use the same context construction path.

The synthetic OCP component now exercises generic component enumeration, schema dump, generic Inspector visibility and generic property command Undo/Redo without central switches.

### 11.6 Reflection schema diagnostics — VERIFIED

`ReflectionRegistry::DumpSchema()` now closes the dedicated reflection-contract debug-tooling gap. It is Frozen-only, deterministic and callback-based, emits registry/type/property/attribute/enum/container/component/function/parameter records, and performs no internal heap allocation.

Automated tests verify type/property/enum, foundation component, function signature/parameter and fixed/dynamic container diagnostics, plus invalid state/writer rejection.

**Design choice (not directly from the book):** the public diagnostic seam uses a line callback rather than `std::string`/STL ownership so the runtime reflection API remains lightweight and allocation policy stays explicit.

### 11.7 Reflection function/performance gate — VERIFIED

**Design choice (not directly from the book):** the first real reflected function set is attached to the engine's \`Vec3\` schema as static math operations matching the existing free functions:

- \`Nocturne.Vec3.Length(value: Vec3) -> Float32\`;
- \`Nocturne.Vec3.Dot(a: Vec3, b: Vec3) -> Float32\`.

They are deliberately side-effect-free and are not marked ScriptVisible; Phase 24 retains authority over script exposure policy.

Automated proof covers stable FunctionId lookup, canonical-name lookup, return/parameter metadata, Static flags, generic invocation, argument-count mismatch, TypeId mismatch and allocator cleanup.

The reflection performance baseline now measures TypeId lookup, canonical-name lookup, property lookup/read/enumeration, function lookup, raw function invocation, generic validated function invocation and reflected component enumeration. Frozen lookup/enumeration/raw-invoke paths are asserted not to call the reflection allocator. Generic invocation reports its OwnedReflectedValue allocation cost separately.

### 11.8 Prefab prototype seam — VERIFIED

\`TransientEntityPrototype\` now makes the Phase 16 prototype seam explicit while reusing \`ReflectedEntitySubtreeSnapshot\` as the only reflected subtree template representation.

**Design choice (not directly from the book):** this type is editor-only and in-memory. It does not define a prefab file, asset identity, persistent entity identity, serialized entity references, migration/version compatibility, override persistence or disk format.

Automated coverage verifies:

- capture leaves the source subtree alive;
- reflection-backed Name/Transform/component state is retained;
- the same transient prototype can instantiate repeatedly;
- every instance receives fresh runtime EntityHandles;
- parented instantiation works;
- tool-owned editor camera capture is rejected;
- prototype Clear releases the transient template state.

The reusable seam for Phase 17 is therefore the reflected schema plus subtree capture/instantiate mechanics, not the transient runtime handles.

### 11.9 Build/project hygiene — VERIFIED

The active project definition now removes the historical Phase 13 \`EditorShell.cpp\` / \`EditorControls.cpp\` pair from compilation. Their source files remain only as historical provenance; \`EditorShellV3\` remains the runtime/editor authority.

Tracked generated leftovers are removed from the repository:

- four MSVC \`.obj\` files under \`Ide/VS2026/NocturneEngine/x64/Development\`;
- \`NocturneEngine.vcxproj.FileListAbsolute.txt\`;
- \`Ide/VS2026/NocturneHost/DerivedDataCache/AssetGraph.json\`.

The existing \`.gitignore\` already ignores \`DerivedDataCache/\`, object/intermediate outputs and common MSVC artifacts, so this gate removes historical tracked residue instead of adding a second ignore policy.

Windows CI on the hygiene baseline validates Host Debug, Phase 15 + Phase 16 tests, Editor Debug, all Development x64 projects and solution Debug.


### 11.10 Manual regression and soak

Before completion, validate the preserved editor baseline plus new authoring operations:

- menu/toolbar/panels/status;
- DX12 viewport;
- mouse/keyboard camera;
- picking and selection bounds;
- resize;
- create/delete/duplicate/rename;
- hierarchy reparent;
- add/remove component;
- Inspector edits;
- undo/redo;
- Local/World toggle;
- no recurring stutter;
- no log spam;
- no crash during a 15+ minute editing session.

---

### 11.11 Implementation report — IMPLEMENTED

`Docs/Phase 16 — Implementation Report.md` records the implemented runtime reflection/editor architecture, milestone chronology, ownership, threading, performance/allocation hardening, transient prototype boundary and deliberate deferrals. It explicitly does not declare Phase 16 complete.

### 11.12 Checklist evidence reconciliation pass — ACTIVE

Implementation/test evidence has now been reconciled across EditorSession ownership, tool-camera protection, EntityHandle selection, create/rename/delete/duplicate, reflected snapshots, command/history, gizmo/orientation, Name/Renderable/Camera Inspector flows, component add/remove, asset assignment, dirty/New Scene, shortcuts, threading and command-history tests.

Remaining unchecked items are intentionally limited to specific proof/policy gaps and manual validation rather than representing missing core subsystems. Manual UI fidelity/feel, Phase 13/14/15 regression, soak, Completion Report and Phase 17 transition remain completion gates.

## 12. Current development sequence

The implementation sequence completed so far is:

~~~text
Runtime Reflection Core
→ EditorSession / selection / command history
→ World-backed hierarchy
→ create / rename
→ delete / duplicate reflected snapshots
→ preserve-world reparent
→ generic reflection-driven Inspector
→ generic add/remove component commands
→ hierarchy drag/drop
→ Local/World gizmo
→ Transform Inspector
→ mesh resource picker
→ bool / angle / enum drawers
→ Inspector scrolling
→ transient New Scene
→ hierarchy context menus
→ nested reflected Inspector editing
→ nested AABB drawer
→ dirty-scene exit warning
→ create-child workflow
→ status-bar authoring diagnostics
→ history failure/budget hardening
~~~

Audited implementation baseline:

~~~text
e7144560 — phase16: cover editor history failure and budget semantics
~~~

---

## 13. Recommended next implementation order

1. Run full manual Phase 13/14/15 regression plus 15+ minute edit-session soak.
2. Reconcile every remaining unchecked checklist gate with evidence or an explicit defer.
3. Produce Phase 16 Implementation Report.
4. Produce Phase 16 Test and CI Validation Report.
5. Produce Phase 16 Completion Report only after all completion gates pass.
6. Finalize the Phase 17 handoff draft and update roadmap.

---

## 14. Completion rule

Phase 16 may be marked **COMPLETE** only after its contract/checklist gates have been satisfied and documented.

Current state:

**Authoring feature core: implemented.**

**Completion hardening/evidence: in progress.**

Phase 17 should consume this authoring/reflection foundation and add persistent scene serialization on top of it; it should not replace the Phase 16 authoring core.
