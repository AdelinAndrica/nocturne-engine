# Nocturne Engine — Phase 15 Test and CI Validation Report

> **Status: COMPLETE**
>
> **Phase:** 15 — Entity / Component System  
> **Final validated code revision:** `5b890be76f4b4444c29f7515a86791bad4b3b2c1`  
> **Final comprehensive CI run:** `35292726512` — SUCCESS  
> **Performance sample run:** `35292493825` — Phase 15 test step PASS  
> **Manual editor regression:** PASS

## 1. Purpose

This report records the validation evidence used to close Phase 15.

It separates:

- isolated/unit-style foundation tests;
- World integration tests;
- stress/performance baselines;
- compiler/build validation;
- CI regression coverage;
- manual Phase 14 editor/viewport regression.

Timing measurements are observations, not shipping budgets.

---

## 2. Phase 15 test entry points

Aggregate:

`NocturneHost.exe --phase15-tests`

Focused switches:

- `--phase15-entity-tests`
- `--phase15-component-tests`
- `--phase15-component-registry-tests`
- `--phase15-transform-tests`
- `--phase15-renderable-tests`
- `--phase15-camera-tests`
- `--phase15-name-tests`
- `--phase15-world-tests`
- `--phase15-stress-perf`

The aggregate suite executes before normal engine startup and therefore does not require creation of a DX12 device or editor window.

---

## 3. Test source files

- `Apps/NocturneHost/Tests/phase15_entity_registry_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_component_storage_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_component_registry_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_transform_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_renderable_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_camera_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_name_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_world_tests.cpp`
- `Apps/NocturneHost/Tests/phase15_stress_perf_tests.cpp`

---

## 4. EntityRegistry validation

Coverage includes:

- initialization/shutdown;
- invalid default handle;
- entity creation;
- deterministic initial slot allocation;
- capacity growth;
- `IsAlive`;
- destroy;
- double destroy;
- invalid/out-of-range handles;
- stale generation rejection;
- free-list reuse;
- generation advancement;
- terminal-generation slot retirement policy;
- 10k entity stress;
- leak-free shutdown.

Result: **PASS**

---

## 5. ComponentStorage validation

Coverage includes:

- add/emplace;
- Has/Get/TryGet;
- duplicate add rejection;
- absent remove rejection;
- dense storage;
- sparse lookup;
- owner mapping;
- swap-remove;
- sparse lookup repair after compaction;
- capacity growth;
- over-aligned component storage;
- non-trivial constructor/move/destructor behavior;
- different-generation rejection;
- pointer/reference invalidation contract under structural mutation;
- leak-free shutdown.

Result: **PASS**

---

## 6. Component metadata validation

Coverage includes:

- explicit ComponentTypeId;
- canonical name;
- version;
- size/alignment;
- flags;
- canonical-name ownership;
- duplicate type ID rejection;
- duplicate canonical-name rejection;
- invalid metadata rejection;
- deterministic enumeration independent of registration order.

Result: **PASS**

---

## 7. Transform validation

Coverage includes:

- default local TRS;
- local mutation;
- cached world matrix;
- parent/child hierarchy;
- multiple siblings;
- deterministic sibling order;
- dirty propagation;
- top-down world update;
- on-demand world update;
- reparent;
- detach;
- self-parent rejection;
- indirect cycle rejection;
- stale handles;
- parent removal;
- child promotion to roots;
- local-TRS preservation policy;
- 1,024-level correctness hierarchy;
- stackless traversal.

Stress/performance adds:

- 10k independent transforms;
- 1% dirty root workload;
- 10k-wide hierarchy;
- 2,048-deep hierarchy.

Result: **PASS**

---

## 8. Renderable validation

Coverage includes:

- defaults;
- mesh handle;
- local bounds;
- derived world bounds;
- dirty bounds cache;
- translation;
- non-uniform scale;
- enabled/disabled state;
- growth;
- swap-remove;
- stale handle rejection;
- dense enumeration;
- component-based render extraction through World.

Result: **PASS**

---

## 9. Camera validation

Coverage includes:

- default lens;
- FOV/aspect/near/far validation;
- camera with missing Transform;
- active camera;
- disable/re-enable;
- active camera removal;
- parented camera transform;
- view/projection rebuild;
- degenerate basis rejection;
- stale handle rejection;
- dense enumeration.

Result: **PASS**

---

## 10. Name validation

Coverage includes:

- empty names;
- copied/owned bytes;
- rename;
- duplicate names;
- exact maximum payload;
- over-limit rejection without truncation;
- storage growth;
- swap-remove;
- stale handles;
- metadata.

Result: **PASS**

---

## 11. World integration validation

Coverage includes:

- World startup/shutdown;
- registration of all foundation component metadata;
- generic entity with no implicit components;
- Phase 14 compatibility object with Transform;
- add/remove/has/get component APIs;
- transform hierarchy through World;
- Name ownership;
- Renderable extraction;
- CameraComponent activation through compatibility path;
- entity destruction cascade;
- parent destruction semantics;
- stale handle behavior after reuse;
- active camera cleanup;
- deterministic entity inspection;
- frame-arena render extraction;
- leak-free shutdown.

Result: **PASS**

---

## 12. Stress/performance workload

The final Phase 15 workload includes:

- 10k entity create;
- 10k IsAlive;
- 5k entity destroy;
- 5k free-list reuse;
- 10k component add;
- 10k component Has/Get;
- 10k dense component iteration;
- 5k component remove;
- 10k transform mutation;
- 10k all-dirty independent roots;
- 10k roots with 1% dirty;
- 10k-wide hierarchy;
- 2,048-deep hierarchy;
- 10k render extraction;
- persistent allocator deltas;
- frame-arena usage.

### Measured Debug x64 sample

Source: final GitHub Actions run `35292726512`, Phase 15 foundation-test step.

| Workload | Items | Time | Allocator calls | Allocated bytes |
|---|---:|---:|---:|---:|
| entity_create_10k | 10,000 | 905 µs | 24 | 293,760 |
| entity_is_alive_10k | 10,000 | 174 µs | 0 | 0 |
| entity_destroy_5k | 5,000 | 154 µs | 0 | 0 |
| entity_reuse_5k | 5,000 | 89 µs | 0 | 0 |
| component_add_name_10k | 10,000 | 1,628 µs | 24 | 2,480,640 |
| component_has_get_name_10k | 10,000 | 725 µs | 0 | 0 |
| component_dense_iteration_name_10k | 10,000 | 299 µs | 0 | 0 |
| component_remove_name_5k | 5,000 | 331 µs | 0 | 0 |
| transform_mutation_10k | 10,000 | 1,141 µs | 0 | 0 |
| transform_update_roots_all_dirty_10k | 10,000 | 12,214 µs | 0 | 0 |
| transform_update_roots_1pct_dirty_10k | 10,000 | 1,504 µs | 0 | 0 |
| transform_update_wide_10k | 10,000 | 17,347 µs | 0 | 0 |
| transform_update_deep_2048 | 2,048 | 3,503 µs | 0 | 0 |
| render_extraction_10k | 10,000 | 20,763 µs | 0 | 0 |

Render extraction frame arena:

- used: 720,000 bytes;
- capacity: 724,096 bytes.

Important pass/fail properties:

- component remove performs no allocator calls;
- component lookup/iteration performs no allocator calls;
- transform update workloads perform no allocator calls;
- render extraction performs zero persistent allocator calls/bytes;
- stress suite is leak-free.

The exact timings are not CI thresholds because shared runner performance is not stable enough to be a shipping budget.

---

## 13. Windows CI contract

Workflow:

`.github/workflows/windows-ci.yml`

The final Phase 15 workflow validates:

1. checkout;
2. MSBuild setup;
3. NocturneHost Debug x64 build;
4. aggregate Phase 15 tests;
5. NocturneEditor Debug x64 regression build;
6. direct Development x64 build of NocturneEngine;
7. direct Development x64 build of NocturneHost;
8. direct Development x64 build of NocturneEditor;
9. Debug x64 `Nocturne.slnx` solution build.

CI uses a v143 toolset override on GitHub-hosted Windows runners while local project files can target the installed project toolset.

---

## 14. CI milestones

Important successful milestone runs during implementation included:

- `35287618949` — dense generic ComponentStorage foundation
- `35287937949` — component metadata registry
- `35288282341` — transform hierarchy
- `35288558951` — renderable component
- `35288855752` — camera component
- `35289158697` — name component
- `35289813447` — World migration
- `35290159493` — editor [[nodiscard]] failure handling
- `35290576797` — editor runtime-component single source of truth
- `35290928834` — stress baseline/dead-model cleanup
- `35290999381` — World component query API
- `35291693747` — direct Host root-independence validation
- `35292726512` — final comprehensive Phase 15 validation

---

## 15. CI-found build defects and fixes

Phase 15 completion intentionally expanded CI instead of relying only on the previously green path.

### Direct project include-root defect

A local direct Host rebuild failed with missing Engine headers because `NocturneHost.vcxproj` used `$(SolutionDir)` even when no solution supplied that property.

Fixed by:

`254f83b8137ce50dbbd10d5e6e9d2c33dd6571b6` — `build: make NocturneHost project root-independent`

The project derives repository root from `$(MSBuildProjectDirectory)`.

NocturneEngine received the same root-independent treatment during final build hardening.

### Development configuration validation

The final CI expansion made Engine/Host Development x64 direct builds explicit and supplied the required include/output/configuration settings.

Commit:

`a1e47771b6e7b890cf2158a9ccf7df1886fc7da5`

### Solution PDB conflict discovered by CI

Run `35292370397` passed:

- Debug Host;
- Phase 15 tests;
- Debug Editor;
- Development direct projects;

but failed the newly-added solution-build step because multiple logically distinct ProjectReference instances of `NocturneEngine` attempted to compile into the same compiler PDB.

This failure was not waived.

Fixed by:

`5b890be76f4b4444c29f7515a86791bad4b3b2c1` — `build: deduplicate engine project references`

Host and Editor no longer inject distinct SolutionDir metadata into the Engine project reference.

Final solution validation: **PASS** in run `35292726512`.

---

## 16. Compiler diagnostics

Final comprehensive CI:

- project C/C++ compile errors: **0**
- project C/C++ compiler warnings: **0**

Any GitHub-hosted runner/action deprecation messages are infrastructure messages and are not Nocturne C++ compiler diagnostics.

---

## 17. Manual Phase 14 editor regression

Performed on the local Windows desktop/GPU path after rebuilding the Phase 15 branch.

Result reported by the user: **PASS**.

Validated:

- EditorShellV3 launch;
- child-HWND DX12 viewport;
- validation scene rendering;
- grid/sky;
- camera controls;
- object picking;
- hierarchy selection synchronization;
- runtime names;
- Move gizmo;
- Rotate gizmo;
- Scale gizmo;
- selection bounds;
- viewport resize;
- camera after resize;
- absence of recurring stutter;
- no observed shader recompilation-every-frame regression.

### Accepted limitation

The transform gizmo currently works in **Local Axis** space.

This is accepted for Phase 15.

**Design choice (not directly from the book):** Local/Global coordinate-space authoring UX is a Phase 16 editor scene-editing concern and is carried explicitly in the Phase 16 handoff.

---

## 18. Local validation commands

Debug Host rebuild:

```powershell
& $msbuild `
    ".\Ide\VS2026\NocturneHost\NocturneHost.vcxproj" `
    /t:Rebuild `
    /p:Configuration=Debug `
    /p:Platform=x64
```

Phase 15 tests:

```powershell
& ".\Build\bin\Debug\NocturneHost.exe" --phase15-tests
```

Debug Editor rebuild:

```powershell
& $msbuild `
    ".\Ide\VS2026\NocturneEditor\NocturneEditor.vcxproj" `
    /t:Rebuild `
    /p:Configuration=Debug `
    /p:Platform=x64
```

Editor launch:

```powershell
& ".\Build\bin\Debug\NocturneEditor.exe"
```

---

## 19. Validation conclusion

All applicable Phase 15 automated and manual gates pass.

The Phase 15 entity/component runtime is accepted as the foundation for Phase 16 and Phase 17.

Future changes should compare regressions against this report rather than relying on the original pre-implementation checklist alone.
