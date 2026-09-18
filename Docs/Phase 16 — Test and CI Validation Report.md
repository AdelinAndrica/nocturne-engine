# Nocturne Engine — Phase 16 Test and CI Validation Report

> **Status:** AUTOMATED VALIDATION PASS — MANUAL EDITOR REGRESSION / SOAK PENDING
>
> **Phase:** 16 — Editor Scene Editing + Runtime Reflection
>
> **Validated code baseline:** \`eb16b5ebba3b4b602928e485c0476d7d12276d19\`
>
> **Commit:** \`phase16: close build and repository hygiene gate\`
>
> **Windows CI run:** \`35363788827\`
>
> **CI job:** \`105661197490\`
>
> **Date:** 2026-09-18
>
> This report validates the automated Phase 16 code/build/test gates. It does not replace the required manual editor regression and 15+ minute soak.

---

## 1. Validation result

The complete Windows CI workflow for the final Phase 16 code/hygiene baseline completed successfully.

All required automated build and test steps passed:

- NocturneHost Debug x64;
- Phase 15 ECS foundation regression tests;
- Phase 16 aggregate tests;
- NocturneEditor Debug x64;
- NocturneEngine Development x64;
- NocturneHost Development x64;
- NocturneEditor Development x64;
- full \`Nocturne.slnx\` Debug x64 build.

The final job log contained:

- **0 compiler/MSBuild warning entries** matching the reviewed warning patterns;
- **0 C++/MSBuild error entries**;
- no failed build/test step.

---

## 2. CI workflow contract

The active workflow is:

\`.github/workflows/windows-ci.yml\`

It is triggered by changes under:

- \`Engine/**\`;
- \`Apps/NocturneHost/**\`;
- \`Apps/NocturneEditor/**\`;
- Engine/Host/Editor VS project trees;
- \`Nocturne.slnx\`;
- the workflow file itself.

Phase 16 tests run from the Debug x64 NocturneHost through:

\`NocturneHost.exe --phase16-tests\`

No Phase 16 automated test requires an interactive desktop.

---

## 3. Phase 15 regression gate

The Phase 15 regression suite remains enabled and passed on the final Phase 16 code baseline.

Observed PASS families include:

- EntityRegistry;
- ComponentStorage;
- ComponentRegistry compatibility facade;
- Transform hierarchy;
- Renderable component;
- Camera component;
- Name component;
- World ECS integration;
- Phase 15 stress/performance baseline.

This verifies that Phase 16 reflection/editor work did not replace the Phase 15 ECS foundation.

---

## 4. Phase 16 aggregate automated tests

The Phase 16 aggregate covers:

- Reflection foundation;
- ReflectionRegistry/schema validation;
- reflected enum/container/function metadata;
- generic reflected values/lifecycle;
- semantic reflected property access;
- foundation component reflection;
- OCP synthetic reflected component;
- reflection performance/stress;
- EditorSession;
- command history and transactions;
- scene-edit integration;
- hierarchy lifecycle;
- Inspector robustness;
- gizmo transform/lifecycle;
- transient prototype behavior;
- editor stress/performance.

The aggregate passed on \`eb16b5eb\`.

---

## 5. Runtime Reflection validation

Validated reflection behavior includes:

- stable \`TypeId\`, \`PropertyId\`, \`FunctionId\`;
- invalid identity handling;
- duplicate identity/name rejection;
- registry Building/Frozen lifecycle;
- registry-owned metadata/string lifetime;
- lifecycle operations for non-trivial and over-aligned reflected values;
- primitive/math reflection;
- enum reflection;
- container reflection;
- component operation adapters;
- semantic property read/write validation;
- real-engine reflected functions;
- generic function invocation mismatch handling;
- deterministic registration/enumeration;
- allocator/leak cleanup.

Real engine reflected-function proof includes:

- \`Nocturne.Vec3.Length\`;
- \`Nocturne.Vec3.Dot\`.

---

## 6. Reflection performance observations

**Design choice (not directly from the book):** timings are observations from GitHub Windows Debug x64 CI, not pass/fail timing budgets.

Final-baseline observations include:

| Workload | Observation |
|---|---:|
| Register/freeze 100 synthetic types | 448 µs |
| Property lookup 10k @ 100 types | 948 µs |
| Property read 100k | 734 µs |
| TypeId lookup 100k @ 100 types | 6,706 µs |
| Canonical-name lookup 10k @ 100 types | 13,271 µs |
| Property enumeration 100k @ 100 types | 7,291 µs |
| Function lookup 100k | 7,507 µs |
| Raw reflected function invoke 100k | 4,406 µs |
| Generic reflected invoke 10k | 5,135 µs |
| Register/freeze 1k synthetic types | 6,348 µs |
| TypeId lookup 100k @ 1k types | 10,759 µs |
| Canonical-name lookup 10k @ 1k types | 61,198 µs |
| Property enumeration 100k @ 1k types | 15,146 µs |
| Reflected component enumeration 100k | 127,358 µs |

Frozen hot lookup/enumeration/raw-invoke paths preserve their allocation invariants. Generic invocation intentionally reports the allocator-backed \`OwnedReflectedValue\` cost separately.

---

## 7. Editor command/history validation

Automated coverage includes:

- empty Undo/Redo;
- single and multi-command history;
- Undo/Redo chains;
- redo-tail invalidation;
- failed Execute not pushed;
- failed Undo/Redo cursor safety;
- count-budget eviction;
- byte-budget eviction;
- command destruction/ownership;
- \`RecordExecuted\` rollback behavior;
- compound command Execute/Undo/Redo;
- compound compensation after child failure;
- transaction Begin/Append/Commit/Cancel;
- nested transaction rejection;
- transaction shutdown behavior.

The history storage hardening uses geometric pre-mutation capacity growth so allocation failure cannot occur after runtime mutation solely because the history vector needs to grow.

---

## 8. Scene authoring integration validation

Automated authoring coverage includes:

- create;
- create child;
- rename;
- delete subtree;
- Undo/Redo delete;
- duplicate subtree;
- redo duplicate with fresh runtime identity;
- reparent;
- unparent;
- cycle rejection;
- singular-parent rejection;
- nonrepresentable TRS/shear rejection;
- add/remove component;
- component state restoration;
- stale selection;
- tool-camera protection;
- transient New Scene reset.

Selection identity is \`EntityHandle\`; no row index is authoritative entity identity.

---

## 9. Hierarchy validation

Automated hierarchy coverage includes:

- empty World;
- one root;
- many roots;
- duplicate names;
- deep hierarchy;
- wide hierarchy;
- expansion-state identity;
- rename refresh;
- reparent refresh;
- cycle rejection;
- stale-row eviction;
- delete refresh;
- Undo restore with new runtime handles;
- redo delete;
- deterministic repeated projection;
- 10k entity workload.

Hierarchy presentation hot-path work was also audited so visible-row indices are cached rather than rebuilt into temporary vectors on every paint/input event.

---

## 10. Inspector validation

Automated Inspector coverage includes:

- no selection;
- Name-only;
- Transform-only;
- Renderable-only;
- Camera-only;
- multiple components;
- nested vector/AABB editing;
- add/remove component refresh;
- component add/remove Undo/Redo;
- stale entity;
- structural component mutation while the Inspector model exists;
- invalid numeric text;
- NaN/Inf rejection;
- overlong Name rejection;
- invalid Camera lens rejection;
- valid Camera edit + Undo;
- component-storage relocation without retained raw component pointers.

Win32 focus/Enter/Escape control behavior remains part of the separate manual editor protocol.

---

## 11. Gizmo validation

Production and tests share \`EditorGizmoDragTransaction\`.

Automated coverage includes:

- Local Move;
- World Move;
- Local Rotate;
- World Rotate;
- Local Scale;
- parented transform;
- rotated parent;
- non-uniform scaled parent World Move;
- begin/cancel;
- begin/commit;
- Undo/Redo;
- no-op drag;
- destroyed target during drag;
- tool/orientation switch;
- capture/focus termination policy;
- shutdown cancellation policy.

**Design choice (not directly from the book):** arbitrary World Scale is not exposed in Phase 16 because shear may not be representable by the current pure-TRS TransformComponent.

---

## 12. Transient prototype validation

\`TransientEntityPrototype\` is validated as an in-memory reusable seam over \`ReflectedEntitySubtreeSnapshot\`.

Tests verify:

- capture leaves source entities alive;
- reflected subtree state is retained;
- repeated instantiation succeeds;
- every instance receives fresh runtime handles;
- parented instantiation works;
- tool-owned editor camera capture is rejected;
- Clear releases the template state.

No prefab file, persistent prefab identity, serialized reference or compatibility guarantee is introduced in Phase 16.

---

## 13. Editor performance observations

Final Windows Debug x64 observations:

| Workload | Observation |
|---|---:|
| Hierarchy rebuild 100 | 187 µs |
| Hierarchy rebuild 1k | 1,329 µs |
| Hierarchy rebuild 10k | 13,170 µs |
| Hierarchy wide 1k | 1,450 µs |
| Hierarchy deep 1k | 1,625 µs |
| History 10k push | 5,104 µs |
| History 10k Undo | 518 µs |
| History 10k Redo | 546 µs |
| Inspector refresh x1000 | 71,771 µs |
| Create 1k | 1,778 µs |
| Selection x100k | 6,560 µs |
| Reparent commit | 29 µs |
| Gizmo preview x10k | 81,230 µs |
| Gizmo commit | 16 µs |
| Delete subtree 1k | 11,162 µs |
| Undo delete subtree 1k | 5,321 µs |
| Duplicate subtree 1k | 15,449 µs |
| Undo duplicate subtree 1k | 975 µs |

These values are diagnostic observations, not hard CI time budgets.

---

## 14. Allocation/leak validation

Automated allocation evidence includes:

- ReflectionRegistry shutdown/leak checks;
- OwnedReflectedValue lifetime cleanup;
- reflected snapshot failure cleanup;
- hierarchy retained-capacity telemetry;
- warmed hierarchy rebuild with zero further STL capacity growth;
- Inspector engine-allocator-call telemetry;
- history approximate memory accounting;
- gizmo preview 10k with **0 Nocturne allocator calls**;
- final aggregate allocator outstanding-byte checks.

The final editor stress/performance suite reports PASS.

---

## 15. Build/repository hygiene validation

The final code baseline also verifies the Phase 16 hygiene gate:

- historical \`EditorShell.cpp\` / \`EditorControls.cpp\` are no longer compiled by the active Editor project;
- \`EditorShellV3\` remains the active shell authority;
- machine-specific Host Debug content-root definition is replaced with repository-relative \`Data\`;
- tracked MSVC object/file-list residue is removed;
- tracked DerivedDataCache residue is removed;
- generated output classes remain covered by \`.gitignore\`;
- direct Debug/Development project builds pass;
- full solution Debug build passes.

---

## 16. Compiler diagnostics

The final CI job log was reviewed for standard MSVC/MSBuild warning and error patterns.

Result:

- warnings: **0**;
- build errors: **0**.

No new compiler warning is being accepted silently by this report.

---

## 17. Manual validation still required

Automated CI cannot prove real desktop/UI behavior.

The required manual gate is defined in:

\`Docs/Phase 16 — Manual Editor Regression and Soak Protocol.md\`

It still requires:

- Phase 13 visual baseline regression;
- Phase 14 viewport/camera/picking regression;
- Phase 16 hierarchy/Inspector/gizmo/input authoring regression;
- dirty/New Scene/exit regression;
- at least 15 minutes of active editor soak;
- no crash/hang/stuck capture/recurring stutter/log spam.

These remain **OPEN**.

---

## 18. Validation conclusion

The Phase 16 **automated code/build/test/CI gates PASS** on \`eb16b5eb\`.

The implementation is ready for the manual editor regression and soak gate.

Phase 16 is **not yet COMPLETE** because the manual completion gate has not been executed and the final Completion Report has therefore not been issued.
