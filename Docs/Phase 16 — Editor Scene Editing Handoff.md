# Handoff — Phase 16: Editor Scene Editing

> **Phase 15 — Entity / Component System: COMPLETE**
>
> Phase 15 completion authority:
>
> - `Docs/Phase 15 — Entity Component System Architecture.md`
> - `Docs/Phase 15 — Implementation Report.md`
> - `Docs/Phase 15 — Test and CI Validation Report.md`
> - `Docs/Phase 15 — Completion Report.md`
> - `Docs/Production Engineering Standard.md`

## 1. Starting point

Phase 15 established the production runtime object foundation.

The canonical runtime model is now:

- transient generational `noc::EntityHandle`;
- `EntityRegistry`;
- dense/sparse `ComponentStorage<T>`;
- explicit `ComponentTypeMetadata`;
- `TransformComponent`;
- `RenderableComponent`;
- `CameraComponent`;
- `NameComponent`;
- `World` as owner/orchestrator;
- component-based render extraction;
- editor/runtime single source of truth.

There is no second authoritative editor world and no competing legacy ECS/world model.

The renderer consumes extracted `RenderQueue` data and does not own mutable component state.

## 2. Mandatory reading at the start of Phase 16

Study all prior phase Markdown files in full.

Give special attention to:

1. `Docs/Production Engineering Standard.md`
2. `Docs/Phase 15 — Entity Component System Architecture.md`
3. `Docs/Phase 15 — Implementation Report.md`
4. `Docs/Phase 15 — Test and CI Validation Report.md`
5. `Docs/Phase 15 — Completion Report.md`
6. `Docs/Phase 14 — Completion Report.md`
7. `Docs/Phase 14 — Editor Rendering Viewport Architecture Guide.md`
8. `Docs/nocturne_engine_architecture.md`

Then inspect the real current code before designing Phase 16.

## 3. Architecture contracts that Phase 16 must preserve

- Active editor shell remains `EditorShellV3`.
- Do not revive historical `EditorShell` / `EditorControls`.
- One runtime `World` remains authoritative.
- Editor operations must mutate/query that same World.
- `EntityHandle` remains transient runtime identity.
- Do not serialize runtime entity index/generation.
- Persistent entity identity remains Phase 17.
- No STL types in public engine headers unless the architecture contract is deliberately revised.
- Renderer continues to consume extracted data.
- Structural ECS mutation remains main-thread-owned in the current contract.
- One main loop remains authoritative.
- Preserve the validated Phase 13/14 editor visual baseline unless Phase 16 explicitly owns a UI change.

## 4. Phase 16 owned scope

Phase 16 owns real editor scene-authoring workflows over the Phase 15 runtime model.

The roadmap contract includes:

- create entities;
- delete entities;
- duplicate entities;
- rename entities;
- reparent/unparent entities;
- add/remove supported components;
- edit component properties;
- real Scene Hierarchy backed by World entities;
- Inspector/component authoring;
- robust viewport ↔ hierarchy ↔ inspector selection synchronization;
- transactional editing semantics;
- undo/redo integration for operations owned by Phase 16;
- prefab prototype only to the extent explicitly scoped by the Phase 16 architecture;
- editor diagnostics for invalid/failed authoring operations.

Phase 16 must replace the fixed validation-scene authoring bridge with real scene editing without creating a second scene model.

## 5. Explicitly deferred

Do not pull these into Phase 16 unless an interface seam is strictly necessary:

- scene-file persistence;
- persistent entity IDs;
- save/load;
- serialized reference fixups;
- schema migrations;
- physics;
- animation;
- audio;
- scripting/gameplay runtime;
- generic ECS scheduler;
- PIE.

Those remain assigned to later roadmap phases, especially Phase 17 for persistence.

## 6. Accepted Phase 15 editor limitation carried into Phase 16

The validated Phase 15 gizmo currently operates in **Local Axis** space.

The user accepted this for Phase 15 completion.

**Design choice (not directly from the book):** Phase 16 should own the authoring UX for transform coordinate space, including a clear Local / Global (World) mode if the Phase 16 design confirms it belongs in the scene-editing tool contract.

Do not reinterpret the Phase 15 completion as proof that a Global Axis mode already exists.

## 7. Phase 15 validation scene at handoff

The current validation runtime contains:

- Cube_A — Name + Transform + Renderable
- Cube_B — Name + Transform + Renderable
- Cube_C — Name + Transform + Renderable
- Ground — Name + Transform + Renderable
- Main Camera — Name + Transform + Camera

Procedural grid/sky/debug selection remain validation/render/editor scaffolding.

Phase 16 may replace the fixed object list with real authoring-created entities, but the Phase 14 viewport behavior must remain regression-covered.

## 8. Required design work before Phase 16 coding

Before implementation, explicitly define:

- editor command/transaction model;
- create/delete ownership semantics;
- selection identity and stale-selection behavior;
- duplicate semantics;
- hierarchy reparent semantics;
- component add/remove rules;
- inspector edit commit semantics;
- undo/redo boundaries;
- component metadata consumption strategy;
- editor-only state vs runtime-authoritative state;
- error/diagnostic behavior;
- validation tests;
- Phase 14/15 regression matrix;
- performance implications for hierarchy/inspector enumeration.

Every Nocturne-specific architectural decision not directly supported by the books must be labeled:

**Design choice (not directly from the book)**

## 9. Branch recommendation

**Design choice (not directly from the book):**

Use a dedicated branch:

`phase-16-editor-scene-editing`

Create it from the final Phase 15 integration point after Phase 15 is merged according to the project's normal branch workflow.

## 10. What to say in the next chat

Use:

> **Phase 15 is COMPLETE. Begin Phase 16 — Editor Scene Editing. Study all previous .md phase files first, especially the Phase 15 Architecture, Implementation Report, Test and CI Validation Report, Completion Report, Production Engineering Standard, and Phase 14 editor documentation. Audit the current editor/runtime integration before proposing the Phase 16 architecture. Preserve EditorShellV3 and the Phase 15 World/ECS single-source-of-truth contract.**
