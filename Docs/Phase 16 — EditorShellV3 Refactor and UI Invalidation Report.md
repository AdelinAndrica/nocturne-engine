# Phase 16 — EditorShellV3 Refactor and UI Invalidation Report

> **Branch:** `phase-16-editor-shell-v3-refactor`
>
> **Base:** `phase-16-editor-scene-editing`
>
> **Scope:** editor invalidation fixes, Inspector paint/lifetime fix, and decomposition of the `EditorShellV3` monolith.
>
> **Phase 16 status:** this work does not bypass the existing Phase 16 completion-hardening, manual-regression, or soak gates.

---

## 1. Problems addressed

### 1.1 Undo/Redo after Transform could leave viewport visuals stale

Observed behavior:

- edit a Transform;
- Undo or Redo changed the authoritative runtime component;
- the Inspector could refresh;
- the selected object's viewport debug representation could remain at the previous transform until the entity was deselected/reselected.

Root cause:

`EditorViewportController::TickFrame()` treats `EditorSession::StateVersion()` as its editor-state invalidation boundary. The old code used `SetSceneDirty()` after authoring commands. Once the scene was already dirty, a second `SetSceneDirty(true)` did not increment `StateVersion`.

This created an invalid condition:

```text
World authored state changed
but
EditorSession observer version did not change
```

The viewport therefore had no reliable notification that cached/editor-derived visualization had to be refreshed.

### Fix

`EditorSession` now exposes:

```cpp
void NotifyAuthoredMutation() noexcept;
```

A successful authored mutation now:

1. keeps the scene dirty;
2. unconditionally increments the session state version.

Undo, Redo, shell authoring commands, and successful gizmo commits use this semantic invalidation operation.

A headless Phase 16 regression verifies that repeated authored mutations increment `StateVersion` even when `SceneDirty() == true`.

**Design choice (not directly from the book):** `EditorSession::StateVersion` is the Nocturne-specific coarse invalidation signal for editor observers. The books support separating authoritative world state from editor/view state, but do not prescribe this exact version-counter API.

---

## 2. Add Component paint bug

Observed behavior:

- `+ Add Component` sometimes appeared to disappear;
- moving the mouse over its location made it visible again;
- the button remained interactive, indicating a paint/z-order problem rather than state loss.

### Root cause

The Inspector body is owner-drawn. The Inspector's dynamic controls were created as siblings of that body under the top-level editor HWND.

The old hierarchy was effectively:

```text
Editor HWND
├── Inspector body (owner drawn)
├── Add Component button
├── reflected EDIT control
├── reflected resource button
├── reflected bool button
└── ...
```

The body could repaint its panel surface over sibling controls. Hovering a button invalidated that button, causing it to repaint and visually reappear.

### Fix

The Inspector is now a real Win32 child-control container:

```text
Editor HWND
└── Inspector body
    ├── Add Component button
    ├── Remove buttons
    ├── Resource picker buttons
    ├── Bool buttons
    ├── Enum buttons
    └── reflected EDIT controls
```

Changes:

- panel body uses `WS_CLIPCHILDREN` and `WS_CLIPSIBLINGS`;
- all Inspector controls are parented to `inspector_.body`;
- Inspector control layout uses body-local coordinates;
- `InspectorBodySubclassProc_` forwards child `WM_COMMAND` notifications to the top-level shell dispatch;
- `WM_CTLCOLOREDIT` is forwarded so existing theme/color behavior remains authoritative;
- child paint regions are clipped from the owner-drawn panel background.

This removes the body-versus-sibling overpaint race rather than relying on hover invalidation as a visual repair.

**Design choice (not directly from the book):** the Win32 child-container implementation and message forwarding are platform/UI implementation details specific to Nocturne Editor.

---

## 3. EditorShellV3 decomposition

Before this work, `EditorShellV3.cpp` was approximately 6.5k lines and simultaneously owned:

- shell lifecycle;
- custom Win32 controls;
- control painting;
- hierarchy authoring;
- Inspector reflection UI;
- property editors;
- content-browser composition;
- panel layout;
- accelerators;
- toolbar/menu command dispatch;
- top-level Win32 message dispatch.

This made unrelated editor systems share one translation unit and allowed implicit dependencies to accumulate.

### New module boundaries

#### `EditorShellV3.cpp`

Responsibility:

- shell initialization/shutdown;
- top-level `IWindowMessageSink`;
- top-level Win32 dispatch;
- small common conversion/path utilities.

It is now the orchestration seam rather than the implementation home for every editor subsystem.

#### `EditorShellV3Controls.h/.cpp`

Responsibility:

- Nocturne V3 Button/Header/Tree/Table/Scroll custom controls;
- shared drawing primitives;
- control class registration;
- tree/table/scroll helper operations;
- internal custom Win32 message definitions.

This is editor-only presentation mechanism. It does not own World/editor authoring state.

#### `EditorShellV3Inspector.cpp`

Responsibility:

- reflected Inspector model presentation;
- Inspector dynamic-control lifecycle;
- reflected property editing;
- resource, bool, and enum editors;
- add/remove component UI;
- Inspector scrolling/layout;
- Inspector owner-draw rendering and command dispatch.

Authority remains:

```text
ReflectionRegistry
+ World semantic setters
+ EditorCommandHistory
```

The Inspector remains a projection/editor of authoritative runtime state, not a second object model.

#### `EditorShellV3Hierarchy.cpp`

Responsibility:

- hierarchy projection;
- scene selection synchronization;
- Create / Delete / Duplicate / Reparent;
- context menu authoring;
- rename interaction.

Mutations continue to go through the existing command/history contracts.

#### `EditorShellV3Layout.cpp`

Responsibility:

- top-level chrome;
- toolbar construction;
- panel construction;
- Content Browser population;
- panel layout;
- console append presentation.

#### `EditorShellV3Commands.cpp`

Responsibility:

- accelerator filtering;
- toolbar/menu command routing;
- active editor tool changes;
- Undo / Redo routing;
- popup dispatch;
- status updates.

### Source-of-truth rule

The split does **not** introduce new scene/editor authority.

```text
World / ECS              = authored runtime state authority
ReflectionRegistry       = schema authority
EditorSession            = editor-session state authority
EditorCommandHistory     = undo/redo mutation history
EditorShellV3 modules    = presentation + orchestration
```

---

## 4. Book grounding

### Jason Gregory — Game Engine Architecture, 3rd Edition

Relevant concepts used by this refactor:

- game-world/editor separation;
- runtime object model versus tooling;
- subsystem boundaries;
- avoiding inappropriate coupling between systems.

Applied here:

- editor presentation remains separate from the runtime World;
- the refactor separates UI mechanisms from authoring orchestration;
- runtime state is not duplicated into an editor-owned scene model.

### Bob Nystrom — Game Programming Patterns

Relevant concept:

- Command pattern.

Applied here:

- Create/Delete/Duplicate/Reparent/property edits remain command-backed;
- Undo/Redo continues to mutate the authoritative runtime model through existing commands;
- the refactor does not add direct UI mutation shortcuts.

### Nocturne Production Engineering Standard

The work explicitly preserves:

- one source of truth;
- explicit ownership;
- command/history authoring boundaries;
- deterministic invalidation behavior;
- negative-path validation;
- small implementation seams instead of parallel scaffolding;
- no persistence claims before Phase 17.

**Design choice (not directly from the book):** the exact split into Controls / Inspector / Hierarchy / Layout / Commands modules is a Nocturne maintainability boundary chosen for the current Win32 editor implementation.

---

## 5. Regression coverage

Automated coverage includes the new editor-session invalidation invariant:

```text
clean session
-> first authored mutation
-> scene dirty + StateVersion increases
-> second authored mutation while already dirty
-> StateVersion increases again
```

Existing Phase 15/16 aggregate tests remain required.

The Windows CI remains responsible for:

- Debug NocturneHost build;
- Phase 15 foundation tests;
- Phase 16 aggregate tests;
- Debug NocturneEditor build;
- Development x64 project builds;
- Debug x64 solution regression.

---

## 6. Manual verification required

The Win32 painting fix requires a visual/manual regression in addition to compilation.

### Undo/Redo viewport

- Select Cube, Sphere, or Cylinder.
- Move it with gizmo.
- Undo without changing selection.
- Geometry, outline, and gizmo must all move back immediately.
- Redo.
- Geometry, outline, and gizmo must all move forward immediately.
- Repeat while scene is already dirty.

### Add Component

- Select multiple authored entities in sequence.
- Scroll Inspector repeatedly.
- Resize the editor and Inspector.
- Add/remove components repeatedly.
- Move the pointer away from `+ Add Component`.
- The button must remain visibly painted without requiring hover.

### Inspector controls

Verify:

- resource picker;
- bool toggle;
- enum picker;
- numeric edit Enter commit;
- numeric edit focus-loss commit;
- Escape cancel;
- component Remove;
- Add Component popup;
- mouse-wheel scrolling.

### Refactor regression

Verify existing Phase 13/14 visual baseline:

- toolbar;
- menu bar;
- hierarchy;
- viewport;
- content browser;
- console;
- status bar;
- selection;
- gizmos;
- editor camera.

---

## 7. Deferred scope

This refactor does not introduce:

- scene serialization;
- persistent entity IDs;
- prefab architecture;
- a new UI framework;
- retained-mode editor scene ownership;
- changes to the runtime game loop.

Those remain outside this maintenance branch unless Phase 16 scope is explicitly revised.
